#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "midi_lead.h"

typedef struct {
    uint32_t tick;
    uint8_t note;
} note_event_t;

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static int read_vlq(const uint8_t *data, size_t size, size_t *pos, uint32_t *out)
{
    uint32_t value = 0;
    int count = 0;

    while (*pos < size && count < 4)
    {
        const uint8_t b = data[*pos];
        (*pos)++;
        value = (value << 7) | (uint32_t)(b & 0x7F);
        count++;
        if ((b & 0x80) == 0)
        {
            *out = value;
            return 0;
        }
    }

    return -1;
}

static int event_cmp(const void *a, const void *b)
{
    const note_event_t *ea = (const note_event_t *)a;
    const note_event_t *eb = (const note_event_t *)b;
    if (ea->tick < eb->tick) return -1;
    if (ea->tick > eb->tick) return 1;
    if (ea->note < eb->note) return -1;
    if (ea->note > eb->note) return 1;
    return 0;
}

static int append_note_event(note_event_t **events, int *count, int *cap, uint32_t tick, uint8_t note)
{
    if (*count >= *cap)
    {
        const int new_cap = (*cap == 0) ? 64 : (*cap * 2);
        note_event_t *new_events = (note_event_t *)realloc(*events, (size_t)new_cap * sizeof(note_event_t));
        if (!new_events) return -1;
        *events = new_events;
        *cap = new_cap;
    }

    (*events)[*count].tick = tick;
    (*events)[*count].note = note;
    (*count)++;
    return 0;
}

static int parse_track_events(
    const uint8_t *trk,
    size_t trk_size,
    const char *target_track_name,
    int *matched_track,
    note_event_t **out_events,
    int *out_count
)
{
    size_t pos = 0;
    uint32_t abs_tick = 0;
    uint8_t running_status = 0;
    note_event_t *events = NULL;
    int events_count = 0;
    int events_cap = 0;

    *matched_track = 0;
    *out_events = NULL;
    *out_count = 0;

    while (pos < trk_size)
    {
        uint32_t delta = 0;
        uint8_t status = 0;
        int has_status_byte = 0;
        uint8_t data1 = 0;
        uint8_t data2 = 0;

        if (read_vlq(trk, trk_size, &pos, &delta) != 0)
            goto fail;
        abs_tick += delta;

        if (pos >= trk_size)
            goto fail;

        status = trk[pos];
        if (status < 0x80)
        {
            if (running_status == 0)
                goto fail;
            status = running_status;
        }
        else
        {
            has_status_byte = 1;
            pos++;
            if (status >= 0x80 && status <= 0xEF)
                running_status = status;
        }

        if (status == 0xFF)
        {
            uint32_t meta_len = 0;
            uint8_t meta_type = 0;

            if (pos >= trk_size) goto fail;
            meta_type = trk[pos++];
            if (read_vlq(trk, trk_size, &pos, &meta_len) != 0) goto fail;
            if (pos + meta_len > trk_size) goto fail;

            if (meta_type == 0x03 || meta_type == 0x04)
            {
                char label[128];
                const size_t copy_len = (meta_len < (sizeof(label) - 1)) ? (size_t)meta_len : (sizeof(label) - 1);
                memcpy(label, &trk[pos], copy_len);
                label[copy_len] = '\0';

                if (target_track_name && strcasecmp(label, target_track_name) == 0)
                    *matched_track = 1;
            }
            else if (meta_type == 0x2F)
            {
                pos += meta_len;
                break;
            }

            pos += meta_len;
            continue;
        }

        if (status == 0xF0 || status == 0xF7)
        {
            uint32_t syx_len = 0;
            if (read_vlq(trk, trk_size, &pos, &syx_len) != 0) goto fail;
            if (pos + syx_len > trk_size) goto fail;
            pos += syx_len;
            continue;
        }

        if (status >= 0xF1 && status <= 0xFE)
        {
            int sys_data_len = 0;
            switch (status)
            {
                case 0xF1: sys_data_len = 1; break;
                case 0xF2: sys_data_len = 2; break;
                case 0xF3: sys_data_len = 1; break;
                case 0xF6: sys_data_len = 0; break;
                default:
                    // Real-time/system one-byte events: no data bytes.
                    sys_data_len = 0;
                    break;
            }

            if (pos + (size_t)sys_data_len > trk_size) goto fail;
            pos += (size_t)sys_data_len;
            continue;
        }

        if (has_status_byte && pos >= trk_size) goto fail;
        if (!has_status_byte && pos >= trk_size) goto fail;

        if (trk[pos] >= 0x80) goto fail;
        data1 = trk[pos++];

        switch (status & 0xF0)
        {
            case 0xC0:
            case 0xD0:
                break;

            default:
                if (pos >= trk_size) goto fail;
                data2 = trk[pos++];
                break;
        }

        if ((status & 0xF0) == 0x90 && data2 > 0)
        {
            if (append_note_event(&events, &events_count, &events_cap, abs_tick, data1) != 0)
                goto fail;
        }
    }

    *out_events = events;
    *out_count = events_count;
    return 0;

fail:
    free(events);
    return -1;
}

