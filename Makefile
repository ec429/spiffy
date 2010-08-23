# Makefile for spiffy
CC ?= gcc
CFLAGS ?= -Wall
SDL = `sdl-config --cflags --libs`

all: spiffy

spiffy: spiffy.c ops.o ops.h
	$(CC) $(CFLAGS) -o spiffy spiffy.c $(SDL) ops.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -o $@ -c $<

