#ifndef TEXT_H
#define TEXT_H

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <SDL.h>
#include <stdint.h>
#include <stdint.h>

extern char font8x8_basic[128][8];

typedef struct {
    int width;
    int height;
    const uint8_t(*data)[8];
} Font;

void text_draw(SDL_Renderer* renderer, Font* font, int x, int y, const char* text, int font_size, SDL_Color color);

#endif