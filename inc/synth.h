#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

#ifndef SYNTH_OUTPUT_RANGE
#define SYNTH_OUTPUT_RANGE 0.90f
#endif

typedef struct {
    float phase;
    float phase_step;
    float env;
    int active;
    int gate_on;
} synth_voice_t;

typedef struct {
    int sample_rate;
    float attack_seconds;
    float release_seconds;
    float sustain_level;
    synth_voice_t voices[2];
} synth_t;

void synth_init(synth_t *synth, int sample_rate);
void synth_set_chord(synth_t *synth, const uint8_t *notes, int count);
void synth_gate_off(synth_t *synth);
float synth_next_sample(synth_t *synth);

#endif
