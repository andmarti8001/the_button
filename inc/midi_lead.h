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

typedef struct {
    uint32_t tick;
    uint8_t note;
    uint8_t is_note_on;
} midi_note_event_t;

typedef struct {
    uint16_t ticks_per_quarter;
    int bpm;
    int event_count;
    midi_note_event_t *events;
} midi_track_events_t;

int midi_extract_track_notes(const char *midi_path, const char *track_name, midi_note_sequence_t *out_seq);
int midi_extract_track_note_events(const char *midi_path, const char *track_name, midi_track_events_t *out_track);
int midi_extract_song_bpm(const char *midi_path, int *out_bpm);
void midi_free_note_sequence(midi_note_sequence_t *seq);
void midi_free_track_events(midi_track_events_t *track);

#endif
