#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "song_library.h"

static int has_midi_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;
    return (strcasecmp(dot, ".mid") == 0) || (strcasecmp(dot, ".midi") == 0);
}

static void make_display_name(char out[SONG_NAME_MAX], const char *filename)
{
    int j = 0;
    for (int i = 0; filename[i] && j < (SONG_NAME_MAX - 1); i++)
    {
        char c = filename[i];
        if (c == '.')
            break;

        if (c == '_' || c == '-')
            c = ' ';
        else
            c = (char)toupper((unsigned char)c);

        if (!isalnum((unsigned char)c) && c != ' ')
            c = ' ';
        out[j++] = c;
    }
    out[j] = '\0';
}

static int song_entry_cmp(const void *a, const void *b)
{
    const song_entry_t *sa = (const song_entry_t *)a;
    const song_entry_t *sb = (const song_entry_t *)b;
    return strcasecmp(sa->name, sb->name);
}

void song_library_clear(song_library_t *lib)
{
    memset(lib, 0, sizeof(*lib));
}

int song_library_load(song_library_t *lib, const char *dirpath)
{
    DIR *dir = opendir(dirpath);
    struct dirent *ent;

    song_library_clear(lib);
    if (!dir) return -1;

    while ((ent = readdir(dir)) != NULL)
    {
        song_entry_t *s;
        if (lib->count >= SONG_LIB_MAX)
            break;
        if (!has_midi_ext(ent->d_name))
            continue;

        s = &lib->entries[lib->count];
        snprintf(s->path, sizeof(s->path), "%s/%s", dirpath, ent->d_name);
        make_display_name(s->name, ent->d_name);
        if (s->name[0] == '\0')
            snprintf(s->name, sizeof(s->name), "SONG %d", lib->count + 1);
        lib->count++;
    }

    closedir(dir);
    qsort(lib->entries, (size_t)lib->count, sizeof(song_entry_t), song_entry_cmp);
    return 0;
}
