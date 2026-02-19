#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <raylib.h>
#include "play.h"
#include "synth.h"

#ifndef PLAY_DEBUG_OUTLINE
#define PLAY_DEBUG_OUTLINE 1
#endif

#ifndef PLAY_SCROLL_STEP_SECONDS
#define PLAY_SCROLL_STEP_SECONDS 0.03f
#endif

#ifndef PLAY_AUDIO_SAMPLE_RATE
#define PLAY_AUDIO_SAMPLE_RATE 48000
#endif

#define PLAY_MIN_BPM 40
#define PLAY_MAX_BPM 300

typedef struct {
    midi_track_events_t midi;
    synth_t synth;
    synth_wave_t wave;
    float drive;
    float gain;
    int cursor;
    int loaded;
} backing_track_t;

static AudioStream g_play_stream;
static synth_t g_lead_synth;
static backing_track_t g_backing_softp;
static backing_track_t g_backing_hardp;
static backing_track_t g_backing_bass;
static uint32_t g_song_tick_q16 = 0;
static int g_song_tpq = 480;
static int g_play_stream_loaded = 0;
static _Atomic int g_audio_cmd = 0;
static _Atomic int g_note_count = 0;
static _Atomic int g_note0 = 60;
static _Atomic int g_note1 = 64;
static _Atomic int g_backing_paused = 0;
static _Atomic int g_backing_restart = 0;
static _Atomic int g_bpm_atomic = 120;

