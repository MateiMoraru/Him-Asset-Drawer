#ifndef ASSET_DRAWER_H
#define ASSET_DRAWER_H
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <SDL.h>

#include "Text.h"

#define PALETTE_WIDTH 120
#define PALETTE_SIZE 32

#define MAX_WINDOW_WIDTH 1600
#define MAX_WINDOW_HEIGHT 900

#define TEXT_SIZE 16

#define GRID_SIZE 16

#define clamp(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))

typedef struct
{
    int x;
    int y;
    int button;
} Mouse;

typedef struct
{
    int r;
    int g;
    int b;
    int a;
} Color4;

typedef struct
{
    int x;
    int y;
} Point;

typedef struct {
    int x1, y1, x2, y2;
    int active;
} SelectionArea;

typedef struct {
    Color4* pixels;
    int width;
    int height;
} Clipboard;

void selection_start(int x, int y);
void selection_update(int x, int y);
void selection_end(void);
void selection_draw_preview(SDL_Renderer* renderer);
void selection_copy(Color4** pixels);
void selection_paste(Color4** pixels, int mouse_x, int mouse_y);
void selection_flip_horizontal(Color4** pixels);
void clipboard_free(void);

extern SDL_Color BLACK;
extern SDL_Color WHITE;
extern SDL_Color RED;

extern Color4 CBLACK;
extern Color4 CWHITE;
extern Color4 CRED;
extern Color4 CNULL;
extern Color4 BACKGROUND_COLOR;

char* rgba_to_hex(Color4 *color);
Color4 hex_to_color(char* hexc);

void compute_initial_scale(void);

Color4 **alloc_pixels(int width, int height);
void free_pixels(Color4 **pixels, int height);
void init_pixels(Color4 **pixels, int width, int height);

void save_pixels(Color4 **pixels, int width, int height, const char* filename);
void load_pixels(Color4 **pixels, int width, int height, const char* filename);

#endif