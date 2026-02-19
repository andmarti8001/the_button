#include <stdint.h>
#include <string.h>
#include "play.h"

static void set_pixel(uint8_t *fb, int width, int height, int x, int y)
{
    if (x < 0 || x >= width || y < 0 || y >= height)
        return;

    const int bytes_per_row = width / 8;
    const int byte_index = y * bytes_per_row + (x / 8);
    const uint8_t bit = (uint8_t)(1u << (7 - (x % 8)));
    fb[byte_index] |= bit;
}

static void draw_hline(uint8_t *fb, int width, int height, int x0, int x1, int y)
{
    if (y < 0 || y >= height) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= width) x1 = width - 1;
    for (int x = x0; x <= x1; x++)
        set_pixel(fb, width, height, x, y);
}

static void draw_vline(uint8_t *fb, int width, int height, int x, int y0, int y1)
{
    if (x < 0 || x >= width) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= height) y1 = height - 1;
    for (int y = y0; y <= y1; y++)
        set_pixel(fb, width, height, x, y);
}

static void draw_rect(uint8_t *fb, int width, int height, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    draw_hline(fb, width, height, x, x + w - 1, y);
    draw_hline(fb, width, height, x, x + w - 1, y + h - 1);
    draw_vline(fb, width, height, x, y, y + h - 1);
    draw_vline(fb, width, height, x + w - 1, y, y + h - 1);
}

static const uint8_t *glyph_5x7(char c)
{
    static const uint8_t g_space[5] = {0x00,0x00,0x00,0x00,0x00};
    static const uint8_t g_p[5] = {0x7F,0x48,0x48,0x48,0x30};
    static const uint8_t g_l[5] = {0x7F,0x01,0x01,0x01,0x01};
    static const uint8_t g_a[5] = {0x1F,0x24,0x44,0x24,0x1F};
    static const uint8_t g_y[5] = {0x70,0x08,0x07,0x08,0x70};

    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');

    switch (c)
    {
        case 'P': return g_p;
        case 'L': return g_l;
        case 'A': return g_a;
        case 'Y': return g_y;
        default: return g_space;
    }
}

static void draw_char_5x7(uint8_t *fb, int width, int height, int x, int y, char c)
{
    const uint8_t *col = glyph_5x7(c);
    for (int cx = 0; cx < 5; cx++)
    {
        const uint8_t bits = col[cx];
        for (int cy = 0; cy < 7; cy++)
        {
            if (bits & (1u << (6 - cy)))
                set_pixel(fb, width, height, x + cx, y + cy);
        }
    }
}

static void draw_text_5x7(uint8_t *fb, int width, int height, int x, int y, const char *s)
{
    int pen = x;
    while (*s)
    {
        draw_char_5x7(fb, width, height, pen, y, *s);
        pen += 6;
        s++;
    }
}

static int text_width_5x7(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return (n > 0) ? (n * 6 - 1) : 0;
}

void write_play(uint8_t *fb, int width, int height)
{
    memset(fb, 0, (size_t)((width * height) / 8));
    draw_rect(fb, width, height, 0, 0, width, height);
    draw_text_5x7(fb, width, height, (width - text_width_5x7("PLAY")) / 2, (height - 7) / 2, "PLAY");
}
