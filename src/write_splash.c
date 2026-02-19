#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <raylib.h>
#include "input_poll.h"
#include "intro_animation.h"
#include "intro_audio.h"
#include "menu.h"
#include "play.h"

#ifndef FB_WIDTH
#define FB_WIDTH 128
#endif

#ifndef FB_HEIGHT
#define FB_HEIGHT 64
#endif

#ifndef FB_SIZE
#define FB_SIZE (FB_WIDTH * FB_HEIGHT / 8)
#endif

// ---------------------
// Horizontal 1bpp layout
// Row-major, 8 pixels per byte
// byte_index = y * (FB_WIDTH/8) + x/8
// bit = (7 - (x % 8))  // MSB-left
// ---------------------
static void fb_clear(uint8_t fb[FB_SIZE])
{
    memset(fb, 0, FB_SIZE);
}

static void fb_copy(uint8_t dst[FB_SIZE], const uint8_t src[FB_SIZE])
{
    memcpy(dst, src, FB_SIZE);
}

static void set_pixel(uint8_t fb[FB_SIZE], int x, int y)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return;

    const int bytes_per_row = FB_WIDTH / 8;
    const int byte_index = y * bytes_per_row + (x / 8);

    // MSB-left. If your hardware is LSB-left, use: (1u << (x % 8))
    const uint8_t bit = (uint8_t)(1u << (7 - (x % 8)));

    fb[byte_index] |= bit;
}

static int get_pixel(const uint8_t fb[FB_SIZE], int x, int y)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return 0;

    const int bytes_per_row = FB_WIDTH / 8;
    const int byte_index = y * bytes_per_row + (x / 8);
    const uint8_t bit = (uint8_t)(1u << (7 - (x % 8)));
    return (fb[byte_index] & bit) ? 1 : 0;
}

static void draw_hline(uint8_t fb[FB_SIZE], int x0, int x1, int y)
{
    if (y < 0 || y >= FB_HEIGHT) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x1 < 0 || x0 >= FB_WIDTH) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= FB_WIDTH) x1 = FB_WIDTH - 1;

    for (int x = x0; x <= x1; x++)
        set_pixel(fb, x, y);
}

static void draw_vline(uint8_t fb[FB_SIZE], int x, int y0, int y1)
{
    if (x < 0 || x >= FB_WIDTH) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y1 < 0 || y0 >= FB_HEIGHT) return;
    if (y0 < 0) y0 = 0;
    if (y1 >= FB_HEIGHT) y1 = FB_HEIGHT - 1;

    for (int y = y0; y <= y1; y++)
        set_pixel(fb, x, y);
}

static void draw_rect(uint8_t fb[FB_SIZE], int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    draw_hline(fb, x, x + w - 1, y);
    draw_hline(fb, x, x + w - 1, y + h - 1);
    draw_vline(fb, x, y, y + h - 1);
    draw_vline(fb, x + w - 1, y, y + h - 1);
}

static void draw_filled_rect(uint8_t fb[FB_SIZE], int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++)
        draw_hline(fb, x, x + w - 1, yy);
}

// Simple filled circle (small radius) for note head
static void draw_filled_circle(uint8_t fb[FB_SIZE], int cx, int cy, int r)
{
    for (int dy = -r; dy <= r; dy++)
    {
        for (int dx = -r; dx <= r; dx++)
        {
            if (dx*dx + dy*dy <= r*r)
                set_pixel(fb, cx + dx, cy + dy);
        }
    }
}

// ---------------------
// Minimal 5x7 font (only needed chars)
// glyph data = 5 columns, each column is 7 bits (LSB = top)
// We plot bits using set_pixel so framebuffer layout doesn't matter.
// ---------------------
typedef struct {
    char c;
    uint8_t col[5];
} glyph_t;

static const glyph_t g_font[] = {
    { 'T', {0x01,0x01,0x7F,0x01,0x01} },
    { 'H', {0x7F,0x08,0x08,0x08,0x7F} },
    { 'E', {0x7F,0x49,0x49,0x49,0x41} },
    { 'B', {0x7F,0x49,0x49,0x49,0x36} },
    { 'U', {0x3F,0x40,0x40,0x40,0x3F} },
    { 'T', {0x01,0x01,0x7F,0x01,0x01} }, // repeated ok
    { 'O', {0x3E,0x41,0x41,0x41,0x3E} },
    { 'N', {0x7F,0x02,0x04,0x08,0x7F} },
    { ' ', {0x00,0x00,0x00,0x00,0x00} },
};

