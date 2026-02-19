#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include <math.h>
#include <raylib.h>
#include "input_poll.h"
#include "midi_lead.h"
#include "synth.h"

#ifndef PLAY_AUDIO_SAMPLE_RATE
#define PLAY_AUDIO_SAMPLE_RATE 48000
#endif

#ifndef LCD_DIAGONAL_INCHES
#define LCD_DIAGONAL_INCHES 0.96f
#endif

#ifndef PLAY_AUDIO_TRACK_NAME
#define PLAY_AUDIO_TRACK_NAME "lead"
#endif

typedef enum {
    AUDIO_CMD_NONE = 0,
    AUDIO_CMD_NOTE_ON,
    AUDIO_CMD_NOTE_OFF
} audio_cmd_t;

static synth_t g_synth;
static _Atomic int g_audio_cmd = AUDIO_CMD_NONE;
static _Atomic int g_note_count = 0;
static _Atomic int g_note0 = 60;
static _Atomic int g_note1 = 64;

static void audio_callback(void *buffer_data, unsigned int frames)
{
    float *out = (float *)buffer_data;
    const int cmd = atomic_exchange(&g_audio_cmd, AUDIO_CMD_NONE);

    if (cmd == AUDIO_CMD_NOTE_ON)
    {
        uint8_t chord[2];
        int count = atomic_load(&g_note_count);
        if (count < 0) count = 0;
        if (count > 2) count = 2;
        chord[0] = (uint8_t)atomic_load(&g_note0);
        chord[1] = (uint8_t)atomic_load(&g_note1);
        synth_set_chord(&g_synth, chord, count);
    }
    else if (cmd == AUDIO_CMD_NOTE_OFF)
    {
        synth_gate_off(&g_synth);
    }

    for (unsigned int i = 0; i < frames; i++)
        out[i] = synth_next_sample(&g_synth);
}

static float get_monitor_ppi(int monitor)
{
    const int px_w = GetMonitorWidth(monitor);
    const int mm_w = GetMonitorPhysicalWidth(monitor);
    if (px_w <= 0 || mm_w <= 0) return 110.0f;
    return (float)px_w / ((float)mm_w / 25.4f);
}

int main(void)
{
    midi_note_sequence_t seq;
    int step_index = 0;
    int prev_play_down = 0;

    if (midi_extract_track_notes("assets/songs/demo.mid", PLAY_AUDIO_TRACK_NAME, &seq) != 0)
    {
        fprintf(stderr, "Failed to parse assets/songs/demo.mid track '%s'\n", PLAY_AUDIO_TRACK_NAME);
        return 1;
    }

    if (seq.group_count <= 0)
    {
        fprintf(stderr, "Track '%s' has no note-on groups\n", PLAY_AUDIO_TRACK_NAME);
        midi_free_note_sequence(&seq);
        return 1;
    }

    const float aspect = 128.0f / 64.0f;
    const float lcd_h_inches = LCD_DIAGONAL_INCHES / sqrtf((aspect * aspect) + 1.0f);
    const float lcd_w_inches = lcd_h_inches * aspect;
    const float host_ppi = get_monitor_ppi(0);
    const int win_w = (int)roundf(lcd_w_inches * host_ppi);
    const int win_h = (int)roundf(lcd_h_inches * host_ppi);

    InitWindow((win_w > 1) ? win_w : 1, (win_h > 1) ? win_h : 1, "Play Audio Test");
    SetTargetFPS(60);

    InitAudioDevice();
    synth_init(&g_synth, PLAY_AUDIO_SAMPLE_RATE);

    AudioStream stream = LoadAudioStream(PLAY_AUDIO_SAMPLE_RATE, 32, 1);
    SetAudioStreamCallback(stream, audio_callback);
    PlayAudioStream(stream);

    while (!WindowShouldClose())
    {
        const input_poll_t in = input_poll();
        const int pressed = (in.play_down && !prev_play_down) ? 1 : 0;
        const int released = (!in.play_down && prev_play_down) ? 1 : 0;
        prev_play_down = in.play_down ? 1 : 0;

        if (pressed)
        {
            const midi_note_group_t *g = &seq.groups[step_index];
            const int count = (g->note_count > 2) ? 2 : g->note_count;
            atomic_store(&g_note_count, count);
            atomic_store(&g_note0, (count > 0) ? g->notes[0] : 60);
            atomic_store(&g_note1, (count > 1) ? g->notes[1] : 64);
            atomic_store(&g_audio_cmd, AUDIO_CMD_NOTE_ON);
        }
        else if (released)
        {
            atomic_store(&g_audio_cmd, AUDIO_CMD_NOTE_OFF);
            step_index++;
            if (step_index >= seq.group_count)
                step_index = 0;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("PLAY AUDIO TEST", 12, 12, 20, RAYWHITE);
        DrawText("HOLD SPACE = NOTE ON", 12, 36, 18, RAYWHITE);
        DrawText("RELEASE SPACE = NEXT NOTE", 12, 58, 18, RAYWHITE);
        DrawText(TextFormat("TRACK: %s", PLAY_AUDIO_TRACK_NAME), 12, 88, 18, RAYWHITE);
        DrawText(TextFormat("STEP: %d / %d", step_index + 1, seq.group_count), 12, 110, 18, RAYWHITE);
        DrawText(TextFormat("BPM RANGE MACRO: SYNTH_OUTPUT_RANGE=%.2f", SYNTH_OUTPUT_RANGE), 12, 132, 18, RAYWHITE);
        EndDrawing();
    }

    UnloadAudioStream(stream);
    CloseAudioDevice();
    CloseWindow();
    midi_free_note_sequence(&seq);
    return 0;
}
