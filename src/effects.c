#include <math.h>
#include "effects.h"

#ifndef FX_PI
#define FX_PI 3.14159265358979323846f
#endif

void tremolo_init(tremolo_t *fx, int sample_rate, float rate_hz, float depth)
{
    fx->sample_rate = sample_rate;
    fx->rate_hz = rate_hz;
    fx->depth = depth;
    fx->phase = 0.0f;
}

float tremolo_process(tremolo_t *fx, float sample)
{
    const float lfo = 0.5f * (sinf(fx->phase) + 1.0f);
    const float gain = 1.0f - (fx->depth * lfo);
    const float step = (2.0f * FX_PI * fx->rate_hz) / (float)fx->sample_rate;

    fx->phase += step;
    if (fx->phase > (2.0f * FX_PI))
        fx->phase -= (2.0f * FX_PI);

    return sample * gain;
}
