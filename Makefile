# Makefile for spiffy
CC := gcc
CFLAGS := -Wall -Wextra -pedantic --std=gnu99 -g -DCORETEST
SDL := `sdl-config --cflags --libs` -lSDL_ttf

all: spiffy

spiffy: spiffy.c ops.o ops.h z80.o z80.h vchips.o vchips.h
	$(CC) $(CFLAGS) -o spiffy spiffy.c $(SDL) ops.o z80.o vchips.o

ops.o: ops.c ops.h z80.h

z80.o: z80.c z80.h ops.h

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

