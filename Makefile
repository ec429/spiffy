# Makefile for spiffy
PREFIX := /usr/local
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g -DCORETEST -DPREFIX=\"$(PREFIX)\"
SDL := `sdl-config --libs` -lSDL_ttf
SDLFLAGS := `sdl-config --cflags`
GTK := `pkg-config --libs gtk+-2.0`
GTKFLAGS := `pkg-config --cflags gtk+-2.0`

all: spiffy filechooser

spiffy: spiffy.c ops.o ops.h z80.o z80.h vchips.o vchips.h bits.o bits.h pbm.o pbm.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDLFLAGS) spiffy.c $(LDFLAGS) -o spiffy ops.o z80.o vchips.o bits.o pbm.o -lspectrum $(SDL)

filechooser: filechooser.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GTKFLAGS) filechooser.c $(LDFLAGS) -o filechooser $(GTK)

ops.o: ops.c ops.h z80.h

z80.o: z80.c z80.h ops.h

vchips.o: vchips.c vchips.h z80.h

pbm.o: pbm.c pbm.h bits.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDLFLAGS) -o $@ -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

install: spiffy filechooser
	install -D -m0755 spiffy $(PREFIX)/bin/spiffy
	install -D -m0755 filechooser $(PREFIX)/bin/spiffy-filechooser
	install -D -m0644 keymap $(PREFIX)/share/spiffy/keymap
	install -D -m0644 Vera.ttf $(PREFIX)/share/fonts/
	install -D -m0644 *.rom -t $(PREFIX)/share/spiffy/
	install -d -m0755 buttons $(PREFIX)/share/spiffy/buttons
	install -D -m0644 buttons/* -t $(PREFIX)/share/spiffy/buttons
