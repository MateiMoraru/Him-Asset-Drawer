#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "asset_drawer.h"

#include <windows.h>

void get_executable_path(char* path, size_t size) {
    GetModuleFileNameA(NULL, path, (DWORD)size);
    char* last_backslash = strrchr(path, '\\');
    if (last_backslash) {
        *(last_backslash + 1) = '\0';
    }
}

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

static int palette_left(void)
{
    return WINDOW_WIDTH - PALETTE_WIDTH;
}

SDL_Color BLACK = { 0, 0, 0, 255 };
SDL_Color WHITE = { 255, 255, 255, 255 };
SDL_Color RED = { 255, 0, 0, 255 };

Color4 CBLACK = { 0, 0, 0, 255 };
Color4 CWHITE = { 255, 255, 255, 255 };
Color4 CRED = { 255, 0, 0, 255 };
Color4 CNULL = { 0, 0, 0, 0 };
Color4 BACKGROUND_COLOR = { 173, 152, 125, 255 };

int width;
int height;
int scale;
int initial_scale = 10;

int zoom_offset_x = 0;
int zoom_offset_y = 0;

int hover_px = 0;
int hover_py = 0;

SelectionArea selection = { 0, 0, 0, 0, 0 };
Clipboard clipboard = { NULL, 0, 0 };

char* rgba_to_hex(Color4* color)
{
    char* hex = malloc(11);
    if (!hex) return NULL;

    sprintf(hex, "0x%02X%02X%02X%02X", color->r, color->g, color->b, color->a);

    return hex;
}

static void apply_canvas_layout(void)
{
    compute_initial_scale();
    scale = initial_scale;

    int canvas_w = width * scale;
    int canvas_h = height * scale;

    zoom_offset_x = (WINDOW_WIDTH - PALETTE_WIDTH - canvas_w) / 2;
    zoom_offset_y = (WINDOW_HEIGHT - canvas_h) / 2;
}


Color4 hex_to_color(char* hexc)
{
    uint32_t hex = (uint32_t)strtoul(hexc, NULL, 0);

    Color4 color;

    color.r = (hex >> 24) & 0xFF;
    color.g = (hex >> 16) & 0xFF;
    color.b = (hex >> 8) & 0xFF;
    color.a = hex & 0xFF;

    return color;
}

Color4** alloc_pixels(int width, int height)
{
    Color4** p = malloc(height * sizeof(Color4*));
    if (!p) return NULL;

    for (int y = 0; y < height; y++)
    {
        p[y] = malloc(width * sizeof(Color4));
        if (!p[y])
        {
            for (int i = 0; i < y; i++) free(p[i]);
            free(p);
            return NULL;
        }
    }
    return p;
}

void free_pixels(Color4** pixels, int height)
{
    for (int y = 0; y < height; y++)
        free(pixels[y]);
    free(pixels);
}

void init_pixels(Color4** pixels, int width, int height)
{
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            pixels[y][x] = CNULL;
}

void save_pixels(Color4** pixels, int width, int height, const char* filename)
{
    size_t plain_cap = (size_t)width * (size_t)height * 11 + (size_t)height + 64;
    char* plain = (char*)malloc(plain_cap);
    if (!plain)
        return;

    size_t pos = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            Color4 c = pixels[y][x];
            int wrote = snprintf(plain + pos, plain_cap - pos, "0x%02X%02X%02X%02X ", c.r, c.g, c.b, c.a);
            if (wrote <= 0 || pos + (size_t)wrote >= plain_cap)
            {
                free(plain);
                return;
            }
            pos += (size_t)wrote;
        }

        if (pos + 2 >= plain_cap)
        {
            free(plain);
            return;
        }

        plain[pos++] = '\n';
        plain[pos] = '\0';
    }

    size_t comp_cap = plain_cap * 32 + 1024;
    char* compressed = (char*)malloc(comp_cap);
    if (!compressed)
    {
        free(plain);
        return;
    }

    compressed[0] = '\0';
    compress_string(plain, compressed);

    FILE* fout = fopen(filename, "wb");
    if (!fout)
    {
        printf("save_pixels: failed to open '%s'\n", filename);
        free(compressed);
        free(plain);
        return;
    }

    fprintf(fout, "%d %d\n", width, height);

    size_t clen = strlen(compressed);
    size_t wrote = fwrite(compressed, 1, clen, fout);
    fclose(fout);

    if (wrote != clen)
        printf("save_pixels: short write '%s' (%zu/%zu)\n", filename, wrote, clen);
    else
        printf("save_pixels: wrote '%s' (%zu bytes payload)\n", filename, clen);

    free(compressed);
    free(plain);
}