static const uint8_t *font_get(char c)
{
    for (unsigned i = 0; i < sizeof(g_font)/sizeof(g_font[0]); i++)
        if (g_font[i].c == c)
            return g_font[i].col;
    // fallback to space
    return g_font[sizeof(g_font)/sizeof(g_font[0]) - 1].col;
}

static void draw_char_5x7(uint8_t fb[FB_SIZE], int x, int y, char c)
{
    const uint8_t *col = font_get(c);
    for (int cx = 0; cx < 5; cx++)
    {
        uint8_t bits = col[cx];
        for (int cy = 0; cy < 7; cy++)
        {
            if (bits & (1u << cy))
                set_pixel(fb, x + cx, y + cy);
        }
    }
}

static void draw_text_5x7(uint8_t fb[FB_SIZE], int x, int y, const char *s)
{
    int pen = x;
    while (*s)
    {
        draw_char_5x7(fb, pen, y, *s);
        pen += 6; // 5 px glyph + 1 px spacing
        s++;
    }
}

static int text_width_5x7(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n * 6 - 1; // last char doesn't need trailing space
}

// ---------------------
// Eighth note icon
// A simple note head + stem + flag.
// ---------------------
static void draw_eighth_note(uint8_t fb[FB_SIZE], int x, int y)
{
    // (x, y) is the top-left of the icon box roughly
    // Note head center
    const int head_cx = x + 6;
    const int head_cy = y + 14;

    draw_filled_circle(fb, head_cx, head_cy, 3);

    // Stem (go up from right side of head)
    const int stem_x = head_cx + 3;
    draw_vline(fb, stem_x, head_cy - 18, head_cy - 2);

    // Flag (a small curve-ish triangle made of lines)
    int fx = stem_x;
    int fy = head_cy - 18;

    // A little “flag” pointing right
    draw_hline(fb, fx, fx + 7, fy + 1);
    draw_hline(fb, fx + 1, fx + 7, fy + 2);
    draw_hline(fb, fx + 2, fx + 6, fy + 3);
    draw_hline(fb, fx + 3, fx + 6, fy + 4);
}

void draw_splash(uint8_t fb[FB_SIZE])
{
    fb_clear(fb);

    // Double border + corner accents
    draw_rect(fb, 0, 0, FB_WIDTH, FB_HEIGHT);
    draw_rect(fb, 2, 2, FB_WIDTH - 4, FB_HEIGHT - 4);
    draw_filled_rect(fb, 0, 0, 3, 3);
    draw_filled_rect(fb, FB_WIDTH - 3, 0, 3, 3);
    draw_filled_rect(fb, 0, FB_HEIGHT - 3, 3, 3);
    draw_filled_rect(fb, FB_WIDTH - 3, FB_HEIGHT - 3, 3, 3);

    // Top title plate
    const char *title = "THE BUTTON";
    const int tw = text_width_5x7(title);
    const int plate_w = tw + 8;
    const int plate_x = (FB_WIDTH - plate_w) / 2;
    const int plate_y = 7;
    draw_rect(fb, plate_x, plate_y, plate_w, 11);
    draw_text_5x7(fb, plate_x + 4, plate_y + 2, title);

    // Center "button" icon
    const int cx = FB_WIDTH / 2;
    const int cy = 36;
    draw_filled_circle(fb, cx, cy, 10);
    draw_filled_circle(fb, cx, cy, 7);
    draw_filled_circle(fb, cx, cy, 4);
    draw_hline(fb, cx - 5, cx + 5, cy);
    draw_vline(fb, cx, cy - 5, cy + 5);

    // Side music bars
    const int bar_y = 30;
    draw_filled_rect(fb, 18, bar_y + 8, 3, 10);
    draw_filled_rect(fb, 23, bar_y + 4, 3, 14);
    draw_filled_rect(fb, 28, bar_y + 1, 3, 17);
    draw_filled_rect(fb, 97, bar_y + 1, 3, 17);
    draw_filled_rect(fb, 102, bar_y + 4, 3, 14);
    draw_filled_rect(fb, 107, bar_y + 8, 3, 10);

    // Notes for the music theme
    draw_eighth_note(fb, 8, 20);
    draw_eighth_note(fb, FB_WIDTH - 23, 20);

    // Bottom accent line
    draw_hline(fb, 14, FB_WIDTH - 15, FB_HEIGHT - 9);
}

