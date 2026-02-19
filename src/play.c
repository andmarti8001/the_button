#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "play.h"

#ifndef PLAY_DEBUG_OUTLINE
#define PLAY_DEBUG_OUTLINE 1
#endif

#ifndef PLAY_SCROLL_STEP_SECONDS
#define PLAY_SCROLL_STEP_SECONDS 0.03f
#endif

#define PLAY_MIN_BPM 40
#define PLAY_MAX_BPM 300

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

static void draw_filled_rect(uint8_t *fb, int width, int height, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < (y + h); yy++)
        draw_hline(fb, width, height, x, x + w - 1, yy);
}

static void draw_dotted_hline(uint8_t *fb, int width, int height, int x0, int x1, int y)
{
    if (y < 0 || y >= height) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    x0 = clampi(x0, 0, width - 1);
    x1 = clampi(x1, 0, width - 1);
    for (int x = x0; x <= x1; x += 2)
        set_pixel(fb, width, height, x, y);
}

static void draw_dotted_rect(uint8_t *fb, int width, int height, int x, int y, int w, int h)
{
    if (w <= 1 || h <= 1) return;
    const int x1 = x + w - 1;
    const int y1 = y + h - 1;
    for (int xx = x; xx <= x1; xx += 2)
    {
        set_pixel(fb, width, height, xx, y);
        set_pixel(fb, width, height, xx, y1);
    }
    for (int yy = y; yy <= y1; yy += 2)
    {
        set_pixel(fb, width, height, x, yy);
        set_pixel(fb, width, height, x1, yy);
    }
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
    static const uint8_t g_b[5] = {0x7F,0x49,0x49,0x49,0x36};
    static const uint8_t g_d[5] = {0x7F,0x41,0x41,0x22,0x1C};
    static const uint8_t g_e[5] = {0x7F,0x49,0x49,0x49,0x41};
    static const uint8_t g_m[5] = {0x7F,0x20,0x10,0x20,0x7F};
    static const uint8_t g_o[5] = {0x3E,0x41,0x41,0x41,0x3E};
    static const uint8_t g_p[5] = {0x7F,0x48,0x48,0x48,0x30};
    static const uint8_t g_x[5] = {0x63,0x14,0x08,0x14,0x63};
    static const uint8_t g_lbr[5] = {0x00,0x7F,0x41,0x41,0x00};
    static const uint8_t g_rbr[5] = {0x00,0x41,0x41,0x7F,0x00};

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
        case 'B': return g_b;
        case 'D': return g_d;
        case 'E': return g_e;
        case 'M': return g_m;
        case 'O': return g_o;
        case 'P': return g_p;
        case 'X': return g_x;
        case '[': return g_lbr;
        case ']': return g_rbr;
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

static int note_to_row(const play_state_t *state, int note, int play_h)
{
    if (play_h <= 1)
        return 0;
    if (state->note_max <= state->note_min)
        return (play_h - 1) / 2;

    const int num = (state->note_max - note) * (play_h - 1);
    const int den = (state->note_max - state->note_min);
    return clampi(num / den, 0, play_h - 1);
}

static uint64_t build_present_mask(const play_state_t *state, int play_h, int play_down)
{
    uint64_t mask = 0;
    if (!play_down || !state->melody_loaded || state->melody.group_count <= 0)
        return 0;

    const midi_note_group_t *g = &state->melody.groups[state->step_index];
    for (int i = 0; i < g->note_count; i++)
    {
        const int row = note_to_row(state, g->notes[i], play_h);
        if (row >= 0 && row < 64)
            mask |= ((uint64_t)1u << row);
    }
    return mask;
}

static void push_history(play_state_t *state, uint64_t mask, int width)
{
    const int w = clampi(width, 1, 128);
    if (state->history_width != w)
    {
        state->history_width = w;
        memset(state->history_cols, 0, sizeof(state->history_cols));
    }

    memmove(&state->history_cols[0], &state->history_cols[1], (size_t)(w - 1) * sizeof(uint64_t));
    state->history_cols[w - 1] = mask;
}

static void analyze_melody_range(play_state_t *state)
{
    int min_note = 127;
    int max_note = 0;
    int found = 0;

    for (int i = 0; i < state->melody.group_count; i++)
    {
        const midi_note_group_t *g = &state->melody.groups[i];
        for (int n = 0; n < g->note_count; n++)
        {
            const int note = g->notes[n];
            if (note < min_note) min_note = note;
            if (note > max_note) max_note = note;
            found = 1;
        }
    }

    if (!found)
    {
        state->note_min = 60;
        state->note_max = 72;
    }
    else
    {
        state->note_min = min_note;
        state->note_max = max_note;
    }
}

void play_init(play_state_t *state)
{
    play_deinit(state);
    state->bpm = 120;
    state->selected = 0;
    state->select_mode = 0;
    state->is_playing = 1;
    state->prev_rot_down = 0;
    state->prev_play_down = 0;
    state->note_min = 60;
    state->note_max = 72;
    state->step_index = 0;
    state->scroll_accum = 0.0f;
    state->history_width = 0;
    memset(state->history_cols, 0, sizeof(state->history_cols));

    if (midi_extract_track_notes("songs/demo.mid", "lead", &state->melody) == 0 && state->melody.group_count > 0)
    {
        state->melody_loaded = 1;
        analyze_melody_range(state);
    }
    else
    {
        state->melody_loaded = 0;
    }
}

void play_deinit(play_state_t *state)
{
    if (state->melody_loaded)
        midi_free_note_sequence(&state->melody);
    memset(&state->melody, 0, sizeof(state->melody));
    state->melody_loaded = 0;
}

play_action_t play_update(play_state_t *state, input_poll_t in, float dt_seconds, int width, int height)
{
    const int rot_pressed = (in.rot_down && !state->prev_rot_down) ? 1 : 0;
    const int play_pressed = (in.play_down && !state->prev_play_down) ? 1 : 0;
    const int play_released = (!in.play_down && state->prev_play_down) ? 1 : 0;
    state->prev_rot_down = in.rot_down ? 1 : 0;
    state->prev_play_down = in.play_down ? 1 : 0;

    if (state->select_mode)
    {
        state->bpm -= in.rot_dl;
        state->bpm += in.rot_dr;
        state->bpm = clampi(state->bpm, PLAY_MIN_BPM, PLAY_MAX_BPM);

        if (rot_pressed)
        {
            state->select_mode = 0;
            state->is_playing = 1; // "restart play session"
        }
    }
    else
    {
        state->selected += in.rot_dr;
        state->selected -= in.rot_dl;
        state->selected = clampi(state->selected, 0, 1);

        if (rot_pressed)
        {
            if (state->selected == 0)
            {
                state->is_playing = 0;   // "turn the song off"
                state->select_mode = 1;  // enter BPM edit mode
            }
            else
            {
                return PLAY_ACTION_EXIT_TO_MENU;
            }
        }
    }

    if (state->melody_loaded && state->melody.group_count > 0)
    {
        if (play_pressed)
            state->is_playing = 1;

        if (play_released)
        {
            state->step_index++;
            if (state->step_index >= state->melody.group_count)
                state->step_index = 0;
        }
    }

    {
        const int top_h = (height * 20) / 100;
        const int play_h = height - top_h;
        uint64_t present_mask = build_present_mask(state, play_h, in.play_down);
        state->scroll_accum += dt_seconds;

        while (state->scroll_accum >= PLAY_SCROLL_STEP_SECONDS)
        {
            push_history(state, present_mask, width);
            state->scroll_accum -= PLAY_SCROLL_STEP_SECONDS;
        }
    }

    return PLAY_ACTION_NONE;
}

void write_play(uint8_t *fb, int width, int height, const play_state_t *state)
{
    char bpm_label[24];
    const int top_h = (height * 20) / 100;
    const int play_y = top_h;
    const int play_h = height - play_y;
    const int demo_y = (top_h - 7) / 2;
    const int top_mid_y = demo_y;

    memset(fb, 0, (size_t)((width * height) / 8));
    draw_rect(fb, width, height, 0, 0, width, height);
    draw_hline(fb, width, height, 0, width - 1, play_y);

#if PLAY_DEBUG_OUTLINE
    draw_dotted_rect(fb, width, height, 0, play_y, width, play_h);
#endif

    draw_text_5x7(fb, width, height, 3, demo_y, "DEMO");

    if (state->selected == 0)
        snprintf(bpm_label, sizeof(bpm_label), "[%d BPM]", state->bpm);
    else
        snprintf(bpm_label, sizeof(bpm_label), "%d BPM", state->bpm);
    draw_text_5x7(fb, width, height, (width - text_width_5x7(bpm_label)) / 2, top_mid_y, bpm_label);

    if (state->selected == 1)
        draw_text_5x7(fb, width, height, width - text_width_5x7("[X]") - 3, top_mid_y, "[X]");
    else
        draw_text_5x7(fb, width, height, width - text_width_5x7("X") - 3, top_mid_y, "X");

    // Historical play-state scroll, right-to-left.
    {
        const int w = clampi(state->history_width, 0, width);
        for (int x = 0; x < w; x++)
        {
            const uint64_t col = state->history_cols[x];
            for (int row = 0; row < play_h && row < 64; row++)
            {
                if (col & ((uint64_t)1u << row))
                    set_pixel(fb, width, height, x, play_y + row);
            }
        }
    }

    // Optional melody range guides (dotted).
    if (state->melody_loaded && state->note_max >= state->note_min)
    {
        const int top_row = note_to_row(state, state->note_max, play_h);
        const int bot_row = note_to_row(state, state->note_min, play_h);
        draw_dotted_hline(fb, width, height, 0, width - 1, play_y + top_row);
        draw_dotted_hline(fb, width, height, 0, width - 1, play_y + bot_row);
    }

    // Right wall "present state" arrow indicator on active note lines.
    if (state->melody_loaded && state->melody.group_count > 0 && state->prev_play_down)
    {
        const uint64_t mask = build_present_mask(state, play_h, 1);
        for (int row = 0; row < play_h && row < 64; row++)
        {
            if (mask & ((uint64_t)1u << row))
            {
                const int y = play_y + row;
                set_pixel(fb, width, height, width - 1, y);
                set_pixel(fb, width, height, width - 2, y - 1);
                set_pixel(fb, width, height, width - 2, y);
                set_pixel(fb, width, height, width - 2, y + 1);
                set_pixel(fb, width, height, width - 3, y);
            }
        }
    }

    if (state->is_playing)
        draw_filled_rect(fb, width, height, 2, play_y + 2, 3, 3);
    else
        draw_rect(fb, width, height, 2, play_y + 2, 3, 3);
}
