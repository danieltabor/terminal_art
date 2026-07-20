#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#define DELAY_MS      100
#define CYCLE_HISTORY 7
#define CYCLE_MAX     500

//#define CELL_CHARACTER "#"
#define CELL_CHARACTER  "\xe2\x96\x88"

//Dark shades
#define BASE_COLOR   30
//Bright shades
//#define BASE_COLOR   90

typedef struct {
	size_t width;
	size_t height;
	uint8_t updated;
} termsize_t;

static int termsize_update(termsize_t *term) {
	struct winsize ws;
	if( !term ) {
		return -1;
	}
	
	//Use ioctl/TIOCGWINSZ to get terminal size
	if( ioctl(STDIN_FILENO,TIOCGWINSZ,&ws) ) {
		return -2;
	}
	
	if( ws.ws_col != term->width  || ws.ws_row != term->height ) {
		term->width = ws.ws_col;
		term->height = ws.ws_row;
		term->updated = 1;
	}
	else {
		term->updated = 0;
	}
	return 0;
}

static int termsize_init(termsize_t* term) {
	if( !term ) {
		return -1;
	}
	term->width = 0;
	term->height = 0;
	if( termsize_update(term) ) {
		return -2;
	}
	return 0;
}

static int user_reset(void) {
	struct timeval timeout;
	fd_set readfds;
	char input;
	
	memset(&timeout,0,sizeof(timeout));
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO,&readfds);
	while( select(STDIN_FILENO+1,&readfds,0,0,&timeout) ) {
		if( read(STDIN_FILENO,&input,1) != 1 ) {
			exit(-1);
		}
		if( input == '\n' ) { return 1; }
	}
	return 0;
}

static void usage(char* cmd) {
	char* cmd_filename = cmd+strlen(cmd);
	while( cmd_filename > cmd ) {
		if( *(cmd_filename-1) != '/' ) {
			cmd_filename--;
		} else {
			break;
		}
	}
	printf("Usage\n");
	printf("%s [-h] [-d1 | -d2] [-rgb] [-ms #] [-hist #] [-max #]\n",cmd_filename);
	printf("\n");
	printf("Arguments:\n");
	printf("  -d1 : Execute 1-D Life\n");
	printf("  -d2 : Execute 2-D Life\n");
	printf("  -rgb: Execute three independent planes of Life\n");
	printf("  -ms : Specify millisecond delay between frames\n");
	printf(" -hist: Specify the length of history for cycle detection\n");
	printf("  -max: Specify absolute maximum cycles before reset\n");
	printf("\n");
	exit(0);
}

