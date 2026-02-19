#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "input_poll.h"
#include "song_library.h"

typedef enum {
    MENU_ACTION_NONE = 0,
    MENU_ACTION_PLAY_SONG,
    MENU_ACTION_EXIT
} menu_action_t;

typedef struct {
    const song_library_t *songs;
    int selected;
    int scroll_top;
    int selected_song_index;
    int prev_select_down;
} menu_state_t;

void menu_init(menu_state_t *state, const song_library_t *songs);
menu_action_t menu_update(menu_state_t *state, input_poll_t in);
void write_menu(uint8_t *fb, int width, int height, const menu_state_t *state);

#endif