#ifndef LCD_DIAGONAL_INCHES
#define LCD_DIAGONAL_INCHES 0.96f
#endif

#ifndef FALLBACK_HOST_PPI
#define FALLBACK_HOST_PPI 110.0f
#endif

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
    typedef enum {
        APP_SPLASH = 0,
        APP_MENU,
        APP_PLAY
    } app_state_t;

    uint8_t framebuffer[FB_SIZE];
    uint8_t splash_fb[FB_SIZE];
    draw_splash(splash_fb);
    menu_state_t menu_state;
    menu_init(&menu_state);
    play_state_t play_state;
    memset(&play_state, 0, sizeof(play_state));
    app_state_t app_state = APP_SPLASH;

    const float aspect = (float)FB_WIDTH / (float)FB_HEIGHT;
    const float lcd_h_inches = LCD_DIAGONAL_INCHES / sqrtf((aspect * aspect) + 1.0f);
    const float lcd_w_inches = lcd_h_inches * aspect;

    const int monitor = 0;
    const float host_ppi = get_monitor_ppi(monitor);

    const int win_w = (int)roundf(lcd_w_inches * host_ppi);
    const int win_h = (int)roundf(lcd_h_inches * host_ppi);

    InitWindow((win_w > 1) ? win_w : 1, (win_h > 1) ? win_h : 1, "128x64 1bpp Framebuffer");
    SetTargetFPS(60);

    Image img = GenImageColor(FB_WIDTH, FB_HEIGHT, BLACK);
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    intro_audio_start();

    Color pixels[FB_WIDTH * FB_HEIGHT];
    double last_print_time = GetTime();
    double splash_start_time = GetTime();
    int prev_select_down = 0;

    while (!WindowShouldClose())
    {
        const double now = GetTime();
        const input_poll_t in = input_poll();
        const int select_down = (in.rot_down || in.play_down) ? 1 : 0;
        const int select_pressed = (select_down && !prev_select_down) ? 1 : 0;
        prev_select_down = select_down;

        if ((now - last_print_time) >= 0.1)
        {
            printf("state_poll rot_dl=%d rot_dr=%d rot_down=%d play_down=%d\n",
                   in.rot_dl, in.rot_dr, in.rot_down, in.play_down);
            fflush(stdout);
            last_print_time = now;
        }

        if (app_state == APP_SPLASH)
        {
            const double elapsed = now - splash_start_time;
            if (elapsed < 3.0)
            {
                const float t = (float)(elapsed / 3.0);
                draw_intro_animation(framebuffer, splash_fb, FB_WIDTH, FB_HEIGHT, t);
            }
            else
            {
                fb_copy(framebuffer, splash_fb);
            }

            if (select_pressed)
            {
                intro_audio_stop();
                menu_init(&menu_state);
                menu_state.prev_select_down = select_down;
                app_state = APP_MENU;
            }
        }
        else if (app_state == APP_MENU)
        {
            const menu_action_t action = menu_update(&menu_state, in);
            write_menu(framebuffer, FB_WIDTH, FB_HEIGHT, &menu_state);

            if (action == MENU_ACTION_DEMO)
            {
                play_init(&play_state);
                app_state = APP_PLAY;
            }
            else if (action == MENU_ACTION_EXIT)
            {
                app_state = APP_SPLASH;
                splash_start_time = now;
                intro_audio_start();
            }
        }
        else
        {
            const play_action_t play_action = play_update(&play_state, in, GetFrameTime(), FB_WIDTH, FB_HEIGHT);
            write_play(framebuffer, FB_WIDTH, FB_HEIGHT, &play_state);

            if (play_action == PLAY_ACTION_EXIT_TO_MENU)
            {
                play_deinit(&play_state);
                app_state = APP_MENU;
            }
        }

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
    intro_audio_stop();
    UnloadTexture(tex);
    CloseWindow();
    return 0;
}
