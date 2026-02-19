CC := cc
CFLAGS := -std=c11 -Wall -Wextra -pedantic -Iinc

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)

ifeq ($(strip $(RAYLIB_LIBS)),)
RAYLIB_CFLAGS :=
RAYLIB_LIBS := -lraylib
endif

LDFLAGS := $(RAYLIB_LIBS) -lm

SRC := src/write_splash.c src/inputs.c src/intro_animation.c src/menu.c src/play.c
BIN := splash_viewer
PLAY_SRC := src/play_main.c src/play.c src/inputs.c
PLAY_BIN := play_viewer

.PHONY: install-deps build run build-play run-play clean

install-deps:
	brew install raylib pkg-config

build:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

run: build
	./$(BIN)

build-play:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(PLAY_SRC) -o $(PLAY_BIN) $(LDFLAGS)

run-play: build-play
	./$(PLAY_BIN)

clean:
	rm -f $(BIN) $(PLAY_BIN)
