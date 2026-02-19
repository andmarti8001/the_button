CC := cc
CFLAGS := -std=c11 -Wall -Wextra -pedantic

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)

ifeq ($(strip $(RAYLIB_LIBS)),)
RAYLIB_CFLAGS :=
RAYLIB_LIBS := -lraylib
endif

LDFLAGS := $(RAYLIB_LIBS) -lm

SRC := write_splash.c
BIN := splash_viewer

.PHONY: install-deps build run clean

install-deps:
	brew install raylib pkg-config

build:
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

run: build
	./$(BIN)

clean:
	rm -f $(BIN)
