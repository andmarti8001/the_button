#ifndef EFFECTS_H
#define EFFECTS_H

typedef struct {
    int sample_rate;
    float rate_hz;
    float depth;
    float phase;
} tremolo_t;

void tremolo_init(tremolo_t *fx, int sample_rate, float rate_hz, float depth);
float tremolo_process(tremolo_t *fx, float sample);

#endif
