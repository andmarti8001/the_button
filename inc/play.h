#ifndef PLAY_H
#define PLAY_H

#include <stdint.h>
#include "input_poll.h"
#include "midi_lead.h"
#include "song_library.h"

typedef enum {
    PLAY_ACTION_NONE = 0,
    PLAY_ACTION_EXIT_TO_MENU
} play_action_t;

typedef struct {
    char song_path[SONG_PATH_MAX];
    char song_title[SONG_NAME_MAX];
    int bpm;
    int selected;      // 0 = PS/SS, 1 = BPM, 2 = X
    int select_mode;   // 1 while editing BPM
    int is_playing;    // 1 while transport running
    int has_started_once;
    int ignore_initial_play;
    int prev_rot_down; // edge detection for rot_down
    int prev_play_down;

    int note_min;
    int note_max;
    int step_index;
    float scroll_accum;

    midi_note_sequence_t melody;
    int melody_loaded;
    int audio_active;

    uint64_t history_cols[128];
    int history_width;
} play_state_t;

void play_init(play_state_t *state, const char *song_path, const char *song_title);
void play_deinit(play_state_t *state);
play_action_t play_update(play_state_t *state, input_poll_t in, float dt_seconds, int width, int height);
void write_play(uint8_t *fb, int width, int height, const play_state_t *state);

#endif