void load_pixels(Color4*** pixels, int* width, int* height, const char* filename)
{
    FILE* fin = fopen(filename, "rb");
    if (!fin)
    {
        printf("load_pixels: failed to open '%s'\n", filename);
        return;
    }

    int file_w = 0;
    int file_h = 0;
    if (fscanf(fin, "%d %d", &file_w, &file_h) != 2)
    {
        printf("load_pixels: bad header '%s'\n", filename);
        fclose(fin);
        return;
    }

    int ch = fgetc(fin);
    while (ch != EOF && ch != '\n')
        ch = fgetc(fin);

    if (*pixels != NULL) {
        free_pixels(*pixels, *height);
    }

    *pixels = alloc_pixels(file_w, file_h);
    if (!*pixels) {
        printf("load_pixels: failed to allocate memory\n");
        fclose(fin);
        return;
    }

    init_pixels(*pixels, file_w, file_h);

    fseek(fin, 0, SEEK_END);
    long end_pos = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    fscanf(fin, "%*d %*d");
    ch = fgetc(fin);
    while (ch != EOF && ch != '\n')
        ch = fgetc(fin);

    long payload_start = ftell(fin);
    size_t payload_len = (size_t)(end_pos - payload_start);

    char* compressed = (char*)malloc(payload_len + 1);
    if (!compressed)
    {
        free_pixels(*pixels, file_h);
        fclose(fin);
        return;
    }

    size_t r = fread(compressed, 1, payload_len, fin);
    fclose(fin);
    compressed[r] = '\0';

    size_t decomp_cap = (size_t)file_w * (size_t)file_h * 11 + (size_t)file_h + 128;
    char* decompressed = (char*)malloc(decomp_cap);
    if (!decompressed)
    {
        free(compressed);
        free_pixels(*pixels, file_h);
        return;
    }

    decompressed[0] = '\0';
    decompress_string(compressed, decompressed);

    char* tok = strtok(decompressed, " \t\r\n");

    for (int y = 0; y < file_h; y++)
    {
        for (int x = 0; x < file_w; x++)
        {
            if (!tok)
            {
                printf("load_pixels: truncated data '%s'\n", filename);
                free(decompressed);
                free(compressed);
                free_pixels(*pixels, file_h);
                return;
            }

            (*pixels)[y][x] = hex_to_color(tok);
            tok = strtok(NULL, " \t\r\n");
        }
    }

    *width = file_w;
    *height = file_h;

    printf("load_pixels: loaded '%s' (%dx%d)\n", filename, file_w, file_h);

    free(decompressed);
    free(compressed);
}


void load_palette(Color4* palette, const char* filename)
{
    FILE* file = fopen(filename, "rb");
    if (!file)
    {
        printf("Failed to open %s\n", filename);
        return;
    }

    int r, g, b;
    Color4 color;
    color.a = 255;
    for (int i = 0; i < PALETTE_SIZE; i++)
    {
        if (fscanf(file, "%d %d %d", &r, &g, &b) != 3)
        {
            fprintf(stderr, "Failed to read color %d\n", i);
            break;
        }

        color.r = r;
        color.g = g;
        color.b = b;

        palette[i] = color;

    }

    fclose(file);

}

void draw_palette_grid(SDL_Renderer* renderer, Color4* palette, int selected_color)
{
    int cols = 4;
    int rows = (PALETTE_SIZE + cols - 1) / cols;

    int start_x = palette_left();

    SDL_Rect rect;
    rect.w = PALETTE_WIDTH / cols;
    rect.h = WINDOW_HEIGHT / rows;

    SDL_Rect selected_rect;

    int index = 0;
    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            if (index >= PALETTE_SIZE) break;

            rect.x = start_x + c * rect.w;
            rect.y = r * rect.h;

            SDL_SetRenderDrawColor(renderer, palette[index].r, palette[index].g, palette[index].b, 255);
            SDL_RenderFillRect(renderer, &rect);

            if (index == selected_color)
            {
                int inset = rect.w < rect.h ? rect.w / 6 : rect.h / 6;
                if (inset < 2) inset = 2;

                selected_rect.x = rect.x + inset;
                selected_rect.y = rect.y + inset;
                selected_rect.w = rect.w - 2 * inset;
                selected_rect.h = rect.h - 2 * inset;

                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &selected_rect);
            }

            index++;
        }
    }
}


