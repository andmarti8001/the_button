#include <stdint.h>
#include <string.h>
#include "menu.h"

static const char *k_menu_items[] = {
    "DEMO",
    "EXIT"
};

#define MENU_ITEM_COUNT ((int)(sizeof(k_menu_items) / sizeof(k_menu_items[0])))

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

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
    x0 = clampi(x0, 0, width - 1);
    x1 = clampi(x1, 0, width - 1);
    for (int x = x0; x <= x1; x++)
        set_pixel(fb, width, height, x, y);
}

static void draw_vline(uint8_t *fb, int width, int height, int x, int y0, int y1)
{
    if (x < 0 || x >= width) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    y0 = clampi(y0, 0, height - 1);
    y1 = clampi(y1, 0, height - 1);
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
    static const uint8_t g_0[5] = {0x3E,0x45,0x49,0x51,0x3E};
    static const uint8_t g_1[5] = {0x00,0x21,0x7F,0x01,0x00};
    static const uint8_t g_2[5] = {0x23,0x45,0x49,0x51,0x21};
    static const uint8_t g_3[5] = {0x22,0x41,0x49,0x49,0x36};
    static const uint8_t g_4[5] = {0x18,0x28,0x48,0x7F,0x08};
    static const uint8_t g_5[5] = {0x72,0x51,0x51,0x51,0x4E};
    static const uint8_t g_6[5] = {0x1E,0x29,0x49,0x49,0x06};
    static const uint8_t g_7[5] = {0x40,0x47,0x48,0x50,0x60};
    static const uint8_t g_8[5] = {0x36,0x49,0x49,0x49,0x36};
    static const uint8_t g_9[5] = {0x30,0x49,0x49,0x4A,0x3C};
    static const uint8_t g_a[5] = {0x1F,0x24,0x44,0x24,0x1F};
    static const uint8_t g_c[5] = {0x1C,0x22,0x41,0x41,0x22};
    static const uint8_t g_d[5] = {0x7F,0x41,0x41,0x22,0x1C};
    static const uint8_t g_e[5] = {0x7F,0x49,0x49,0x49,0x41};
    static const uint8_t g_h[5] = {0x7F,0x08,0x08,0x08,0x7F};
    static const uint8_t g_i[5] = {0x00,0x41,0x7F,0x41,0x00};
    static const uint8_t g_l[5] = {0x7F,0x01,0x01,0x01,0x01};
    static const uint8_t g_m[5] = {0x7F,0x20,0x10,0x20,0x7F};
    static const uint8_t g_n[5] = {0x7F,0x10,0x08,0x04,0x7F};
    static const uint8_t g_o[5] = {0x3E,0x41,0x41,0x41,0x3E};
    static const uint8_t g_p[5] = {0x7F,0x48,0x48,0x48,0x30};
    static const uint8_t g_r[5] = {0x7F,0x48,0x4C,0x4A,0x31};
    static const uint8_t g_t[5] = {0x40,0x40,0x7F,0x40,0x40};
    static const uint8_t g_u[5] = {0x7E,0x01,0x01,0x01,0x7E};
    static const uint8_t g_x[5] = {0x63,0x14,0x08,0x14,0x63};

    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');

    switch (c)
    {
        case '0': return g_0;
        case '1': return g_1;
        case '2': return g_2;
        case '3': return g_3;
        case '4': return g_4;
        case '5': return g_5;
        case '6': return g_6;
        case '7': return g_7;
        case '8': return g_8;
        case '9': return g_9;
        case 'A': return g_a;
        case 'C': return g_c;
        case 'D': return g_d;
        case 'E': return g_e;
        case 'H': return g_h;
        case 'I': return g_i;
        case 'L': return g_l;
        case 'M': return g_m;
        case 'N': return g_n;
        case 'O': return g_o;
        case 'P': return g_p;
        case 'R': return g_r;
        case 'T': return g_t;
        case 'U': return g_u;
        case 'X': return g_x;
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

void menu_init(menu_state_t *state)
{
    state->selected = 0;
    state->scroll_top = 0;
    state->prev_select_down = 0;
}

menu_action_t menu_update(menu_state_t *state, input_poll_t in)
{
    for (int i = 0; i < in.rot_dl; i++)
        state->selected--;
    for (int i = 0; i < in.rot_dr; i++)
        state->selected++;

    state->selected = clampi(state->selected, 0, MENU_ITEM_COUNT - 1);

    const int list_y = 16;
    const int row_h = 9;
    const int visible_rows = clampi((64 - list_y - 2) / row_h, 1, MENU_ITEM_COUNT);

    if (state->selected < state->scroll_top)
        state->scroll_top = state->selected;
    if (state->selected >= (state->scroll_top + visible_rows))
        state->scroll_top = state->selected - visible_rows + 1;

    const int max_top = MENU_ITEM_COUNT - visible_rows;
    state->scroll_top = clampi(state->scroll_top, 0, (max_top > 0) ? max_top : 0);

    const int select_down = (in.rot_down || in.play_down) ? 1 : 0;
    const int select_pressed = (select_down && !state->prev_select_down) ? 1 : 0;
    state->prev_select_down = select_down;

    if (!select_pressed)
        return MENU_ACTION_NONE;

    if (state->selected == 0)
        return MENU_ACTION_DEMO;
    if (state->selected == 1)
        return MENU_ACTION_EXIT;
    return MENU_ACTION_NONE;
}

void write_menu(uint8_t *fb, int width, int height, const menu_state_t *state)
{
    memset(fb, 0, (size_t)((width * height) / 8));
    draw_rect(fb, width, height, 0, 0, width, height);
    draw_text_5x7(fb, width, height, (width - text_width_5x7("MENU")) / 2, 4, "MENU");

    const int list_x = 8;
    const int list_y = 16;
    const int row_h = 9;
    const int visible_rows = clampi((height - list_y - 2) / row_h, 1, MENU_ITEM_COUNT);

    for (int row = 0; row < visible_rows; row++)
    {
        const int idx = state->scroll_top + row;
        if (idx >= MENU_ITEM_COUNT)
            break;

        const int y = list_y + row * row_h;
        const char *label = k_menu_items[idx];
        draw_text_5x7(fb, width, height, list_x, y, label);

        if (idx == state->selected)
        {
            const int tw = text_width_5x7(label);
            draw_hline(fb, width, height, list_x, list_x + tw, y + 8);
        }
    }
}
