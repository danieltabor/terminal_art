/*
 * Copyright (c) 2023, Daniel Tabor
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>

#define FRAME_RATE (10)
#define FRAME_DELAY (1000000/FRAME_RATE)
#define GRAVITY  (9.8/FRAME_RATE)
#define WATER_TENSION   0.025
#define WATER_DAMPENING 0.025
#define WATER_SPREAD    0.25
#define CLOUD_SPEED     (10.0 / FRAME_RATE)
#define DRIP_RATE       10

char* drip_char = "\u25CF";
char* fish_chars[2] = {
	"\u25B6\u25CF",
	"\u25CF\u25C0",
};
char* cloud_char[3] = {
	" @@@ ",
	"@@@@@",
	" @@@ ",
};
char* bird_chars[5] = {
	"\U0001FB7B\u25C6\U0001FB7B",
	"\U0001FB7A\u25C6\U0001FB7A",
	"\U0001FB79\u25C6\U0001FB79",
	"\U0001FB78\u25C6\U0001FB78",
	"\U0001FB77\u25C6\U0001FB77",
};
char* water_chars[8] = {
	"\u2581",
	"\u2582",
	"\u2583",
	"\u2584",
	"\u2585",
	"\u2586",
	"\u2587",
	"\u2588",
};

uint8_t fgcolors[] = {30, 31, 32, 33, 34, 35, 36, 37,  90,  91,  92,  93,  94,  95,  96,  97};
uint8_t bgcolors[] = {40, 41, 42, 43, 44, 45, 46, 47, 100, 101, 102, 103, 104, 105, 106, 107};

typedef struct {
	size_t width;
	size_t height;
	uint8_t updated;
} termsize_t;

typedef struct {
	uint8_t active;
	size_t x;
	size_t y;
	float speed;
} drip_t;

typedef struct {
	drip_t *drips;
	size_t size;
	termsize_t *term;
} drips_t;

typedef struct {
	termsize_t *term;
	float pos;
	float speed;
	size_t drop_delay;
	size_t drop_count;
} cloud_t;

typedef struct {
	float height;
	float speed;
	float ldelta;
	float rdelta;
} water_column_t;

typedef struct {
	termsize_t *term;
	water_column_t *cols;
	float  target_height;
	size_t island_y;
} water_t;


int termsize_update(termsize_t *term) {
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


int termsize_init(termsize_t* term) {
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


int water_update(water_t *water) {
	size_t i;
	size_t j;
	water_column_t *tmp;
	
	if( ! water ) {
		return -1;
	}
	
	if( water->term->updated ) {
		tmp = realloc(water->cols,sizeof(water_column_t)*water->term->width);
		if( !tmp ) {
			return -2;
		}
		water->cols = tmp;
		water->target_height = 8;//water->term->height*8/4;
		for( i=0; i<water->term->width; i++ ) {
			water->cols[i].height = water->target_height;
			water->cols[i].speed  = 0.0;
		}
		water->island_y = (water->term->height*3/4)-1;
	}
	
	for( i=0; i<water->term->width; i++ ) {
		water->cols[i].speed = water->cols[i].speed + 
			( (WATER_TENSION * (water->target_height - water->cols[i].height)) - 
			  (water->cols[i].speed * WATER_DAMPENING) );
		water->cols[i].height  = water->cols[i].height + water->cols[i].speed;
	}
	
	for( j=0; j<8; j++ ) {
		for( i=0; i<water->term->width; i++ ) {
			if( i > 0 ) {
				water->cols[i].ldelta = WATER_SPREAD * (water->cols[i].height - water->cols[i-1].height);
				water->cols[i-1].speed = water->cols[i-1].speed + water->cols[i].ldelta;
			}
			if( i < water->term->width - 1 ) {
				water->cols[i].rdelta = WATER_SPREAD * (water->cols[i].height - water->cols[i+1].height);
				water->cols[i+1].speed = water->cols[i+1].speed + water->cols[i].rdelta;
			}
		}
		for( i=0; i<water->term->width; i++ ) {
			if( i > 0 ) {
				water->cols[i-1].height = water->cols[i-1].height + water->cols[i].ldelta;
			}
			if( i < water->term->width - 1 ) {
				water->cols[i+1].height = water->cols[i+1].height + water->cols[i].rdelta;
			}
		}
	}
	return 0;
}


int water_init(water_t *water, termsize_t *term) {
	if( ! water ) {
		return -1;
	}
	if( ! term ) {
		return -2;
	}
	water->cols = 0;
	water->term = term;
	return water_update(water);
}


#define CM_UNKNOWN  0
#define CM_CLOUD    1
#define CM_ISLAND   2
#define CM_WATER_FG 3
#define CM_WATER_BG 4

int render( water_t *water, drips_t *drips, cloud_t *cloud) {
	size_t y,x,i;
	float y_water_height;
	uint8_t move = 1;
	uint8_t color_mode;
	for( y=0; y<water->term->height; y++ ) {
		y_water_height = (water->term->height-y)*8;
		color_mode = CM_UNKNOWN;
		for( x=0; x<water->term->width; x++ ) {
			//Set background color for island
			if( y == water->island_y-5 &&
					(x==(water->term->width/2)-3 || x==(water->term->width/2)-1 || x==(water->term->width/2)+1 ) ) {
				printf("\x1b[%d;%dm",fgcolors[4],bgcolors[2]);
				color_mode = CM_ISLAND;
			}
			else if( y == water->island_y-4 &&
					(x>=(water->term->width/2)-2 && x<=(water->term->width/2) ) ) {
				printf("\x1b[%d;%dm",fgcolors[4],bgcolors[2]);
				color_mode = CM_ISLAND;
			}
			else if( y == water->island_y-3 &&
					(x==(water->term->width/2)-3 || x==(water->term->width/2)+1 ) ) {
				printf("\x1b[%d;%dm",fgcolors[4],bgcolors[2]);
				color_mode = CM_ISLAND;
			}
			else if( y == water->island_y-3 &&
					x==(water->term->width/2)-1 ){
				printf("\x1b[%d;%dm",fgcolors[4],bgcolors[3]);
				color_mode = CM_ISLAND;
			}
			else if( (y == water->island_y-2 || y == water->island_y-1) &&
					x==(water->term->width/2) ) {
				printf("\x1b[%d;%dm",fgcolors[4],bgcolors[3]);
				color_mode = CM_ISLAND;
			}
			else if( y >= water->island_y && 
					x >= (water->term->width/2) - (1+2*(y-water->island_y)) && 
					x <= (water->term->width/2) + (1+2*(y-water->island_y)) ) {
				printf("\x1b[%d;%dm",fgcolors[4],bgcolors[11]);
				color_mode = CM_ISLAND;
			} 
			else if( color_mode == CM_ISLAND ) {
				printf("\x1b[%d;%dm",fgcolors[0],bgcolors[0]);
				color_mode = CM_UNKNOWN;
			}
			
			//Render Drips
			for( i=0; i<drips->size; i++ ) {
				if( drips->drips[i].active ) {
					if( (y == ((drips->term->height*8)-drips->drips[i].y)/8) && (x == drips->drips[i].x) ) {
						if( color_mode != CM_WATER_FG ) {
							printf("\x1b[%dm",fgcolors[4]);
							color_mode = CM_WATER_FG;
						}
						if( move ) {
							printf("\x1b[%ld;%ldH",y+1, x+1);
						}
						printf("%s",drip_char);
						continue;
					}
				}
			}
			
			//Render Cloud
			if( y >= 0 && y <=2 && x == (int)(cloud->pos/8) ) {
				if( move ) {
					printf("\x1b[%ld;%ldH",y+1, x+1);
				}
				if( color_mode != CM_CLOUD ) {
					printf("\x1b[%d;%dm",fgcolors[15],bgcolors[0]);
					color_mode = CM_CLOUD;
				}
				printf("%s",cloud_char[y]);
				move = 1;
				continue;
			}
			
			//Render empty island characters
			if( water->cols[x].height < (y_water_height-8) ) {
				if( color_mode == CM_ISLAND ) {
					if( move ) {
						printf("\x1b[%ld;%ldH",y+1, x+1);
						move = 0;
					}
					printf(" ");
				}
				else {
					move = 1;
				}
			}
			//Render water
			else {
				if( move ) {
					printf("\x1b[%ld;%ldH",y+1, x+1);
					move = 0;
				}
				//Completely underwater (blue is background)
				if( water->cols[x].height >= y_water_height ) {
					if( color_mode != CM_WATER_BG ) {
						printf("\x1b[%dm",bgcolors[4]);
						color_mode = CM_WATER_BG;
					}
					printf(" ");
				}
				//Water line (blue is foreground)
				else {
					if( color_mode != CM_WATER_FG && color_mode != CM_ISLAND ) {
						printf("\x1b[%d;%dm",fgcolors[4],bgcolors[0]);
						color_mode = CM_WATER_FG;
					}
					printf("%s",water_chars[ (int)water->cols[x].height % 8 ] );
				}
			}
		}
	}
}


int drips_init(drips_t* drips, termsize_t *term) {
	if( !drips ) {
		return -1;
	}
	if( !term ) {
		return -2;
	}
	drips->term = term;
	drips->size = 0;
	drips->drips = 0;
	return 0;
}


int drips_generate(drips_t* drips, size_t x) {
	size_t i;
	drip_t *tmp;
	
	if( ! drips ) {
		return -1;
	}
	
	for( i=0; i<drips->size; i++ ) {
		if( ! drips->drips[i].active ) {
			break;
		}
	}
	if( i == drips->size ) {
		tmp = realloc(drips->drips,sizeof(drip_t)*(drips->size+1));
		if( ! tmp ) {
			return -2;
		}
		drips->drips = tmp;
		drips->size++;
	}
	drips->drips[i].active = 1;
	drips->drips[i].x = x;
	drips->drips[i].y = (drips->term->height - 2)*8;
	drips->drips[i].speed = 0;
	return 0;
}


int drips_update(drips_t* drips, water_t *water) {
	size_t i;
	
	if( ! drips ) {
		return -1;
	}
	if( ! water ) {
		return -2;
	}
	
	for( i=0; i<drips->size; i++ ) {
		if( drips->drips[i].active ) {
			drips->drips[i].speed = drips->drips[i].speed - GRAVITY;
			if( drips->drips[i].y < drips->drips[i].speed ) {
				drips->drips[i].y = 0;
			}
			else {
				drips->drips[i].y = drips->drips[i].y + drips->drips[i].speed;
			}
			if( drips->drips[i].y <= water->cols[drips->drips->x].height ) {
				water->cols[drips->drips->x].speed = water->cols[drips->drips->x].speed + 
					drips->drips[i].speed;
				drips->drips[i].active = 0;
				if( water->target_height < (water->term->height-3)*8 ) {
					water->target_height = water->target_height + 8.0 / water->term->width;
				}
			}
		}
	}
	return 0;
}


int cloud_init(cloud_t *cloud, termsize_t *term) {
	if( !cloud ) {
		return -1;
	}
	if( !term ) {
		return -2;
	}
	
	cloud->term = term;
	cloud->pos = (term->width*8)/2-2;
	cloud->speed = CLOUD_SPEED;
	
	cloud->drop_count = 0;
	cloud->drop_delay = 30;
	return 0;
}


int cloud_update(cloud_t *cloud, drips_t *drips) {
	if( !cloud ) {
		return -1;
	}
		
	if( cloud->term->updated ) {
		if( cloud->term->width < 5 ) {
			cloud->pos = 0;
		}
		else if( cloud->pos >= (cloud->term->width-5)*8 ) {
			cloud->pos = (cloud->term->width-5)*8;
		}
	}
	
	if( random()%(cloud->term->width*8) == 0 ) {
		cloud->speed = -1*cloud->speed;
	}
	
	cloud->pos = cloud->pos + cloud->speed;
	if( cloud->pos >= (cloud->term->width-5)*8 ) {
		cloud->pos = (cloud->term->width-5)*8;
		cloud->speed = -CLOUD_SPEED;
	}
	if( cloud->pos <= 0 ) {
		cloud->pos = 0;
		cloud->speed = CLOUD_SPEED;
	}
	
	cloud->drop_count++;
	if( cloud->drop_count >= cloud->drop_delay ) {
		cloud->drop_count = 0;
		drips_generate(drips,(int)(cloud->pos/8.0)+2);
	}
	
	return 0;
}


int main() {
	termsize_t term;
	drips_t drips;
	cloud_t cloud;
	water_t water;
	
	srandom(time(0));
	
	if( termsize_init(&term) ) {
		printf("Failed to intialize term\n");
		return 1;
	}
	if( drips_init(&drips,&term) ) {
		printf("Failed to initialize drips\n");
		return 1;
	}
	if( cloud_init(&cloud,&term) ) {
		printf("Failed to initialize cloud\n");
		return 1;
	}
	if( water_init(&water,&term) ) {
		printf("Failed to initialize water\n");
		return 1;
	}
	for(;;) {
		termsize_update(&term);
		drips_update(&drips,&water);
		cloud_update(&cloud,&drips);
		water_update(&water);
		printf("\x1b[2J\x1b[H");
		render(&water,&drips,&cloud);
		printf("\x1b[0m");
		fflush(0);
		usleep(100000);
	}
	return 0;
}