void draw_pixels(SDL_Renderer* renderer, Color4** pixels, Mouse* mouse)
{
    SDL_Rect rect = { 0, 0, scale, scale };

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            rect.x = x * scale + zoom_offset_x;
            rect.y = y * scale + zoom_offset_y;

            SDL_SetRenderDrawColor(renderer, pixels[y][x].r, pixels[y][x].g, pixels[y][x].b, pixels[y][x].a);
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    if (mouse->x >= palette_left())
        return;

    int px = (mouse->x - zoom_offset_x) / scale;
    int py = (mouse->y - zoom_offset_y) / scale;

    px = clamp(px, 0, width - 1);
    py = clamp(py, 0, height - 1);

    rect.x = px * scale + zoom_offset_x;
    rect.y = py * scale + zoom_offset_y;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);
}

void draw_grid(SDL_Renderer* renderer)
{
    int block = GRID_SIZE * scale;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (int x = 0; x <= width * scale; x += block)
    {
        int sx = x + zoom_offset_x;
        SDL_RenderDrawLine(renderer, sx, zoom_offset_y, sx, height * scale + zoom_offset_y);
    }

    for (int y = 0; y <= height * scale; y += block)
    {
        int sy = y + zoom_offset_y;
        SDL_RenderDrawLine(renderer, zoom_offset_x, sy, width * scale + zoom_offset_x, sy);
    }
}

void event_mouse(Mouse* mouse, int selected_color, Color4* palette, Color4** pixels)
{
    if (mouse->x >= palette_left())
        return;
    if (!(mouse->button & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))))
        return;

    int px = (mouse->x - zoom_offset_x) / scale;
    int py = (mouse->y - zoom_offset_y) / scale;

    px = clamp(px, 0, width - 1);
    py = clamp(py, 0, height - 1);

    if (mouse->button & SDL_BUTTON(SDL_BUTTON_LEFT))
    {
        pixels[py][px] = palette[selected_color];
    }
    else if (mouse->button & SDL_BUTTON(SDL_BUTTON_RIGHT))
    {
        pixels[py][px] = CNULL;
    }
}

void event_scroll(int delta, Mouse* mouse, int* selected_color)
{
    int mx = 0, my = 0;
    SDL_GetMouseState(&mx, &my);

    if (mx >= palette_left())
    {
        *selected_color += delta;
        if (*selected_color < 0) *selected_color = PALETTE_SIZE - 1;
        if (*selected_color >= PALETTE_SIZE) *selected_color = 0;
    }
    else
    {
        scale += delta;
        if (scale < 1) scale = 1;
    }
}

void load_file(char* argv[], Color4*** pixels, int* width, int* height)
{
    load_pixels(pixels, width, height, argv[4]);
    printf("Loaded file %s\n", argv[4]);
}

//void draw_text(SDL_Renderer* renderer, const char* str, TTF_Font* font, int posx, int posy)
//{
//    SDL_Surface* surface = TTF_RenderText_Blended(font, str, WHITE);
//    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, surface);
//    SDL_Rect dest = {posx, posy, surface->w, surface->h};
//    SDL_RenderCopy(renderer, text_texture, NULL, &dest);
//    SDL_FreeSurface(surface);
//}

