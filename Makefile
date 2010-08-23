# Makefile for spiffy
CC ?= gcc
CFLAGS ?= -Wall
SDL = `sdl-config --cflags --libs`

all: spiffy

spiffy: spiffy.c
	$(CC) $(CFLAGS) -o spiffy spiffy.c $(SDL)

