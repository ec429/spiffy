# Makefile for spiffy
CC ?= gcc
CFLAGS ?= -Wall
SDL = `sdl-config --cflags --libs` -lSDL_ttf

all: spiffy

spiffy: spiffy.c ops.o ops.h z80.h
	$(CC) $(CFLAGS) -o spiffy spiffy.c $(SDL) ops.o

ops.o: ops.c ops.h z80.h

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

