CC ?= gcc
SRC := src
OUT ?= ./miniscript
BUILD := build

HFILES := $(wildcard $(SRC)/*.h)
CFILES := $(wildcard $(SRC)/*.c)
OBJECTS := $(addprefix $(BUILD)/, $(notdir $(CFILES:.c=.o)))
CFLAGS := -std=c99 -I$(SRC) -Wall -Wextra -pedantic
LDLIBS := -lm

debug-flags ?= MS_DEBUG

ifndef release
	OBJECTS := $(OBJECTS:.o=.debug.o)
	CFLAGS += -g $(addprefix -D, $(debug-flags))
	OUT := $(OUT)-debug
endif

.PHONY: clean all

all: $(BUILD) $(OUT)

$(BUILD):
	mkdir -p $(BUILD)

$(OUT): $(HFILES) $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) $(LDLIBS)

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.debug.o: $(SRC)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm $(OUT) $(BUILD) -r
