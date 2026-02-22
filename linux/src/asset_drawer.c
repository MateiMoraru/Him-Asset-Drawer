#include "asset_drawer.h"

SDL_Color BLACK = {0, 0, 0, 255};
SDL_Color WHITE = {255, 255, 255, 255};
SDL_Color RED = {255, 0, 0, 255};

Color4 CBLACK = {0, 0, 0, 255};
Color4 CWHITE = {255, 255, 255, 255};
Color4 CRED = {255, 0, 0, 255};
Color4 CNULL = {0, 0, 0, 0};
Color4 BACKGROUND_COLOR = {173, 152, 125, 255};

int width;
int height;
int scale;
int initial_scale = 10;

int zoom_offset_x = 0;
int zoom_offset_y = 0;

char* rgba_to_hex(Color4 *color)
{
    char *hex = malloc(11);
    if (!hex) return NULL;

    sprintf(hex, "0x%02X%02X%02X%02X", color->r, color->g, color->b, color->a);

    return hex;
}

Color4 hex_to_color(char* hexc)
{
    uint32_t hex = (uint32_t) strtoul(hexc, NULL, 0);
    
    Color4 color;

    color.r = (hex >> 24) & 0xFF;
    color.g = (hex >> 16) & 0xFF;
    color.b = (hex >> 8) & 0xFF;
    color.a = hex & 0xFF;

    return color;
}

Color4 **alloc_pixels(int width, int height)
{
    Color4 **p = malloc(height * sizeof(Color4 *));
    if (!p) return NULL;

    for (int y = 0; y < height; y++)
    {
        p[y] = malloc(width * sizeof(Color4));
        if (!p[y]) return NULL;
    }
    return p;
}

void free_pixels(Color4 **pixels, int height)
{
    for (int y = 0; y < height; y++)
        free(pixels[y]);
    free(pixels);
}

void init_pixels(Color4 **pixels, int width, int height)
{
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            pixels[y][x] = CNULL;
}

void save_pixels(Color4 **pixels, int width, int height, const char* filename)
{
    FILE *fout = fopen(filename, "w");
    if (!fout) return;

    fprintf(fout, "%d %d\n", width, height);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            char *hex = rgba_to_hex(&pixels[y][x]);
            fprintf(fout, "%s ", hex);
            free(hex);
        }
        fprintf(fout, "\n");
    }
    fclose(fout);
}

void load_pixels(Color4 **pixels, int width, int height, const char* filename)
{
    FILE *fin = fopen(filename, "r");
    if (!fin) return;

    int w, h;
    if (fscanf(fin, "%d %d", &w, &h) != 2)
    {
        fclose(fin);
        return;
    }

    if (w != width || h != height)
    {
        printf("Size mismatch: file %dx%d, expected %dx%d\n", w, h, width, height);
        fclose(fin);
        return;
    }

    char color_str[11];
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            fscanf(fin, "%10s", color_str);
            pixels[y][x] = hex_to_color(color_str);
        }
    }

    fclose(fin);
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
        if(fscanf(file, "%d %d %d", &r, &g, &b) != 3) 
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
    SDL_Rect rect;
    rect.w = PALETTE_WIDTH / 4;
    rect.h = height * initial_scale / 8;

    SDL_Rect selected_rect;
    selected_rect.w = PALETTE_WIDTH / initial_scale;
    selected_rect.h = PALETTE_WIDTH / initial_scale;

    int index = 0;

    int start_x = width * initial_scale;

    for (int x = start_x; x < start_x + PALETTE_WIDTH; x += PALETTE_WIDTH / 4)
    {
        for (int y = 0; y < height * initial_scale; y += rect.h)
        {
            //if (index >= PALETTE_SIZE) break;

            rect.x = x;
            rect.y = y;

            SDL_SetRenderDrawColor(renderer, palette[index].r, palette[index].g, palette[index].b, 255);
            SDL_RenderFillRect(renderer, &rect);

            if (index == selected_color)
            {
                selected_rect.x = rect.x + (rect.w - selected_rect.w) / 2;
                selected_rect.y = rect.y + (rect.h - selected_rect.h) / 2;
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderDrawRect(renderer, &selected_rect);
            }

            index++;
        }
    }
}

