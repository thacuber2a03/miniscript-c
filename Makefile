SRC := src
OUT ?= ./miniscript

HFILES := $(wildcard $(SRC)/*.h)
CFILES := $(wildcard $(SRC)/*.c) ./main.c
CFLAGS := -std=c99 -lm -I$(SRC)

.PHONY: release debug clean

all: debug

debug: $(CFILES)
	$(CC) $(CFILES) -g -DMS_DEBUG $(CFLAGS) -o $(OUT)

release: $(wildcard *.h) $(CFILES)
	# TODO: figure out how not to compile 'ms_debug.c' in a release build
	$(CC) $(CFILES) -o $(CFLAGS) $(OUT)

clean:
	rm $(OUT)
