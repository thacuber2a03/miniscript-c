SRC := src
OUT ?= ./miniscript
BUILD := build

HFILES := $(wildcard $(SRC)/*.h)
CFILES := $(wildcard $(SRC)/*.c) ./main.c
OBJECTS := $(addprefix $(BUILD)/, $(notdir $(CFILES:.c=.o)))
CFLAGS := -std=c99 -I$(SRC) -g -DMS_DEBUG
LDLIBS := -lm

.PHONY: always release debug clean

all: $(BUILD) debug

$(BUILD):
	mkdir -p $(BUILD)

debug: $(HFILES) $(OBJECTS)
	$(CC) -o $(OUT) $(OBJECTS) $(LDLIBS)

release: $(HFILES) $(OBJECTS)
	# TODO: this isn't a "release" build yet
	$(CC) -o $(OUT) $(OBJECTS) $(LDLIBS)

$(BUILD)/%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm $(OUT) $(BUILD) -r
