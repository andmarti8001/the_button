CC := cc
CFLAGS := -std=c11 -Wall -Wextra -pedantic -Iinc
BIG_LCD_DIAGONAL_INCHES := 4.5f

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)

ifeq ($(strip $(RAYLIB_LIBS)),)
RAYLIB_CFLAGS :=
RAYLIB_LIBS := -lraylib
endif

LDFLAGS := $(RAYLIB_LIBS) -lm

SRC := src/write_splash.c src/inputs.c src/intro_animation.c src/intro_audio.c src/effects.c src/menu.c src/play.c src/midi_lead.c src/synth.c src/song_library.c
BIN := splash_viewer
BIN_BIG := splash_viewer_big
PLAY_SRC := src/play_main.c src/play.c src/inputs.c src/midi_lead.c src/synth.c
PLAY_BIN := play_viewer
AUDIO_PLAY_SRC := src/play_audio_main.c src/inputs.c src/midi_lead.c src/synth.c
AUDIO_PLAY_BIN := play_audio_viewer

.PHONY: install-deps build build-big run build-play run-play build-play-audio run-play-audio clean

install-deps:
	brew install raylib pkg-config

build:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

build-big:
	$(CC) $(CFLAGS) -DLCD_DIAGONAL_INCHES=$(BIG_LCD_DIAGONAL_INCHES) $(RAYLIB_CFLAGS) $(SRC) -o $(BIN_BIG) $(LDFLAGS)

run: build
	./$(BIN)

build-play:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(PLAY_SRC) -o $(PLAY_BIN) $(LDFLAGS)

run-play: build-play
	./$(PLAY_BIN)

build-play-audio:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(AUDIO_PLAY_SRC) -o $(AUDIO_PLAY_BIN) $(LDFLAGS)

run-play-audio: build-play-audio
	./$(AUDIO_PLAY_BIN)

clean:
	rm -f $(BIN) $(BIN_BIG) $(PLAY_BIN) $(AUDIO_PLAY_BIN)