Color4** process_command(SDL_Window* window, SDL_Renderer* renderer, Color4** pixels, const char* command, char* out_filename, Color4* palette, int* prev_height)
{
    if (strlen(command) <= 1)
        return;

    char* tok = strtok(command, " ");

    if (strcmp(tok, "-o") == 0)
    {
        tok = strtok(NULL, " ");
        strcpy(out_filename, tok);

        printf("Save file: %s\n", tok);
    }
    else if (strcmp(tok, "-lp") == 0)
    {
        tok = strtok(NULL, " ");
        load_palette(palette, tok);
        printf("Changed palette to %s\n", tok);
    }
    else if (strcmp(tok, "size") == 0)
    {
        tok = strtok(NULL, " ");
        int new_w = tok ? atoi(tok) : 0;

        tok = strtok(NULL, " ");
        int new_h = tok ? atoi(tok) : 0;

        if (new_w <= 0 || new_h <= 0)
        {
            printf("Usage: size <w> <h>\n");
            return pixels;
        }

        Color4** new_pixels = alloc_pixels(new_w, new_h);
        if (!new_pixels)
        {
            printf("Resize failed: alloc_pixels(%d,%d)\n", new_w, new_h);
            return pixels;
        }

        init_pixels(new_pixels, new_w, new_h);

        int copy_w = new_w < width ? new_w : width;
        int copy_h = new_h < height ? new_h : height;

        for (int y = 0; y < copy_h; y++)
            for (int x = 0; x < copy_w; x++)
                new_pixels[y][x] = pixels[y][x];

        free_pixels(pixels, *prev_height);

        width = new_w;
        height = new_h;
        *prev_height = new_h;

        apply_canvas_layout();
        SDL_SetWindowSize(window, WINDOW_WIDTH, WINDOW_HEIGHT);

        printf("Resized to %dx%d\n", width, height);
        return new_pixels;
    }
    else if (strcmp(tok, "scale") == 0)
    {
        tok = strtok(NULL, " ");
        scale = atoi(tok);

        printf("Scale: %d\n", scale);
    }
    else if (strcmp(tok, "clear") == 0)
    {
        init_pixels(pixels, width, height);

        printf("Cleared screen\n");
    }
    else if (strcmp(tok, "set") == 0)
    {
        tok = strtok(NULL, " ");

        if (strcmp(tok, "palette") == 0)
        {
            tok = strtok(NULL, " ");

            int index = atoi(tok);

            Color4 color;
            tok = strtok(NULL, " ");
            color.r = atoi(tok);

            tok = strtok(NULL, " ");
            color.g = atoi(tok);

            tok = strtok(NULL, " ");
            color.b = atoi(tok);

            color.a = 255;

            palette[index] = color;
        }
    }
    else
    {
        printf("Unknown command\n");
    }

    return pixels;
}

