#include <stdint.h>
#include <string.h>
#include <math.h>
#include <raylib.h>
#include "play.h"
#include "input_poll.h"

#ifndef FB_WIDTH
#define FB_WIDTH 128
#endif

#ifndef FB_HEIGHT
#define FB_HEIGHT 64
#endif

#ifndef FB_SIZE
#define FB_SIZE (FB_WIDTH * FB_HEIGHT / 8)
#endif

#ifndef LCD_DIAGONAL_INCHES
#define LCD_DIAGONAL_INCHES 0.96f
#endif

#ifndef FALLBACK_HOST_PPI
#define FALLBACK_HOST_PPI 110.0f
#endif

static int get_pixel(const uint8_t fb[FB_SIZE], int x, int y)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return 0;

    const int bytes_per_row = FB_WIDTH / 8;
    const int byte_index = y * bytes_per_row + (x / 8);
    const uint8_t bit = (uint8_t)(1u << (7 - (x % 8)));
    return (fb[byte_index] & bit) ? 1 : 0;
}

static float get_monitor_ppi(int monitor)
{
    const int px_w = GetMonitorWidth(monitor);
    const int mm_w = GetMonitorPhysicalWidth(monitor);

    if (px_w <= 0 || mm_w <= 0)
        return FALLBACK_HOST_PPI;

    return (float)px_w / ((float)mm_w / 25.4f);
}

static void fb_to_rgba(const uint8_t fb[FB_SIZE], Color *rgba)
{
    for (int y = 0; y < FB_HEIGHT; y++)
    {
        for (int x = 0; x < FB_WIDTH; x++)
        {
            const int i = y * FB_WIDTH + x;
            rgba[i] = get_pixel(fb, x, y) ? WHITE : BLACK;
        }
    }
}

int main(void)
{
    uint8_t framebuffer[FB_SIZE];
    play_state_t play_state;
    memset(&play_state, 0, sizeof(play_state));
    play_init(&play_state);

    const float aspect = (float)FB_WIDTH / (float)FB_HEIGHT;
    const float lcd_h_inches = LCD_DIAGONAL_INCHES / sqrtf((aspect * aspect) + 1.0f);
    const float lcd_w_inches = lcd_h_inches * aspect;
    const float host_ppi = get_monitor_ppi(0);
    const int win_w = (int)roundf(lcd_w_inches * host_ppi);
    const int win_h = (int)roundf(lcd_h_inches * host_ppi);

    InitWindow((win_w > 1) ? win_w : 1, (win_h > 1) ? win_h : 1, "Play Screen Test");
    SetTargetFPS(60);

    Image img = GenImageColor(FB_WIDTH, FB_HEIGHT, BLACK);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);

    Color pixels[FB_WIDTH * FB_HEIGHT];

    while (!WindowShouldClose())
    {
        const input_poll_t in = input_poll();
        const play_action_t action = play_update(&play_state, in, GetFrameTime(), FB_WIDTH, FB_HEIGHT);
        write_play(framebuffer, FB_WIDTH, FB_HEIGHT, &play_state);

        if (action == PLAY_ACTION_EXIT_TO_MENU)
            break;

        fb_to_rgba(framebuffer, pixels);
        UpdateTexture(tex, pixels);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(
            tex,
            (Rectangle){ 0.0f, 0.0f, (float)FB_WIDTH, (float)FB_HEIGHT },
            (Rectangle){ 0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight() },
            (Vector2){ 0.0f, 0.0f },
            0.0f,
            WHITE
        );
        EndDrawing();
    }

    play_deinit(&play_state);
    UnloadTexture(tex);
    CloseWindow();
    return 0;
}
