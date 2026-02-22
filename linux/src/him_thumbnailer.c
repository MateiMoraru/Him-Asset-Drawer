#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

typedef struct {
    uint8_t r, g, b, a;
} Color4;


static Color4* load_him(const char* path, int* w, int* h)
{
    FILE* f = fopen(path, "r");
    if (!f) return NULL;

    if (fscanf(f, "%d %d", w, h) != 2) {
        fclose(f);
        return NULL;
    }

    Color4* pixels = malloc((*w) * (*h) * sizeof(Color4));
    if (!pixels) {
        fclose(f);
        return NULL;
    }

    char buf[11];
    for (int y = 0; y < *h; y++) {
        for (int x = 0; x < *w; x++) {
            if (fscanf(f, "%10s", buf) != 1) {
                free(pixels);
                fclose(f);
                return NULL;
            }

            uint32_t hex = (uint32_t)strtoul(buf, NULL, 0);
            Color4 c = {
                .r = (hex >> 24) & 0xFF,
                .g = (hex >> 16) & 0xFF,
                .b = (hex >> 8)  & 0xFF,
                .a =  hex        & 0xFF
            };

            pixels[y * (*w) + x] = c;
        }
    }

    fclose(f);
    return pixels;
}

static Color4* scale_image(Color4* src, int sw, int sh, int dw, int dh)
{
    Color4* dst = malloc(dw * dh * sizeof(Color4));
    if (!dst) return NULL;

    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            int sy = y * sh / dh;
            dst[y * dw + x] = src[sy * sw + sx];
        }
    }
    return dst;
}

int main(int argc, char** argv)
{
    if (argc < 4)
        return 1;

    const char* input  = argv[1];
    const char* output = argv[2];
    int thumb_size     = atoi(argv[3]);

    int w, h;
    Color4* pixels = load_him(input, &w, &h);
    if (!pixels)
        return 1;

    int tw = thumb_size;
    int th = thumb_size;

    if (w > h)
        th = (h * thumb_size) / w;
    else
        tw = (w * thumb_size) / h;

    Color4* thumb = scale_image(pixels, w, h, tw, th);
    free(pixels);

    if (!thumb)
        return 1;

    int ok = stbi_write_png(output, tw, th, 4, thumb, tw * sizeof(Color4));

    free(thumb);
    return ok ? 0 : 1;
}
