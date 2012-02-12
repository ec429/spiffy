# Makefile for spiffy
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g -DCORETEST
SDL := `sdl-config --cflags --libs` -lSDL_ttf

all: spiffy

spiffy: spiffy.c ops.o ops.h z80.o z80.h vchips.o vchips.h bits.o bits.h pbm.o pbm.h
	$(CC) $(CFLAGS) $(CPPFLAGS) spiffy.c $(SDL) $(LDFLAGS) -o spiffy ops.o z80.o vchips.o bits.o pbm.o -lspectrum

ops.o: ops.c ops.h z80.h

z80.o: z80.c z80.h ops.h

vchips.o: vchips.c vchips.h z80.h

pbm.o: pbm.c pbm.h bits.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $< $(SDL)

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

