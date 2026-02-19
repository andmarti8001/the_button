#ifndef PLAY_H
#define PLAY_H

#include <stdint.h>
#include "input_poll.h"

typedef enum {
    PLAY_ACTION_NONE = 0,
    PLAY_ACTION_EXIT_TO_MENU
} play_action_t;

typedef struct {
    int bpm;
    int selected;      // 0 = BPM, 1 = X
    int select_mode;   // 1 while editing BPM
    int is_playing;    // 1 while session is active
    int prev_rot_down; // edge detection for rot_down
} play_state_t;

void play_init(play_state_t *state);
play_action_t play_update(play_state_t *state, input_poll_t in);
void write_play(uint8_t *fb, int width, int height, const play_state_t *state);

#endif