void draw_pixels(SDL_Renderer* renderer, Color4 **pixels, Mouse* mouse)
{
    SDL_Rect rect = {0, 0, scale, scale};
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            rect.x = x * scale + zoom_offset_x;
            rect.y = y * scale + zoom_offset_y;

            SDL_SetRenderDrawColor(renderer, pixels[y][x].r, pixels[y][x].g, pixels[y][x].b, pixels[y][x].a);
            SDL_RenderFillRect(renderer, &rect);

            // if (rect.x == mouse->x && rect.y == mouse->y)
            // {
            //     SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            //     SDL_RenderDrawRect(renderer, &rect);
            // }
        }
    }

    int x = mouse->x / scale;
    int y = mouse->y / scale;

    rect.x = x * scale + zoom_offset_x;
    rect.y = y * scale + zoom_offset_y;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);

}

void draw_grid(SDL_Renderer* renderer)
{
    int block = 16 * scale;

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

void event_mouse(Mouse* mouse, int selected_color, Color4* palette, Color4 **pixels)
{
    if (!(mouse->button & (SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT))))
        return;

    int px = (mouse->x - zoom_offset_x) / scale;
    int py = (mouse->y - zoom_offset_y) / scale;

    px = clamp(px, 0, width  - 1);
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

void event_scroll(int delta, Mouse* mouse, int *selected_color)
{
    if (mouse->x > width * initial_scale)
    {
        *selected_color += delta;
        if (*selected_color < 0)
            *selected_color = PALETTE_SIZE - 1;
        if (*selected_color >= PALETTE_SIZE)
            *selected_color = 0;
    }
    else
    {
        scale += delta;
    }
}

void load_file(char* argv[], Color4 **pixels)
{
    FILE *fin = fopen(argv[4], "rb");

    if (!fin)
    {
        printf("Failed to open %s\n", argv[4]);
        return;
    }

    int w, h;
    fscanf(fin, "%d %d", &w, &h);

    fclose(fin);

    if(w != width || h != height)
    {
        printf("Expected %dx%d but found %dx%d\n", width, height, w, h);
        return;
        //init_pixels(pixels);
    }
    
    load_pixels(pixels, 128, 128, argv[4]);
    printf("Loaded file %s successfully\n", argv[4]);
    
    
}

void draw_text(SDL_Renderer* renderer, const char* str, TTF_Font* font, int posx, int posy)
{
    SDL_Surface* surface = TTF_RenderText_Blended(font, str, WHITE);
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest = {posx, posy, surface->w, surface->h};
    SDL_RenderCopy(renderer, text_texture, NULL, &dest);
    SDL_FreeSurface(surface);
}

Color4** process_command(SDL_Window* window, SDL_Renderer* renderer, Color4** pixels, const char* command, char* out_filename, Color4* palette)
{
    if (strlen(command) <= 1)
        return;

    char *tok = strtok(command, " ");

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
        width = atoi(tok);
        tok = strtok(NULL, " ");
        height = atoi(tok);

        pixels = alloc_pixels(width, height); 
        init_pixels(pixels, width, height);

        printf("Resized to %dx%d\n", width, height);

        compute_initial_scale();
        scale = initial_scale;

        SDL_SetWindowSize(window, width * scale + PALETTE_WIDTH, height * scale);

        return pixels;
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

void reinit_window(SDL_Window* window, SDL_Renderer* renderer, Color4 **pixels, const char* filename)
{
    load_file(filename, pixels);

    if(window) SDL_DestroyWindow(window);
    if(renderer) SDL_DestroyRenderer(renderer);

    window = SDL_CreateWindow("Asset drawer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width * scale + PALETTE_WIDTH, height * scale, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    compute_initial_scale();
    scale = initial_scale;
}

void compute_initial_scale()
{
    int scale_w = MAX_WINDOW_WIDTH / (width + PALETTE_WIDTH / initial_scale);
    int scale_h = MAX_WINDOW_HEIGHT / height;

    int s = scale_w < scale_h ? scale_w : scale_h;
    if (s < 1) s = 1;

    initial_scale = s;
}

void draw_line(Color4 **pixels, Point line[2], Color4 *color)
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

void draw_line_preview(SDL_Renderer* renderer, Point start, Mouse* mouse)
{
    int x0 = start.x;
    int y0 = start.y;

    int x1 = mouse->x / scale;
    int y1 = mouse->y / scale;

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);

    while (1)
    {
        int sxp = x0 * scale + zoom_offset_x;
        int syp = y0 * scale + zoom_offset_y;

        SDL_RenderDrawPoint(renderer, sxp, syp);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

int main(int argc, char* argv[])
{
    if(argc < 3)
    {
        printf("Usage: %s <width> <height>\n", argv[0]);
        return 1;
    }   

    width = atoi(argv[1]);
    height = atoi(argv[2]);

    char out_filename[128] = "assets/test.bmp";

    Color4 **pixels = alloc_pixels(width, height);
    if (!pixels) {
        printf("Failed to allocate pixels\n");
        return 1;
    }

    init_pixels(pixels, width, height);

    if(argc == 5)
    {
        printf("Attempting to %s %s\n", argv[3], argv[4]);
        if (strncmp(argv[3], "-l", 2) == 0)
        {
            load_file(argv, pixels);

        }
        else if (strncmp(argv[3], "-o", 2) == 0)
        {
            strcpy(out_filename, argv[4]);
        }
    }
    if (argc == 7)
    {
        if (strncmp(argv[3], "-l", 2) == 0)
        {
            load_file(argv, pixels);

        }
        if (strncmp(argv[5], "-o", 2) == 0)
        {
            strcpy(out_filename, argv[6]);
            printf("Save file: %s\n", out_filename);
        }
    }
    printf("Pixels working\n"); 

    compute_initial_scale();
    scale = initial_scale;

    int speed = scale;

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();    
    SDL_Window* window = SDL_CreateWindow("Asset drawer - bunda brudders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width * scale + PALETTE_WIDTH, height * scale, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    TTF_Font* font = TTF_OpenFont("assets/font.ttf", TEXT_SIZE);

    Mouse mouse = {0, 0, 0};
    printf("Mouse working\n");

    Color4 palette[PALETTE_SIZE];
    load_palette(palette, "assets/palette0.txt");
    printf("Palette working\n");

    int selected_color = 0;

    bool running = true;
    SDL_Event event;

    char text_buffer[128] = "";
    char command_buffer[128] = "";
    char command_text_buffer[132] = "";

    Point line_to_draw[2] = {{-1, -1}, {-1, -1}};
    Point temp_point = {0, 0};

    bool typing_command = false;
    bool drawing_line = false;

    while (running)
    {
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
                        pixels = process_command(window, renderer, pixels, command_buffer, out_filename, palette);
                        command_buffer[0] = '\0';
                    }
                }
            }

            if(event.type == SDL_KEYDOWN)
            {
                if(event.key.keysym.sym == SDLK_q && !typing_command)
                {
                    save_pixels(pixels, width, height, out_filename);
                    printf("\nSaved file\n");
                }
                
                if (event.key.keysym.sym == SDLK_RETURN) {
                    typing_command = !typing_command;
                }

                if (event.key.keysym.sym == SDLK_l && !typing_command)
                {
                    temp_point.x = mouse.x / scale;
                    temp_point.y = mouse.y / scale;
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

        }

        mouse.button = SDL_GetMouseState(&mouse.x, &mouse.y);

        mouse.x -= zoom_offset_x;
        mouse.y -= zoom_offset_y;

        int px = clamp(mouse.x / scale, 0, width  - 1);
        int py = clamp(mouse.y / scale, 0, height - 1);

        mouse.x = px * scale;
        mouse.y = py * scale;

        event_mouse(&mouse, selected_color, palette, pixels);

        const Uint8* keys = SDL_GetKeyboardState(NULL);

        if (!typing_command)
        {
            if (keys[SDL_SCANCODE_W]) {
                zoom_offset_y += speed;
            }
            if (keys[SDL_SCANCODE_S]) {
                zoom_offset_y -= speed;
            }
            if (keys[SDL_SCANCODE_A]) {
                zoom_offset_x += speed;
            }
            if (keys[SDL_SCANCODE_D]) {
                zoom_offset_x -= speed;
            }
        }

        SDL_SetRenderDrawColor(renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g, BACKGROUND_COLOR.b, BACKGROUND_COLOR.a);
        SDL_RenderClear(renderer);

        draw_pixels(renderer, pixels, &mouse);

        if (drawing_line)
        {
            draw_line_preview(renderer, line_to_draw[0], &mouse);
        }

        draw_grid(renderer);
        draw_palette_grid(renderer, palette, selected_color);


        sprintf(text_buffer, "%d, %d", mouse.x / scale, mouse.y / scale);
        draw_text(renderer, text_buffer, font, 0, height - TEXT_SIZE);

        if (typing_command)
        {
            sprintf(command_text_buffer, ">>> %s", command_buffer);
            draw_text(renderer, command_text_buffer, font, 0, height * initial_scale - TEXT_SIZE);
        }

        SDL_RenderPresent(renderer);
    }

    free_pixels(pixels, height);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}