void reinit_window(SDL_Window* window, SDL_Renderer* renderer, Color4** pixels, const char* filename)
{
    //load_file(filename, pixels);

    if (window) SDL_DestroyWindow(window);
    if (renderer) SDL_DestroyRenderer(renderer);

    window = SDL_CreateWindow("Asset drawer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width * scale + PALETTE_WIDTH, height * scale, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    compute_initial_scale();
    scale = initial_scale;
}

void compute_initial_scale()
{
    int usable_width = WINDOW_WIDTH - PALETTE_WIDTH;
    int usable_height = WINDOW_HEIGHT;

    int scale_w = usable_width / width;
    int scale_h = usable_height / height;

    int s = scale_w < scale_h ? scale_w : scale_h;

    if (s < 1) s = 1;

    initial_scale = s;
}


void draw_line(Color4** pixels, Point line[2], Color4* color)
{
    int x0 = line[0].x;
    int y0 = line[0].y;
    int x1 = line[1].x;
    int y1 = line[1].y;

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    while (1)
    {
        if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height)
            pixels[y0][x0] = *color;

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void draw_line_preview(SDL_Renderer* renderer, Point start, int end_x, int end_y)
{
    int x0 = start.x;
    int y0 = start.y;

    int x1 = end_x;
    int y1 = end_y;

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);

    while (1)
    {
        SDL_Rect r;
        r.x = x0 * scale + zoom_offset_x;
        r.y = y0 * scale + zoom_offset_y;
        r.w = scale;
        r.h = scale;

        SDL_RenderDrawRect(renderer, &r);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

static int circle_radius_from_points(Point c, int x, int y)
{
    int dx = x - c.x;
    int dy = y - c.y;
    double d = sqrt((double)dx * (double)dx + (double)dy * (double)dy);
    int r = (int)(d + 0.5);
    if (r < 0) r = 0;
    return r;
}

void draw_circle(Color4** pixels, Point center, int radius, Color4* color)
{
    int cx = center.x;
    int cy = center.y;

    int x = radius;
    int y = 0;
    int err = 1 - x;

    while (x >= y)
    {
        int pts[8][2] =
        {
            { cx + x, cy + y },
            { cx + y, cy + x },
            { cx - y, cy + x },
            { cx - x, cy + y },
            { cx - x, cy - y },
            { cx - y, cy - x },
            { cx + y, cy - x },
            { cx + x, cy - y }
        };

        for (int i = 0; i < 8; i++)
        {
            int px = pts[i][0];
            int py = pts[i][1];

            if (px >= 0 && px < width && py >= 0 && py < height)
                pixels[py][px] = *color;
        }

        y++;

        if (err < 0)
        {
            err += 2 * y + 1;
        }
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void draw_circle_preview(SDL_Renderer* renderer, Point center, int end_x, int end_y)
{
    int radius = circle_radius_from_points(center, end_x, end_y);

    int cx = center.x;
    int cy = center.y;

    int x = radius;
    int y = 0;
    int err = 1 - x;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);

    while (x >= y)
    {
        int pts[8][2] =
        {
            { cx + x, cy + y },
            { cx + y, cy + x },
            { cx - y, cy + x },
            { cx - x, cy + y },
            { cx - x, cy - y },
            { cx - y, cy - x },
            { cx + y, cy - x },
            { cx + x, cy - y }
        };

        for (int i = 0; i < 8; i++)
        {
            int px = pts[i][0];
            int py = pts[i][1];

            if (px < 0 || px >= width || py < 0 || py >= height)
                continue;

            SDL_Rect r;
            r.x = px * scale + zoom_offset_x;
            r.y = py * scale + zoom_offset_y;
            r.w = scale;
            r.h = scale;

            SDL_RenderDrawRect(renderer, &r);
        }

        y++;

        if (err < 0)
        {
            err += 2 * y + 1;
        }
        else
        {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void selection_start(int x, int y)
{
    selection.x1 = x;
    selection.y1 = y;
    selection.x2 = x;
    selection.y2 = y;
    selection.active = 1;
}

void selection_update(int x, int y)
{
    if (selection.active) {
        selection.x2 = x;
        selection.y2 = y;
    }
}

void selection_end(void)
{
    if (selection.x1 > selection.x2) {
        int temp = selection.x1;
        selection.x1 = selection.x2;
        selection.x2 = temp;
    }
    if (selection.y1 > selection.y2) {
        int temp = selection.y1;
        selection.y1 = selection.y2;
        selection.y2 = temp;
    }

    if (selection.x1 == selection.x2 || selection.y1 == selection.y2) {
        selection.active = 0;
    }
}

void selection_draw_preview(SDL_Renderer* renderer)
{
    if (!selection.active) return;

    int x1 = selection.x1 < selection.x2 ? selection.x1 : selection.x2;
    int y1 = selection.y1 < selection.y2 ? selection.y1 : selection.y2;
    int x2 = selection.x1 > selection.x2 ? selection.x1 : selection.x2;
    int y2 = selection.y1 > selection.y2 ? selection.y1 : selection.y2;

    SDL_Rect rect;
    rect.x = x1 * scale + zoom_offset_x;
    rect.y = y1 * scale + zoom_offset_y;
    rect.w = (x2 - x1 + 1) * scale;
    rect.h = (y2 - y1 + 1) * scale;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int corner_len = 10;
    SDL_RenderDrawLine(renderer, rect.x, rect.y, rect.x + corner_len, rect.y);
    SDL_RenderDrawLine(renderer, rect.x, rect.y, rect.x, rect.y + corner_len);
    SDL_RenderDrawLine(renderer, rect.x + rect.w, rect.y, rect.x + rect.w - corner_len, rect.y);
    SDL_RenderDrawLine(renderer, rect.x + rect.w, rect.y, rect.x + rect.w, rect.y + corner_len);
    SDL_RenderDrawLine(renderer, rect.x, rect.y + rect.h, rect.x + corner_len, rect.y + rect.h);
    SDL_RenderDrawLine(renderer, rect.x, rect.y + rect.h, rect.x, rect.y + rect.h - corner_len);
    SDL_RenderDrawLine(renderer, rect.x + rect.w, rect.y + rect.h, rect.x + rect.w - corner_len, rect.y + rect.h);
    SDL_RenderDrawLine(renderer, rect.x + rect.w, rect.y + rect.h, rect.x + rect.w, rect.y + rect.h - corner_len);
}

void clipboard_free(void)
{
    if (clipboard.pixels) {
        free(clipboard.pixels);
        clipboard.pixels = NULL;
        clipboard.width = 0;
        clipboard.height = 0;
    }
}

void selection_copy(Color4** pixels)
{
    if (!selection.active) {
        printf("No selection to copy\n");
        return;
    }

    clipboard_free();

    int sel_width = selection.x2 - selection.x1 + 1;
    int sel_height = selection.y2 - selection.y1 + 1;

    clipboard.pixels = (Color4*)malloc(sel_width * sel_height * sizeof(Color4));
    if (!clipboard.pixels) {
        printf("Failed to allocate clipboard memory\n");
        return;
    }

    for (int y = 0; y < sel_height; y++) {
        for (int x = 0; x < sel_width; x++) {
            int src_y = selection.y1 + y;
            int src_x = selection.x1 + x;
            int dst_index = y * sel_width + x;
            clipboard.pixels[dst_index] = pixels[src_y][src_x];
        }
    }

    clipboard.width = sel_width;
    clipboard.height = sel_height;

    printf("Copied selection (%dx%d)\n", sel_width, sel_height);
}

void selection_paste(Color4** pixels, int mouse_x, int mouse_y)
{
    if (!clipboard.pixels) {
        printf("Nothing to paste\n");
        return;
    }

    int paste_x = mouse_x - clipboard.width / 2;
    int paste_y = mouse_y - clipboard.height / 2;

    if (paste_x < 0) paste_x = 0;
    if (paste_y < 0) paste_y = 0;
    if (paste_x + clipboard.width > width) paste_x = width - clipboard.width;
    if (paste_y + clipboard.height > height) paste_y = height - clipboard.height;

    for (int y = 0; y < clipboard.height; y++) {
        for (int x = 0; x < clipboard.width; x++) {
            int dst_y = paste_y + y;
            int dst_x = paste_x + x;
            int src_index = y * clipboard.width + x;
            pixels[dst_y][dst_x] = clipboard.pixels[src_index];
        }
    }

    printf("Pasted at (%d,%d)\n", paste_x, paste_y);
}

void selection_flip_horizontal(Color4** pixels)
{
    if (!selection.active) {
        printf("No selection to flip\n");
        return;
    }

    int sel_width = selection.x2 - selection.x1 + 1;
    int sel_height = selection.y2 - selection.y1 + 1;

    for (int y = 0; y < sel_height; y++) {
        for (int x = 0; x < sel_width / 2; x++) {
            int left_x = selection.x1 + x;
            int right_x = selection.x2 - x;

            Color4 temp = pixels[selection.y1 + y][left_x];
            pixels[selection.y1 + y][left_x] = pixels[selection.y1 + y][right_x];
            pixels[selection.y1 + y][right_x] = temp;
        }
    }

    printf("Flipped selection horizontally\n");
}

int main(int argc, char* argv[])
{
    char exe_path[512];
    char palette_path[512];
    char default_out_path[512];

    get_executable_path(exe_path, sizeof(exe_path));

    snprintf(palette_path, sizeof(palette_path), "%sassets\\palette0.txt", exe_path);
    snprintf(default_out_path, sizeof(default_out_path), "%sassets\\test.bmp", exe_path);

    width = 160;
    height = 160;
    char load_filename[256] = "";
    char out_filename[256] = "";
    strcpy(out_filename, default_out_path);  // Use full fucking path

    if (argc == 2) {
        char* ext = strrchr(argv[1], '.');
        if (ext && (strcmp(ext, ".him") == 0 || strcmp(ext, ".HIM") == 0)) {
            strcpy(load_filename, argv[1]);
            strcpy(out_filename, argv[1]);
            printf("Opening file with auto-flags: %s\n", load_filename);
        }
    }
    else if (argc >= 3) {
        width = atoi(argv[1]);
        height = atoi(argv[2]);

        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "-l") == 0 && i + 1 < argc) {
                strcpy(load_filename, argv[i + 1]);
                i++;
                printf("Load file: %s\n", load_filename);
            }
            else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                if (strchr(argv[i + 1], '\\') == NULL && strchr(argv[i + 1], '/') == NULL) {
                    snprintf(out_filename, sizeof(out_filename), "%s%s", exe_path, argv[i + 1]);
                }
                else {
                    strcpy(out_filename, argv[i + 1]);
                }
                i++;
                printf("Save file: %s\n", out_filename);
            }
        }
    }

    Color4** pixels = alloc_pixels(width, height);
    if (!pixels) {
        printf("Failed to allocate pixels\n");
        return 1;
    }

    float zoom_offset_x_f;
    float zoom_offset_y_f;

    init_pixels(pixels, width, height);

    int canvas_w = width * scale;
    int canvas_h = height * scale;

    if (strlen(load_filename) > 0) {
        load_pixels(&pixels, &width, &height, load_filename);
        printf("Loaded file %s (%dx%d)\n", load_filename, width, height);

        compute_initial_scale();
        scale = initial_scale;

        canvas_w = width * scale;
        canvas_h = height * scale;
        zoom_offset_x = (WINDOW_WIDTH - PALETTE_WIDTH - canvas_w) / 2;
        zoom_offset_y = (WINDOW_HEIGHT - canvas_h) / 2;
        zoom_offset_x_f = (float)zoom_offset_x;
        zoom_offset_y_f = (float)zoom_offset_y;
    }
    else
    {
        zoom_offset_x = (WINDOW_WIDTH - PALETTE_WIDTH - canvas_w) / 2;
        zoom_offset_y = (WINDOW_HEIGHT - canvas_h) / 2;

        zoom_offset_x_f = (float)zoom_offset_x;
        zoom_offset_y_f = (float)zoom_offset_y;
    }

    printf("Pixels working\n");
    printf("Executable path: %s\n", exe_path);
    printf("Palette path: %s\n", palette_path);
    printf("Default output: %s\n", default_out_path);

    compute_initial_scale();
    scale = initial_scale;

    int speed = scale;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Asset drawer - bunda brudders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    Font font =
    {
        8,
        8,
        font8x8_basic
    };

    Mouse mouse = { 0, 0, 0 };
    printf("Mouse working\n");

    Color4 palette[PALETTE_SIZE];
    load_palette(palette, palette_path);  // Use full path for fucks
    printf("Palette working\n");

    int selected_color = 0;

    bool running = true;
    SDL_Event event;

    char text_buffer[128] = "";
    char command_buffer[128] = "";
    char command_text_buffer[132] = "";

    Point line_to_draw[2] = { {-1, -1}, {-1, -1} };
    Point temp_point = { 0, 0 };
    Point circle_center = { -1, -1 };

    bool typing_command = false;
    bool drawing_line = false;
    bool drawing_circle = false;
    bool selecting = false;

    Uint32 last_ticks = SDL_GetTicks();

    int pixels_height_allocated = height;

    while (running)
    {
        Uint32 now_ticks = SDL_GetTicks();
        float dt = (now_ticks - last_ticks) / 1000.0f;
        last_ticks = now_ticks;

        if (dt > 0.05f) dt = 0.05f;

        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            if (event.type == SDL_MOUSEWHEEL)
            {
                event_scroll(event.wheel.y, &mouse, &selected_color);
            }

            if (typing_command)
            {
                if (event.type == SDL_TEXTINPUT) {
                    strcat(command_buffer, event.text.text);
                }

                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_BACKSPACE && strlen(command_buffer) > 0) {
                        command_buffer[strlen(command_buffer) - 1] = '\0';
                    }
                    else if (event.key.keysym.sym == SDLK_RETURN) {
                        pixels = process_command(window, renderer, pixels, command_buffer, out_filename, palette, &pixels_height_allocated);
                        command_buffer[0] = '\0';
                    }
                }
            }

            if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_q && !typing_command)
                {
                    save_pixels(pixels, width, height, out_filename);
                    printf("\nSaved file\n");
                }

                if (event.key.keysym.sym == SDLK_RETURN) {
                    typing_command = !typing_command;
                }

                if (event.key.keysym.sym == SDLK_l && !typing_command)
                {
                    if (mouse.x < palette_left())
                    {
                        temp_point.x = hover_px;
                        temp_point.y = hover_py;

                        if (drawing_line)
                        {
                            line_to_draw[1] = temp_point;
                            drawing_line = false;
                            draw_line(pixels, line_to_draw, &palette[selected_color]);
                        }
                        else
                        {
                            line_to_draw[0] = temp_point;
                            drawing_line = true;
                        }
                    }
                }

                if (event.key.keysym.sym == SDLK_c && !typing_command)
                {
                    if (mouse.x < palette_left())
                    {
                        if (!drawing_circle)
                        {
                            circle_center.x = hover_px;
                            circle_center.y = hover_py;
                            drawing_circle = true;
                        }
                        else
                        {
                            int r = circle_radius_from_points(circle_center, hover_px, hover_py);
                            drawing_circle = false;
                            draw_circle(pixels, circle_center, r, &palette[selected_color]);
                        }
                    }
                }

                if (event.key.keysym.sym == SDLK_x && !typing_command) {
                    if (selection.active) {
                        selection_copy(pixels);
                    }
                    else {
                        printf("No active selection to copy\n");
                    }
                }

                if (event.key.keysym.sym == SDLK_v && !typing_command) {
                    if (mouse.x < palette_left()) {
                        selection_paste(pixels, hover_px, hover_py);
                    }
                }

                if (event.key.keysym.sym == SDLK_r && !typing_command) {
                    if (selection.active) {
                        selection_flip_horizontal(pixels);
                    }
                    else {
                        printf("No active selection to flip\n");
                    }
                }

            }

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_MIDDLE && mouse.x < palette_left()) {
                    // Start selection :)
                    selecting = true;
                    selection_start(hover_px, hover_py);
                    printf("Selection started at (%d,%d)\n", hover_px, hover_py);
                }
            }

            if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_MIDDLE) {
                    // End selection
                    selecting = false;
                    selection_end();
                    if (selection.active) {
                        printf("Selection ended: (%d,%d) to (%d,%d)\n",
                            selection.x1, selection.y1, selection.x2, selection.y2);
                    }
                }
            }

            if (event.type == SDL_MOUSEMOTION) {
                if (selecting) {
                    selection_update(hover_px, hover_py);
                }
            }

        }

        int raw_mx = 0;
        int raw_my = 0;
        mouse.button = SDL_GetMouseState(&raw_mx, &raw_my);

        if (raw_mx < palette_left())
        {
            hover_px = clamp((raw_mx - zoom_offset_x) / scale, 0, width - 1);
            hover_py = clamp((raw_my - zoom_offset_y) / scale, 0, height - 1);
        }

        mouse.x = raw_mx;
        mouse.y = raw_my;

        if (raw_mx < palette_left())
        {
            int canvas_mx = raw_mx - zoom_offset_x;
            int canvas_my = raw_my - zoom_offset_y;

            int px = clamp(canvas_mx / scale, 0, width - 1);
            int py = clamp(canvas_my / scale, 0, height - 1);

            mouse.x = hover_px * scale + zoom_offset_x;
            mouse.y = hover_py * scale + zoom_offset_y;

            event_mouse(&mouse, selected_color, palette, pixels);
        }

        const Uint8* keys = SDL_GetKeyboardState(NULL);

        if (!typing_command)
        {
            float pan_speed = 900.0f;
            if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
                pan_speed = 1600.0f;

            float dx = 0.0f;
            float dy = 0.0f;

            if (keys[SDL_SCANCODE_W]) dy += pan_speed * dt;
            if (keys[SDL_SCANCODE_S]) dy -= pan_speed * dt;
            if (keys[SDL_SCANCODE_A]) dx += pan_speed * dt;
            if (keys[SDL_SCANCODE_D]) dx -= pan_speed * dt;

            zoom_offset_x_f += dx;
            zoom_offset_y_f += dy;

            zoom_offset_x = (int)zoom_offset_x_f;
            zoom_offset_y = (int)zoom_offset_y_f;
        }

        SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, BACKGROUND_COLOR.a);
        SDL_RenderClear(renderer);

        draw_pixels(renderer, pixels, &mouse);

        draw_grid(renderer);
        draw_palette_grid(renderer, palette, selected_color);

        if (drawing_line)
        {
            draw_line_preview(renderer, line_to_draw[0], hover_px, hover_py);
        }
        if (drawing_circle && mouse.x < palette_left())
        {
            draw_circle_preview(renderer, circle_center, hover_px, hover_py);
        }

        if (selecting || selection.active) {
            selection_draw_preview(renderer);
        }

        sprintf(text_buffer, "pos: %d, %d", hover_px, hover_py);
        text_draw(renderer, &font, 0, 0, text_buffer, TEXT_SIZE / 8, (SDL_Color) { 255, 255, 255, 255 });

        sprintf(text_buffer, "tile pos: %d, %d", hover_px % GRID_SIZE, hover_py % GRID_SIZE);
        text_draw(renderer, &font, 0, TEXT_SIZE, text_buffer, TEXT_SIZE / 8, (SDL_Color) { 255, 255, 255, 255 });

        if (typing_command)
        {
            sprintf(command_text_buffer, "> %s", command_buffer);
            text_draw(renderer, &font, 0, WINDOW_HEIGHT - TEXT_SIZE, command_text_buffer, TEXT_SIZE / 8, (SDL_Color) { 255, 255, 255, 255 });
        }

        SDL_RenderPresent(renderer);
    }

    free_pixels(pixels, height);
    clipboard_free();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}