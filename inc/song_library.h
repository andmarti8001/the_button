#ifndef SONG_LIBRARY_H
#define SONG_LIBRARY_H

#define SONG_LIB_MAX 64
#define SONG_PATH_MAX 256
#define SONG_NAME_MAX 32

typedef struct {
    char path[SONG_PATH_MAX];
    char name[SONG_NAME_MAX];
} song_entry_t;

typedef struct {
    int count;
    song_entry_t entries[SONG_LIB_MAX];
} song_library_t;

void song_library_clear(song_library_t *lib);
int song_library_load(song_library_t *lib, const char *dirpath);

#endif
