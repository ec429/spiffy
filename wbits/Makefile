# Makefile for spiffy, Windows build
PREFIX := .
CC := i586-mingw32msvc-gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g -DCORETEST -DPREFIX=\"$(PREFIX)\" -fgnu89-inline
CPPFLAGS := -I. -Dusleep\(v\)=Sleep\(\(v\)/1000\)
LDFLAGS := -L.
SDL := `/usr/i586-mingw32msvc/bin/sdl-config --libs` -lSDL_ttf
SDLFLAGS := -I/usr/i586-mingw32msvc/include -I/usr/i586-mingw32msvc/include/SDL `/usr/i586-mingw32msvc/bin/sdl-config --cflags`
GTK := `/usr/i586-mingw32msvc/bin/pkg-config --libs gtk+-2.0`
GTKFLAGS := `/usr/i586-mingw32msvc/bin/pkg-config --cflags gtk+-2.0`
VERSION := `git describe --tags`

all: spiffy.exe spiffy-filechooser.exe

spiffy.exe: spiffy.c ops.o ops.h z80.o z80.h vchips.o vchips.h bits.o bits.h pbm.o pbm.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDLFLAGS) spiffy.c $(LDFLAGS) -o spiffy.exe ops.o z80.o vchips.o bits.o pbm.o -lspectrum $(SDL)

spiffy-filechooser.exe: filechooser.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GTKFLAGS) filechooser.c $(LDFLAGS) -o spiffy-filechooser.exe $(GTK)

ops.o: ops.c ops.h z80.h

z80.o: z80.c z80.h ops.h

vchips.o: vchips.c vchips.h z80.h

pbm.o: pbm.c pbm.h bits.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDLFLAGS) -o $@ -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

install: spiffy.exe spiffy-filechooser.exe
	install -D -m0755 spiffy.exe $(PREFIX)/bin/spiffy.exe
	install -D -m0755 spiffy-filechooser.exe $(PREFIX)/bin/spiffy-filechooser.exe
	install -D -m0644 keymap $(PREFIX)/share/spiffy/keymap
	install -D -m0644 Vera.ttf $(PREFIX)/share/fonts/
	install -D -m0644 *.rom -t $(PREFIX)/share/spiffy/
	install -d -m0755 buttons $(PREFIX)/share/spiffy/buttons
	install -D -m0644 buttons/* -t $(PREFIX)/share/spiffy/buttons

