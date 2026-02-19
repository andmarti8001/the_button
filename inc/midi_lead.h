#ifndef MIDI_LEAD_H
#define MIDI_LEAD_H

#include <stdint.h>

#ifndef MIDI_MAX_NOTES_PER_TICK
#define MIDI_MAX_NOTES_PER_TICK 8
#endif

typedef struct {
    int note_count;
    uint8_t notes[MIDI_MAX_NOTES_PER_TICK];
} midi_note_group_t;

typedef struct {
    uint16_t ticks_per_quarter;
    int group_count;
    midi_note_group_t *groups;
} midi_note_sequence_t;

int midi_extract_track_notes(const char *midi_path, const char *track_name, midi_note_sequence_t *out_seq);
void midi_free_note_sequence(midi_note_sequence_t *seq);

#endif
