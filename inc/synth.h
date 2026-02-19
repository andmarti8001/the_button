#ifndef SYNTH_H
#define SYNTH_H

#include <stdint.h>

#ifndef SYNTH_OUTPUT_RANGE
#define SYNTH_OUTPUT_RANGE 0.90f
#endif

#ifndef SYNTH_MAX_VOICES
#define SYNTH_MAX_VOICES 12
#endif

typedef enum {
    SYNTH_WAVE_SINE = 0,
    SYNTH_WAVE_TRIANGLE,
    SYNTH_WAVE_SQUARE,
    SYNTH_WAVE_DIST_SINE
} synth_wave_t;

typedef struct {
    float phase;
    float phase_step;
    float env;
    int active;
    int gate_on;
    uint8_t note;
    synth_wave_t wave;
    float drive;
} synth_voice_t;

typedef struct {
    int sample_rate;
    float attack_seconds;
    float release_seconds;
    float sustain_level;
    synth_wave_t default_wave;
    float default_drive;
    int max_voices;
    synth_voice_t voices[SYNTH_MAX_VOICES];
} synth_t;

void synth_init(synth_t *synth, int sample_rate);
void synth_set_default_wave(synth_t *synth, synth_wave_t wave, float drive);
void synth_note_on(synth_t *synth, uint8_t note, synth_wave_t wave, float drive);
void synth_note_off(synth_t *synth, uint8_t note);
void synth_all_notes_off(synth_t *synth);
void synth_set_chord(synth_t *synth, const uint8_t *notes, int count);
void synth_gate_off(synth_t *synth);
float synth_next_sample(synth_t *synth);

#endif
