BUILD := build
SRC := src
LIB := lib

CC = gcc
CFLAGS = -g -Wall -pthread -Iinclude
LDLIBS = -lrt -lncurses

BOLD=\e[1m
NC=\e[0m

all: dirs $(BUILD)/simulator $(BUILD)/monitor $(BUILD)/leader $(BUILD)/spaceship

clean:
	@rm -rfv $(BUILD)

dirs:
	@mkdir -pv $(BUILD)

$(BUILD)/simulator: $(LIB)/map.c  $(SRC)/simulator.c
	$(CC) $(CFLAGS) $^ -o $@ -lrt -lm -lncurses

$(BUILD)/monitor: $(LIB)/map.c  $(LIB)/gamescreen.c $(SRC)/monitor.c
	$(CC) $(CFLAGS) $^ -o $@ -lrt -lm -lncurses

$(BUILD)/leader: $(SRC)/leader.c
	$(CC) $(CFLAGS) $^ -o $@ -lrt -lm -lncurses

$(BUILD)/spaceship: $(LIB)/map.c $(SRC)/spaceship.c
	$(CC) $(CFLAGS) $^ -o $@ -lrt -lm -lncurses