int main(int argc, char** argv) {
	size_t i,j;
	size_t x,y;
	uint8_t rgb;
	size_t totals[3];
	termsize_t term;
	size_t cell_count;
	uint8_t *hist_cells = 0;
	uint8_t *next_cells = 0;
	int gen_diff;
	int cell_diff;
	int dimensions = 0;
	int do_rgb = 0;
	int delay_ms = -1;
	int cycle_history = -1;
	int cycle_max = -1;
	int cycle_count = 0;
	
	i = 1;
	while( i < argc ) {
		if( !strcmp(argv[i],"-h") ) {
			usage(argv[0]);
		}
		else if( !strcmp(argv[i],"-d1") ) {
			if( dimensions ) { usage(argv[0]); }
			dimensions = 1;
		}
		else if( !strcmp(argv[i],"-d2") ) {
			if( dimensions ) { usage(argv[0]); }
			dimensions = 2;
		}
		else if( !strcmp(argv[i],"-rgb") ) {
			if( do_rgb ) { usage(argv[0]); }
			do_rgb = 1;
		}
		else if( !strcmp(argv[i],"-ms") ) {
			if( ++i >= argc || delay_ms >= 0 ) {
				usage(argv[0]);
			}
			errno = 0;
			delay_ms = strtoull(argv[i],0,10);
			if( errno || delay_ms < 0 ) {
				usage(argv[0]);
			}
		}
		else if( !strcmp(argv[i],"-hist") ) {
			if( ++i >= argc || cycle_history >= 0 ) {
				usage(argv[0]);
			}
			errno = 0;
			cycle_history = strtoull(argv[i],0,10);
			if( errno || cycle_history < 1 ) {
				usage(argv[0]);
			}
		}
		else if( !strcmp(argv[i],"-max") ) {
			if( ++i >= argc || cycle_max >= 0 ) {
				usage(argv[0]);
			}
			errno = 0;
			cycle_max = strtoull(argv[i],0,10);
			if( errno || cycle_max < 0 ) {
				usage(argv[0]);
			}
		}
		else {
			usage(argv[0]);
		}
		++i;
	}
	
	if( !dimensions ) {
		dimensions = 2;
	}
	if( delay_ms < 0 ) {
		delay_ms = DELAY_MS;
	}
	if( cycle_history < 0 ) {
		cycle_history = CYCLE_HISTORY;
	}
	if( cycle_max < 0 ) {
		cycle_max = CYCLE_MAX;
	}
	
	termsize_init(&term);
	cell_count = 0;
	cycle_count = 0;
	for(;;) {
		if( term.updated || !gen_diff || user_reset() || cycle_count >= cycle_max ) {
			if( dimensions == 1 ) {
				cell_count = term.width;
			}
			else {  //dimensions == 2
				cell_count = term.width*term.height;
			}
			
			if( cell_count ) {
				hist_cells = (uint8_t*)realloc(hist_cells,cell_count*cycle_history);
				next_cells = (uint8_t*)realloc(next_cells,cell_count);
			}
			memset(hist_cells,0,cell_count*cycle_history);
			srandom((unsigned int)time(0));
			for( i=0; i<cell_count; i++ ) {
				next_cells[i] = (random() & (do_rgb?7:1));
			}
			cycle_count = 0;
			
		}
		for( i=0; i<cell_count; i++ ) {
			for( j=1; j<cycle_history; j++ ) {
				hist_cells[j*cell_count+i] = hist_cells[(j-1)*cell_count+i];
			}
			hist_cells[i] = next_cells[i];
			next_cells[i] = 0;
		}
		
		if( dimensions == 1 ) {
			printf("\n");
		}
		else { // dimensions == 2 
			printf("\x1b[H");
		}
		gen_diff = 0;
		for( i=0; i<cell_count; i++ ) {
			if( dimensions == 1 && !do_rgb ) {
				totals[0] = 0;
				if( i >= 2 ) { totals[0] += hist_cells[i-2]; }
				if( i >= 1 ) { totals[0] += hist_cells[i-1]; }
				if( i <= term.width-2 ) { totals[0] += hist_cells[i+1]; }
				if( i <= term.width-1 ) { totals[0] += hist_cells[i+2]; }
				if( !hist_cells[i] && (totals[0] == 2 || totals[0] == 3) ) { next_cells[i] = 1; }
				if( hist_cells[i] && (totals[0] == 2 || totals[0] == 4) ) { next_cells[i] = 1; }

				for( j=0; j<cycle_history; j++ ) {
					if( next_cells[i] == hist_cells[(j*cell_count)+i] ) { break; }
				}
				if( j == cycle_history ) { gen_diff = 1; }

				if( next_cells[i] ) { printf(CELL_CHARACTER); }
				else{ printf(" "); }
			}
			else if( dimensions == 1 && do_rgb ) {
				totals[0] = 0;
				totals[1] = 0;
				totals[2] = 0;
				if( i >= 2 ) { 
					rgb = hist_cells[i-2]; 
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
				}
				if( i >= 1 ) { 
					rgb = hist_cells[i-1]; 
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
				}
				if( i <= term.width-2 ) { 
					rgb = hist_cells[i+1];
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
				}
				if( i <= term.width-1 ) { 
					rgb = hist_cells[i+2];
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
				}
				rgb = hist_cells[i];
				if( rgb&1 && (totals[0]==2 || totals[0] == 4) ) { next_cells[i] |= 1; }
				if( !(rgb&1) && (totals[0]==2 || totals[0] == 3) ) { next_cells[i] |= 1; }
				if( rgb&2 && (totals[1]==2 || totals[1] == 4) ) { next_cells[i] |= 2; }
				if( !(rgb&2) && (totals[0]==2 || totals[0] == 3)  ) { next_cells[i] |= 2; }
				if( rgb&4 && (totals[2]==2 || totals[2] == 4) ) { next_cells[i] |= 4; }
				if( !(rgb&4) && (totals[0]==2 || totals[0] == 3)  ) { next_cells[i] |= 4; }

				for( j=0; j<cycle_history; j++ ) {
					if( next_cells[i] == hist_cells[(j*cell_count)+i] ) { break; }
				}
				if( j == cycle_history ) { gen_diff = 1; }

				printf("\x1b[%d;%dm" CELL_CHARACTER,BASE_COLOR+next_cells[i],BASE_COLOR+10);
			}
			else if( dimensions == 2 && !do_rgb ) {
				y = i/term.width;
				x = i%term.width;
				totals[0]= 0;
				if( y > 0 ) {
					if( x > 0 ) { totals[0]+= hist_cells[i-term.width-1]; }
					totals[0]+= hist_cells[i-term.width];
					if( x < (term.width-1) ) { totals[0]+= hist_cells[i-term.width+1]; }
				}
				if( x > 0 ) { totals[0]+= hist_cells[i-1]; }
				if( x < (term.width-1) ) { totals[0]+= hist_cells[i+1]; }
				if( y < (term.height-1) ) {
					if( x > 0 ) { totals[0]+= hist_cells[i+term.width-1]; }
					totals[0]+= hist_cells[i+term.width];
					if( x < (term.width-1) ) { totals[0]+= hist_cells[i+term.width+1]; }
				}
				if( hist_cells[i] && (totals[0]== 2 || totals[0]== 3) ) { next_cells[i] = 1; }
				if( !hist_cells[i] && totals[0]== 3 ) { next_cells[i] = 1; }

				for( j=0; j<cycle_history; j++ ) {
					if( next_cells[i] == hist_cells[(j*cell_count)+i] ) { break; }
				}
				if( j == cycle_history ) { gen_diff = 1; }

				if( next_cells[i] ) { printf(CELL_CHARACTER); }
				else{ printf(" "); }
			}
			else if( dimensions == 2 && do_rgb ) {
				y = i/term.width;
				x = i%term.width;
				totals[0] = 0;
				totals[1] = 0;
				totals[2] = 0;
				if( y > 0 ) {
					if( x > 0 ) { 
						rgb = hist_cells[i-term.width-1];
						totals[0] += (rgb&1)?1:0;
						totals[1] += (rgb&2)?1:0;
						totals[2] += (rgb&4)?1:0;
					}
					rgb = hist_cells[i-term.width];
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
					if( x < (term.width-1) ) { 
						rgb = hist_cells[i-term.width+1];
						totals[0] += (rgb&1)?1:0;
						totals[1] += (rgb&2)?1:0;
						totals[2] += (rgb&4)?1:0;
					}
				}
				if( x > 0 ) { 
					rgb = hist_cells[i-1];
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
				}
				if( x < (term.width-1) ) { 
					rgb = hist_cells[i+1];
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
				}
				if( y < (term.height-1) ) {
					if( x > 0 ) { 
						rgb = hist_cells[i+term.width-1];
						totals[0] += (rgb&1)?1:0;
						totals[1] += (rgb&2)?1:0;
						totals[2] += (rgb&4)?1:0;
					}
					rgb =  hist_cells[i+term.width];
					totals[0] += (rgb&1)?1:0;
					totals[1] += (rgb&2)?1:0;
					totals[2] += (rgb&4)?1:0;
					if( x < (term.width-1) ) { 
						rgb = hist_cells[i+term.width+1];
						totals[0] += (rgb&1)?1:0;
						totals[1] += (rgb&2)?1:0;
						totals[2] += (rgb&4)?1:0;
					}
				}
				rgb = hist_cells[i];
				if( rgb&1 && (totals[0]==2 || totals[0] == 3) ) { next_cells[i] |= 1; }
				if( !(rgb&1) && totals[0] == 3 ) { next_cells[i] |= 1; }
				if( rgb&2 && (totals[1]==2 || totals[1] == 3) ) { next_cells[i] |= 2; }
				if( !(rgb&2) && totals[1] == 3 ) { next_cells[i] |= 2; }
				if( rgb&4 && (totals[2]==2 || totals[2] == 3) ) { next_cells[i] |= 4; }
				if( !(rgb&4) && totals[2] == 3 ) { next_cells[i] |= 4; }

				for( j=0; j<cycle_history; j++ ) {
					if( next_cells[i] == hist_cells[(j*cell_count)+i] ) { break; }
				}
				if( j == cycle_history ) { gen_diff = 1; }

				printf("\x1b[%d;%dm" CELL_CHARACTER,BASE_COLOR+next_cells[i],BASE_COLOR+10);
			}
		}
		if( do_rgb ) {
			printf("\x1b[0m");
		}
		usleep((useconds_t)delay_ms*1000);
		termsize_update(&term);
		++cycle_count;
	}
}