enum {
    PLAY_AUDIO_CMD_NONE = 0,
    PLAY_AUDIO_CMD_NOTE_ON,
    PLAY_AUDIO_CMD_NOTE_OFF,
    PLAY_AUDIO_CMD_ALL_OFF
};

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void derive_song_title(char out[SONG_NAME_MAX], const char *path)
{
    const char *name = path;
    int j = 0;
    for (const char *p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
            name = p + 1;
    }
    for (int i = 0; name[i] && j < (SONG_NAME_MAX - 1); i++)
    {
        char c = name[i];
        if (c == '.') break;
        if (c == '_' || c == '-') c = ' ';
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[j++] = c;
    }
    out[j] = '\0';
    if (out[0] == '\0')
        snprintf(out, SONG_NAME_MAX, "SONG");
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
    static const uint8_t g_c[5] = {0x3E,0x41,0x41,0x41,0x22};
    static const uint8_t g_d[5] = {0x7F,0x41,0x41,0x22,0x1C};
    static const uint8_t g_e[5] = {0x7F,0x49,0x49,0x49,0x41};
    static const uint8_t g_f[5] = {0x7F,0x48,0x48,0x48,0x40};
    static const uint8_t g_g[5] = {0x3E,0x41,0x45,0x45,0x26};
    static const uint8_t g_h[5] = {0x7F,0x08,0x08,0x08,0x7F};
    static const uint8_t g_i[5] = {0x00,0x41,0x7F,0x41,0x00};
    static const uint8_t g_j[5] = {0x02,0x01,0x01,0x01,0x7E};
    static const uint8_t g_k[5] = {0x7F,0x08,0x14,0x22,0x41};
    static const uint8_t g_l[5] = {0x7F,0x01,0x01,0x01,0x01};
    static const uint8_t g_m[5] = {0x7F,0x20,0x10,0x20,0x7F};
    static const uint8_t g_n[5] = {0x7F,0x10,0x08,0x04,0x7F};
    static const uint8_t g_o[5] = {0x3E,0x41,0x41,0x41,0x3E};
    static const uint8_t g_p[5] = {0x7F,0x48,0x48,0x48,0x30};
    static const uint8_t g_q[5] = {0x3E,0x41,0x45,0x42,0x3D};
    static const uint8_t g_r[5] = {0x7F,0x48,0x4C,0x4A,0x31};
    static const uint8_t g_s[5] = {0x31,0x49,0x49,0x49,0x46};
    static const uint8_t g_t[5] = {0x40,0x40,0x7F,0x40,0x40};
    static const uint8_t g_u[5] = {0x7E,0x01,0x01,0x01,0x7E};
    static const uint8_t g_v[5] = {0x7C,0x02,0x01,0x02,0x7C};
    static const uint8_t g_w[5] = {0x7E,0x01,0x0E,0x01,0x7E};
    static const uint8_t g_x[5] = {0x63,0x14,0x08,0x14,0x63};
    static const uint8_t g_y[5] = {0x70,0x08,0x07,0x08,0x70};
    static const uint8_t g_z[5] = {0x43,0x45,0x49,0x51,0x61};
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
        case 'C': return g_c;
        case 'D': return g_d;
        case 'E': return g_e;
        case 'F': return g_f;
        case 'G': return g_g;
        case 'H': return g_h;
        case 'I': return g_i;
        case 'J': return g_j;
        case 'K': return g_k;
        case 'L': return g_l;
        case 'M': return g_m;
        case 'N': return g_n;
        case 'O': return g_o;
        case 'P': return g_p;
        case 'Q': return g_q;
        case 'R': return g_r;
        case 'S': return g_s;
        case 'T': return g_t;
        case 'U': return g_u;
        case 'V': return g_v;
        case 'W': return g_w;
        case 'X': return g_x;
        case 'Y': return g_y;
        case 'Z': return g_z;
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

    // 10% vertical padding inside play area: 5% top + 5% bottom.
    const int pad = clampi(play_h / 20, 1, (play_h - 1) / 2);
    const int inner_top = pad;
    const int inner_bottom = play_h - 1 - pad;
    const int inner_h = inner_bottom - inner_top + 1;

    if (inner_h <= 1 || state->note_max <= state->note_min)
        return clampi((play_h - 1) / 2, 0, play_h - 1);

    const int num = (state->note_max - note) * (inner_h - 1);
    const int den = (state->note_max - state->note_min);
    return clampi(inner_top + (num / den), 0, play_h - 1);
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

static void backing_track_reset(backing_track_t *trk)
{
    trk->cursor = 0;
    synth_all_notes_off(&trk->synth);
}

static void backing_reset_all(void)
{
    g_song_tick_q16 = 0;
    backing_track_reset(&g_backing_softp);
    backing_track_reset(&g_backing_hardp);
    backing_track_reset(&g_backing_bass);
}

static void backing_process_events(backing_track_t *trk, uint32_t tick)
{
    while (trk->loaded && trk->cursor < trk->midi.event_count && trk->midi.events[trk->cursor].tick <= tick)
    {
        const midi_note_event_t *e = &trk->midi.events[trk->cursor];
        if (e->is_note_on)
            synth_note_on(&trk->synth, e->note, trk->wave, trk->drive);
        else
            synth_note_off(&trk->synth, e->note);
        trk->cursor++;
    }
}

static int backing_all_finished(void)
{
    const int soft_done = (!g_backing_softp.loaded) || (g_backing_softp.cursor >= g_backing_softp.midi.event_count);
    const int hard_done = (!g_backing_hardp.loaded) || (g_backing_hardp.cursor >= g_backing_hardp.midi.event_count);
    const int bass_done = (!g_backing_bass.loaded) || (g_backing_bass.cursor >= g_backing_bass.midi.event_count);
    return soft_done && hard_done && bass_done;
}

static void play_audio_callback(void *buffer_data, unsigned int frames)
{
    float *out = (float *)buffer_data;
    const int cmd = atomic_exchange(&g_audio_cmd, PLAY_AUDIO_CMD_NONE);
    const int paused = atomic_load(&g_backing_paused);
    const int bpm = atomic_load(&g_bpm_atomic);

    if (cmd == PLAY_AUDIO_CMD_NOTE_ON)
    {
        uint8_t notes[2];
        int count = atomic_load(&g_note_count);
        if (count < 0) count = 0;
        if (count > 2) count = 2;
        notes[0] = (uint8_t)atomic_load(&g_note0);
        notes[1] = (uint8_t)atomic_load(&g_note1);
        synth_all_notes_off(&g_lead_synth);
        for (int i = 0; i < count; i++)
            synth_note_on(&g_lead_synth, notes[i], SYNTH_WAVE_TRIANGLE, 1.0f);
    }
    else if (cmd == PLAY_AUDIO_CMD_NOTE_OFF)
    {
        synth_all_notes_off(&g_lead_synth);
    }
    else if (cmd == PLAY_AUDIO_CMD_ALL_OFF)
    {
        synth_all_notes_off(&g_lead_synth);
        synth_all_notes_off(&g_backing_softp.synth);
        synth_all_notes_off(&g_backing_hardp.synth);
        synth_all_notes_off(&g_backing_bass.synth);
    }

    if (atomic_exchange(&g_backing_restart, 0))
        backing_reset_all();

    for (unsigned int i = 0; i < frames; i++)
    {
        float mixed = 0.0f;
        mixed += synth_next_sample(&g_lead_synth) * 0.90f;

        if (!paused && g_song_tpq > 0 && bpm > 0)
        {
            const float ticks_per_sample = ((float)bpm * (float)g_song_tpq) / (60.0f * (float)PLAY_AUDIO_SAMPLE_RATE);
            const uint32_t delta_q16 = (uint32_t)(ticks_per_sample * 65536.0f);
            const uint32_t cur_tick = g_song_tick_q16 >> 16;

            backing_process_events(&g_backing_softp, cur_tick);
            backing_process_events(&g_backing_hardp, cur_tick);
            backing_process_events(&g_backing_bass, cur_tick);

            if (backing_all_finished())
                backing_reset_all();

            g_song_tick_q16 += delta_q16;
        }

        mixed += synth_next_sample(&g_backing_softp.synth) * g_backing_softp.gain;
        mixed += synth_next_sample(&g_backing_hardp.synth) * g_backing_hardp.gain;
        mixed += synth_next_sample(&g_backing_bass.synth) * g_backing_bass.gain;

        if (mixed > 0.98f) mixed = 0.98f;
        if (mixed < -0.98f) mixed = -0.98f;
        out[i] = mixed;
    }
}

static void play_audio_start(play_state_t *state)
{
    if (state->audio_active)
        return;

    if (!IsAudioDeviceReady())
        InitAudioDevice();

    synth_init(&g_lead_synth, PLAY_AUDIO_SAMPLE_RATE);
    synth_set_default_wave(&g_lead_synth, SYNTH_WAVE_TRIANGLE, 1.0f);

    synth_init(&g_backing_softp.synth, PLAY_AUDIO_SAMPLE_RATE);
    synth_init(&g_backing_hardp.synth, PLAY_AUDIO_SAMPLE_RATE);
    synth_init(&g_backing_bass.synth, PLAY_AUDIO_SAMPLE_RATE);

    g_backing_softp.wave = SYNTH_WAVE_SINE;
    g_backing_softp.drive = 1.0f;
    g_backing_softp.gain = 0.34f;
    g_backing_hardp.wave = SYNTH_WAVE_DIST_SINE;
    g_backing_hardp.drive = 2.6f;
    g_backing_hardp.gain = 0.30f;
    g_backing_bass.wave = SYNTH_WAVE_SQUARE;
    g_backing_bass.drive = 1.0f;
    g_backing_bass.gain = 0.22f;

    backing_reset_all();

    g_play_stream = LoadAudioStream(PLAY_AUDIO_SAMPLE_RATE, 32, 1);
    SetAudioStreamCallback(g_play_stream, play_audio_callback);
    PlayAudioStream(g_play_stream);
    g_play_stream_loaded = 1;
    state->audio_active = 1;
}

static void play_audio_stop(play_state_t *state)
{
    if (!state->audio_active)
        return;

    if (g_play_stream_loaded)
    {
        UnloadAudioStream(g_play_stream);
        g_play_stream_loaded = 0;
    }
    if (IsAudioDeviceReady())
        CloseAudioDevice();

    state->audio_active = 0;
}

static void play_audio_note_on(play_state_t *state)
{
    if (!state->melody_loaded || state->melody.group_count <= 0)
        return;

    {
        const midi_note_group_t *g = &state->melody.groups[state->step_index];
        int count = g->note_count;
        if (count > 2) count = 2;
        atomic_store(&g_note_count, count);
        atomic_store(&g_note0, (count > 0) ? g->notes[0] : 60);
        atomic_store(&g_note1, (count > 1) ? g->notes[1] : 64);
        atomic_store(&g_audio_cmd, PLAY_AUDIO_CMD_NOTE_ON);
    }
}

static void play_audio_note_off(void)
{
    atomic_store(&g_audio_cmd, PLAY_AUDIO_CMD_NOTE_OFF);
}

static void play_audio_all_off(void)
{
    atomic_store(&g_audio_cmd, PLAY_AUDIO_CMD_ALL_OFF);
}

void play_init(play_state_t *state, const char *song_path, const char *song_title)
{
    int song_bpm = 120;

    play_deinit(state);
    state->bpm = 120;
    snprintf(state->song_path, sizeof(state->song_path), "%s", (song_path && song_path[0]) ? song_path : "assets/songs/demo.mid");
    if (song_title && song_title[0])
        snprintf(state->song_title, sizeof(state->song_title), "%s", song_title);
    else
        derive_song_title(state->song_title, state->song_path);
    state->selected = 0;
    state->select_mode = 0;
    state->is_playing = 1;
    state->ignore_initial_play = 1;
    state->prev_rot_down = 0;
    state->prev_play_down = 0;
    state->note_min = 60;
    state->note_max = 72;
    state->step_index = 0;
    state->scroll_accum = 0.0f;
    state->history_width = 0;
    state->audio_active = 0;
    memset(state->history_cols, 0, sizeof(state->history_cols));

    memset(&g_backing_softp, 0, sizeof(g_backing_softp));
    memset(&g_backing_hardp, 0, sizeof(g_backing_hardp));
    memset(&g_backing_bass, 0, sizeof(g_backing_bass));

    if (midi_extract_track_notes(state->song_path, "lead", &state->melody) == 0 && state->melody.group_count > 0)
    {
        state->melody_loaded = 1;
        analyze_melody_range(state);
    }
    else
    {
        state->melody_loaded = 0;
    }

    if (midi_extract_song_bpm(state->song_path, &song_bpm) == 0)
        state->bpm = clampi(song_bpm, PLAY_MIN_BPM, PLAY_MAX_BPM);

    if (midi_extract_track_note_events(state->song_path, "soft_p", &g_backing_softp.midi) == 0)
        g_backing_softp.loaded = 1;
    if (midi_extract_track_note_events(state->song_path, "hard_p", &g_backing_hardp.midi) == 0)
        g_backing_hardp.loaded = 1;
    if (midi_extract_track_note_events(state->song_path, "bass", &g_backing_bass.midi) == 0)
        g_backing_bass.loaded = 1;

    if (g_backing_softp.loaded) g_song_tpq = g_backing_softp.midi.ticks_per_quarter;
    else if (g_backing_hardp.loaded) g_song_tpq = g_backing_hardp.midi.ticks_per_quarter;
    else if (g_backing_bass.loaded) g_song_tpq = g_backing_bass.midi.ticks_per_quarter;
    else g_song_tpq = 480;

    atomic_store(&g_bpm_atomic, state->bpm);
    atomic_store(&g_backing_paused, 0);
    atomic_store(&g_backing_restart, 1);

    play_audio_start(state);
}

void play_deinit(play_state_t *state)
{
    play_audio_note_off();
    play_audio_stop(state);

    if (state->melody_loaded)
        midi_free_note_sequence(&state->melody);
    memset(&state->melody, 0, sizeof(state->melody));
    state->melody_loaded = 0;

    if (g_backing_softp.loaded)
        midi_free_track_events(&g_backing_softp.midi);
    if (g_backing_hardp.loaded)
        midi_free_track_events(&g_backing_hardp.midi);
    if (g_backing_bass.loaded)
        midi_free_track_events(&g_backing_bass.midi);
    memset(&g_backing_softp, 0, sizeof(g_backing_softp));
    memset(&g_backing_hardp, 0, sizeof(g_backing_hardp));
    memset(&g_backing_bass, 0, sizeof(g_backing_bass));
}

play_action_t play_update(play_state_t *state, input_poll_t in, float dt_seconds, int width, int height)
{
    int play_down_effective = in.play_down ? 1 : 0;

    // Ignore carry-over press when entering play mode from menu selection.
    if (state->ignore_initial_play)
    {
        if (in.play_down)
            play_down_effective = 0;
        else
            state->ignore_initial_play = 0;
    }

    const int play_enabled = state->select_mode ? 0 : 1;
    const int play_down_for_logic = play_enabled ? play_down_effective : 0;

    const int rot_pressed = (in.rot_down && !state->prev_rot_down) ? 1 : 0;
    const int play_pressed = (play_down_for_logic && !state->prev_play_down) ? 1 : 0;
    const int play_released = (!play_down_for_logic && state->prev_play_down) ? 1 : 0;
    state->prev_rot_down = in.rot_down ? 1 : 0;
    state->prev_play_down = play_down_for_logic;

    if (state->select_mode)
    {
        state->bpm -= in.rot_dl;
        state->bpm += in.rot_dr;
        state->bpm = clampi(state->bpm, PLAY_MIN_BPM, PLAY_MAX_BPM);
        atomic_store(&g_bpm_atomic, state->bpm);

        if (rot_pressed)
        {
            state->select_mode = 0;
            state->is_playing = 1; // "restart play session"
            state->step_index = 0;
            state->scroll_accum = 0.0f;
            memset(state->history_cols, 0, sizeof(state->history_cols));
            atomic_store(&g_backing_paused, 0);
            atomic_store(&g_backing_restart, 1);
            play_audio_all_off();
            if (state->prev_play_down)
                play_audio_note_on(state);
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
                atomic_store(&g_backing_paused, 1);
                play_audio_all_off();
            }
            else
            {
                play_audio_all_off();
                return PLAY_ACTION_EXIT_TO_MENU;
            }
        }
    }

    if (state->melody_loaded && state->melody.group_count > 0)
    {
        if (play_pressed)
        {
            state->is_playing = 1;
            if (!state->select_mode)
                play_audio_note_on(state);
        }

        if (play_released)
        {
            play_audio_note_off();
            state->step_index++;
            if (state->step_index >= state->melody.group_count)
                state->step_index = 0;
        }
    }

    {
        const int top_h = (height * 20) / 100;
        const int play_h = height - top_h;
        uint64_t present_mask = build_present_mask(state, play_h, play_down_for_logic);
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
    char title_label[SONG_NAME_MAX];
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

    snprintf(title_label, sizeof(title_label), "%.8s", state->song_title);
    draw_text_5x7(fb, width, height, 3, demo_y, title_label);

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
