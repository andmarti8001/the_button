#include <raylib.h>
#include "input_poll.h"

input_poll_t input_poll(void)
{
    input_poll_t s;

    // Rotary encoder deltas are edge-triggered to mimic one detent per keypress.
    s.rot_dl = IsKeyPressed(KEY_LEFT) ? 1 : 0;
    s.rot_dr = IsKeyPressed(KEY_RIGHT) ? 1 : 0;

    // Push states are level-triggered while held down.
    s.rot_down = IsKeyDown(KEY_DOWN) ? 1 : 0;
    s.play_down = IsKeyDown(KEY_SPACE) ? 1 : 0;

    return s;
}
