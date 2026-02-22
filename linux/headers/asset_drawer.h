#ifndef ASSET_DRAWER_H
#define ASSET_DRAWER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define PALETTE_WIDTH 120
#define PALETTE_SIZE 32

#define MAX_WINDOW_WIDTH 1600
#define MAX_WINDOW_HEIGHT 900

#define TEXT_SIZE 16

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

Color4 **alloc_pixels(int width, int height);
void free_pixels(Color4 **pixels, int height);
void init_pixels(Color4 **pixels, int width, int height);

void save_pixels(Color4 **pixels, int width, int height, const char* filename);
void load_pixels(Color4 **pixels, int width, int height, const char* filename);

#endif