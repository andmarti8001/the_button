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

void synth_init(synth_t *synth, int sample_rate)
{
    memset(synth, 0, sizeof(*synth));
    synth->sample_rate = sample_rate;
    synth->attack_seconds = 0.05f;
    synth->release_seconds = 0.10f;
    synth->sustain_level = 0.95f;
}

void synth_set_chord(synth_t *synth, const uint8_t *notes, int count)
{
    for (int i = 0; i < 2; i++)
    {
        synth_voice_t *v = &synth->voices[i];
        if (i < count)
        {
            const float freq = midi_note_to_freq(notes[i]);
            v->phase_step = (2.0f * SYNTH_PI * freq) / (float)synth->sample_rate;
            v->active = 1;
            v->gate_on = 1;
            v->env = 0.0f;
        }
        else
        {
            v->active = 0;
            v->gate_on = 0;
            v->env = 0.0f;
        }
    }
}

void synth_gate_off(synth_t *synth)
{
    for (int i = 0; i < 2; i++)
        synth->voices[i].gate_on = 0;
}

float synth_next_sample(synth_t *synth)
{
    float sample = 0.0f;
    const float attack_step = (synth->attack_seconds > 0.0f)
        ? (synth->sustain_level / (synth->attack_seconds * (float)synth->sample_rate))
        : synth->sustain_level;
    const float release_step = (synth->release_seconds > 0.0f)
        ? (synth->sustain_level / (synth->release_seconds * (float)synth->sample_rate))
        : synth->sustain_level;

    for (int i = 0; i < 2; i++)
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

        sample += sinf(v->phase) * v->env;
        v->phase += v->phase_step;
        if (v->phase > (2.0f * SYNTH_PI))
            v->phase -= (2.0f * SYNTH_PI);
    }

    sample *= SYNTH_OUTPUT_RANGE * 0.5f; // 2 voices max.
    if (sample > SYNTH_OUTPUT_RANGE) sample = SYNTH_OUTPUT_RANGE;
    if (sample < -SYNTH_OUTPUT_RANGE) sample = -SYNTH_OUTPUT_RANGE;
    return sample;
}
