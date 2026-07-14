#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#define CYCLE_HISTORY 7
#define DELAY_US      100e3

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

int main() {
	size_t i,j;
	int total;
	termsize_t term;
	uint8_t *hist_cells[CYCLE_HISTORY];
	uint8_t *next_cells = 0;
	int gen_diff;
	int cell_diff;
	
	memset(hist_cells,0,sizeof(hist_cells));
	termsize_init(&term);
	for(;;) {
		if( term.updated || !gen_diff || user_reset() ) {
			for( j=0; j<CYCLE_HISTORY; j++ ) {
				hist_cells[j] = (uint8_t*)realloc(hist_cells[j],term.width);
			}
			next_cells = (uint8_t*)realloc(next_cells,term.width);
			srandom((unsigned int)time(0));
			for( i=0; i<term.width; i++ ) {
				for( j=0; j<CYCLE_HISTORY; j++ ) {
					hist_cells[j][i] = 0;
				}
				next_cells[i] = (random()&1);
			}
		}
		for( i=0; i<term.width; i++ ) {
			for( j=1; j<CYCLE_HISTORY; j++ ) {
				hist_cells[j][i] = hist_cells[j-1][i];
			}
			hist_cells[0][i] = next_cells[i];
			next_cells[i] = 0;
		}
		gen_diff = 0;
		for( i=0; i<term.width; i++ ) {
			total = 0;
			if( i >= 2 ) { total += hist_cells[0][i-2]; }
			if( i >= 1 ) { total += hist_cells[0][i-1]; }
			if( i <= term.width-2 ) { total += hist_cells[0][i+1]; }
			if( i <= term.width-1 ) { total += hist_cells[0][i+2]; }
			if( !hist_cells[0][i] && (total == 2 || total == 3) ) { next_cells[i] = 1; }
			if( hist_cells[0][i] && (total == 2 || total == 4) ) { next_cells[i] = 1; }
			
			for( j=0; j<CYCLE_HISTORY; j++ ) {
				if( next_cells[i] == hist_cells[j][i] ) { break; }
			}
			if( j == CYCLE_HISTORY ) { gen_diff = 1; }
			
			if( next_cells[i] ) { printf("\xe2\x96\x88"); }
			else{ printf(" "); }
		}
		printf("\n");
		usleep(DELAY_US);
		termsize_update(&term);
	}
}
