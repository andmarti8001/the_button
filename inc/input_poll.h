#ifndef INPUT_POLL_H
#define INPUT_POLL_H

typedef struct {
    int rot_dl;     // Rotary encoder delta left (steps since last poll)
    int rot_dr;     // Rotary encoder delta right (steps since last poll)
    int play_down;  // Tactile play button state (space bar held)
    int rot_down;   // Rotary push switch state (down arrow held)
} input_poll_t;

input_poll_t input_poll(void);

#endif
