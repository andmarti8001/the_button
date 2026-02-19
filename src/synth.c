#include <math.h>
#include <string.h>
#include "synth.h"

#ifndef SYNTH_PI
#define SYNTH_PI 3.14159265358979323846f
#endif

static float midi_note_to_freq(uint8_t note)
{
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float wave_sample(synth_wave_t wave, float phase, float drive)
{
    const float s = sinf(phase);
    switch (wave)
    {
        case SYNTH_WAVE_TRIANGLE:
            return (2.0f / SYNTH_PI) * asinf(s);
        case SYNTH_WAVE_SQUARE:
            return (s >= 0.0f) ? 1.0f : -1.0f;
        case SYNTH_WAVE_DIST_SINE:
        {
            const float d = (drive < 0.1f) ? 0.1f : drive;
            return tanhf(s * d) / tanhf(d);
        }
        case SYNTH_WAVE_SINE:
        default:
            return s;
    }
}

void synth_init(synth_t *synth, int sample_rate)
{
    memset(synth, 0, sizeof(*synth));
    synth->sample_rate = sample_rate;
    synth->attack_seconds = 0.05f;
    synth->release_seconds = 0.10f;
    synth->sustain_level = 0.95f;
    synth->default_wave = SYNTH_WAVE_SINE;
    synth->default_drive = 2.0f;
    synth->max_voices = SYNTH_MAX_VOICES;
}

void synth_set_default_wave(synth_t *synth, synth_wave_t wave, float drive)
{
    synth->default_wave = wave;
    synth->default_drive = drive;
}

void synth_note_on(synth_t *synth, uint8_t note, synth_wave_t wave, float drive)
{
    int idx = -1;
    for (int i = 0; i < synth->max_voices; i++)
    {
        if (synth->voices[i].active && synth->voices[i].note == note)
        {
            idx = i;
            break;
        }
        if (idx < 0 && !synth->voices[i].active)
            idx = i;
    }
    if (idx < 0) idx = 0; // voice steal

    synth_voice_t *v = &synth->voices[idx];
    const float freq = midi_note_to_freq(note);
    v->note = note;
    v->phase_step = (2.0f * SYNTH_PI * freq) / (float)synth->sample_rate;
    v->active = 1;
    v->gate_on = 1;
    v->env = 0.0f;
    v->wave = wave;
    v->drive = drive;
}

void synth_note_off(synth_t *synth, uint8_t note)
{
    for (int i = 0; i < synth->max_voices; i++)
    {
        if (synth->voices[i].active && synth->voices[i].note == note)
            synth->voices[i].gate_on = 0;
    }
}

void synth_all_notes_off(synth_t *synth)
{
    for (int i = 0; i < synth->max_voices; i++)
        synth->voices[i].gate_on = 0;
}

void synth_set_chord(synth_t *synth, const uint8_t *notes, int count)
{
    for (int i = 0; i < synth->max_voices; i++)
    {
        synth->voices[i].active = 0;
        synth->voices[i].gate_on = 0;
        synth->voices[i].env = 0.0f;
    }

    if (count < 0) count = 0;
    if (count > 2) count = 2;
    for (int i = 0; i < count; i++)
        synth_note_on(synth, notes[i], synth->default_wave, synth->default_drive);
}

void synth_gate_off(synth_t *synth)
{
    synth_all_notes_off(synth);
}

float synth_next_sample(synth_t *synth)
{
    float sample = 0.0f;
    int active_count = 0;
    const float attack_step = (synth->attack_seconds > 0.0f)
        ? (synth->sustain_level / (synth->attack_seconds * (float)synth->sample_rate))
        : synth->sustain_level;
    const float release_step = (synth->release_seconds > 0.0f)
        ? (synth->sustain_level / (synth->release_seconds * (float)synth->sample_rate))
        : synth->sustain_level;

    for (int i = 0; i < synth->max_voices; i++)
    {
        synth_voice_t *v = &synth->voices[i];
        if (!v->active)
            continue;

        if (v->gate_on)
        {
            v->env += attack_step;
            if (v->env > synth->sustain_level)
                v->env = synth->sustain_level;
        }
        else
        {
            v->env -= release_step;
            if (v->env <= 0.0f)
            {
                v->env = 0.0f;
                v->active = 0;
                continue;
            }
        }

        sample += wave_sample(v->wave, v->phase, v->drive) * v->env;
        active_count++;

        v->phase += v->phase_step;
        if (v->phase > (2.0f * SYNTH_PI))
            v->phase -= (2.0f * SYNTH_PI);
    }

    if (active_count > 0)
        sample /= (float)active_count;

    sample *= SYNTH_OUTPUT_RANGE;
    sample = clampf(sample, -SYNTH_OUTPUT_RANGE, SYNTH_OUTPUT_RANGE);
    return sample;
}