static int events_to_groups(note_event_t *events, int event_count, midi_note_sequence_t *out_seq)
{
    int i = 0;
    int group_count = 0;
    midi_note_group_t *groups = NULL;

    if (event_count <= 0)
    {
        out_seq->groups = NULL;
        out_seq->group_count = 0;
        return 0;
    }

    qsort(events, (size_t)event_count, sizeof(note_event_t), event_cmp);

    groups = (midi_note_group_t *)calloc((size_t)event_count, sizeof(midi_note_group_t));
    if (!groups)
        return -1;

    while (i < event_count)
    {
        const uint32_t tick = events[i].tick;
        midi_note_group_t *g = &groups[group_count];
        g->note_count = 0;

        while (i < event_count && events[i].tick == tick)
        {
            int duplicate = 0;
            for (int n = 0; n < g->note_count; n++)
            {
                if (g->notes[n] == events[i].note)
                {
                    duplicate = 1;
                    break;
                }
            }

            if (!duplicate && g->note_count < MIDI_MAX_NOTES_PER_TICK)
            {
                g->notes[g->note_count] = events[i].note;
                g->note_count++;
            }
            i++;
        }

        group_count++;
    }

    out_seq->groups = groups;
    out_seq->group_count = group_count;
    return 0;
}

int midi_extract_track_notes(const char *midi_path, const char *track_name, midi_note_sequence_t *out_seq)
{
    FILE *fp = NULL;
    uint8_t *data = NULL;
    size_t size = 0;
    size_t pos = 0;
    uint16_t track_count = 0;
    int matched = 0;
    note_event_t *selected_events = NULL;
    int selected_count = 0;

    memset(out_seq, 0, sizeof(*out_seq));

    fp = fopen(midi_path, "rb");
    if (!fp) return -1;

    if (fseek(fp, 0, SEEK_END) != 0) goto fail;
    size = (size_t)ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) goto fail;

    data = (uint8_t *)malloc(size);
    if (!data) goto fail;
    if (fread(data, 1, size, fp) != size) goto fail;
    fclose(fp);
    fp = NULL;

    if (size < 14) goto fail;
    if (memcmp(data, "MThd", 4) != 0) goto fail;
    if (read_be32(&data[4]) != 6) goto fail;

    out_seq->ticks_per_quarter = read_be16(&data[12]);
    track_count = read_be16(&data[10]);
    pos = 14;

    for (uint16_t t = 0; t < track_count; t++)
    {
        uint32_t trk_len = 0;
        note_event_t *events = NULL;
        int events_count = 0;
        int this_matched = 0;

        if (pos + 8 > size) goto fail;
        if (memcmp(&data[pos], "MTrk", 4) != 0) goto fail;
        trk_len = read_be32(&data[pos + 4]);
        pos += 8;
        if (pos + trk_len > size) goto fail;

        if (parse_track_events(&data[pos], trk_len, track_name, &this_matched, &events, &events_count) != 0)
        {
            free(events);
            goto fail;
        }

        if (this_matched)
        {
            matched = 1;
            free(selected_events);
            selected_events = events;
            selected_count = events_count;
        }
        else
        {
            free(events);
        }

        pos += trk_len;
    }

    if (!matched)
        goto fail;

    if (events_to_groups(selected_events, selected_count, out_seq) != 0)
        goto fail;

    free(selected_events);
    free(data);
    return 0;

fail:
    if (fp) fclose(fp);
    free(selected_events);
    free(data);
    midi_free_note_sequence(out_seq);
    return -1;
}

void midi_free_note_sequence(midi_note_sequence_t *seq)
{
    if (!seq) return;
    free(seq->groups);
    seq->groups = NULL;
    seq->group_count = 0;
    seq->ticks_per_quarter = 0;
}
