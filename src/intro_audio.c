#include <string.h>
#include <stdatomic.h>
#include <raylib.h>
#include "intro_audio.h"
#include "midi_lead.h"
#include "synth.h"
#include "effects.h"

#ifndef INTRO_AUDIO_SAMPLE_RATE
#define INTRO_AUDIO_SAMPLE_RATE 48000
#endif

typedef struct {
    int loaded;
    int running;
    AudioStream stream;
    synth_t synth;
    tremolo_t trem;
    midi_track_events_t track;
    int cursor;
    uint32_t tick_q16;
    int tpq;
    int bpm;
    int finished;
    _Atomic int restart_req;
} intro_audio_state_t;

static intro_audio_state_t g_intro;

static void intro_audio_reset_playhead(void)
{
    g_intro.cursor = 0;
    g_intro.tick_q16 = 0;
    g_intro.finished = 0;
    synth_all_notes_off(&g_intro.synth);
}

static void intro_audio_apply_events(uint32_t tick)
{
    while (g_intro.cursor < g_intro.track.event_count && g_intro.track.events[g_intro.cursor].tick <= tick)
    {
        const midi_note_event_t *e = &g_intro.track.events[g_intro.cursor];
        if (e->is_note_on)
            synth_note_on(&g_intro.synth, e->note, SYNTH_WAVE_SINE, 1.0f);
        else
            synth_note_off(&g_intro.synth, e->note);
        g_intro.cursor++;
    }

    if (g_intro.cursor >= g_intro.track.event_count)
    {
        g_intro.finished = 1;
        synth_all_notes_off(&g_intro.synth);
    }
}

static void intro_audio_callback(void *buffer_data, unsigned int frames)
{
    float *out = (float *)buffer_data;

    if (atomic_exchange(&g_intro.restart_req, 0))
        intro_audio_reset_playhead();

    for (unsigned int i = 0; i < frames; i++)
    {
        float s;

        if (!g_intro.finished)
        {
            const float ticks_per_sample = ((float)g_intro.bpm * (float)g_intro.tpq) / (60.0f * (float)INTRO_AUDIO_SAMPLE_RATE);
            const uint32_t delta_q16 = (uint32_t)(ticks_per_sample * 65536.0f);
            const uint32_t tick = g_intro.tick_q16 >> 16;
            intro_audio_apply_events(tick);
            g_intro.tick_q16 += delta_q16;
        }

        s = synth_next_sample(&g_intro.synth) * 0.55f;
        s = tremolo_process(&g_intro.trem, s);
        if (s > 0.95f) s = 0.95f;
        if (s < -0.95f) s = -0.95f;
        out[i] = s;
    }
}

static int intro_audio_try_load_track(const char *path)
{
    static const char *k_tracks[] = {"lead", "soft_p", "hard_p", "bass"};

    for (int i = 0; i < (int)(sizeof(k_tracks) / sizeof(k_tracks[0])); i++)
    {
        if (midi_extract_track_note_events(path, k_tracks[i], &g_intro.track) == 0)
            return 0;
    }
    return -1;
}

int intro_audio_start(void)
{
    const char *path1 = "assets/intro.mid";
    const char *path2 = "intro.mid";

    if (g_intro.running)
    {
        atomic_store(&g_intro.restart_req, 1);
        return 0;
    }

    memset(&g_intro, 0, sizeof(g_intro));
    if (intro_audio_try_load_track(path1) != 0 && intro_audio_try_load_track(path2) != 0)
        return -1;

    g_intro.loaded = 1;
    g_intro.tpq = (g_intro.track.ticks_per_quarter > 0) ? g_intro.track.ticks_per_quarter : 480;
    g_intro.bpm = (g_intro.track.bpm > 0) ? g_intro.track.bpm : 120;

    if (!IsAudioDeviceReady())
        InitAudioDevice();

    synth_init(&g_intro.synth, INTRO_AUDIO_SAMPLE_RATE);
    synth_set_default_wave(&g_intro.synth, SYNTH_WAVE_SINE, 1.0f);
    tremolo_init(&g_intro.trem, INTRO_AUDIO_SAMPLE_RATE, 6.5f, 0.45f);
    intro_audio_reset_playhead();

    g_intro.stream = LoadAudioStream(INTRO_AUDIO_SAMPLE_RATE, 32, 1);
    SetAudioStreamCallback(g_intro.stream, intro_audio_callback);
    PlayAudioStream(g_intro.stream);
    g_intro.running = 1;
    return 0;
}

void intro_audio_stop(void)
{
    if (!g_intro.running)
        return;

    UnloadAudioStream(g_intro.stream);
    if (g_intro.loaded)
        midi_free_track_events(&g_intro.track);
    memset(&g_intro, 0, sizeof(g_intro));
    if (IsAudioDeviceReady())
        CloseAudioDevice();
}
