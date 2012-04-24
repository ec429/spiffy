# Makefile for spiffy
PREFIX := /usr/local
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g -DCORETEST -DPREFIX=\"$(PREFIX)\"
SDL := `sdl-config --libs` -lSDL_ttf
SDLFLAGS := `sdl-config --cflags`
GTK := `pkg-config --libs gtk+-2.0`
GTKFLAGS := `pkg-config --cflags gtk+-2.0`
VERSION := `git describe --tags`
LIBS := ops.o z80.o vchips.o bits.o pbm.o sysvars.o basic.o
INCLUDES := ops.h z80.h vchips.h bits.h pbm.h sysvars.h basic.h

all: spiffy spiffy-filechooser

spiffy: spiffy.c $(INCLUDES) $(LIBS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDLFLAGS) spiffy.c $(LDFLAGS) -o spiffy $(LIBS) -lspectrum $(SDL)

spiffy-filechooser: filechooser.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GTKFLAGS) filechooser.c $(LDFLAGS) -o spiffy-filechooser $(GTK)

ops.o: ops.c ops.h z80.h

z80.o: z80.c z80.h ops.h

vchips.o: vchips.c vchips.h z80.h

pbm.o: pbm.c pbm.h bits.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(SDLFLAGS) -o $@ -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ -c $<

install: spiffy spiffy-filechooser
	install -D -m0755 spiffy $(PREFIX)/bin/spiffy
	install -D -m0755 spiffy-filechooser $(PREFIX)/bin/spiffy-filechooser
	install -D -m0644 keymap $(PREFIX)/share/spiffy/keymap
	install -D -m0644 Vera.ttf $(PREFIX)/share/fonts/
	install -D -m0644 *.rom -t $(PREFIX)/share/spiffy/
	install -d -m0755 buttons $(PREFIX)/share/spiffy/buttons
	install -D -m0644 buttons/* -t $(PREFIX)/share/spiffy/buttons

dist: all
	mkdir spiffy_$(VERSION)
	for p in $$(ls); do cp $$p spiffy_$(VERSION)/$$p; done;
	cp -r buttons spiffy_$(VERSION)/
	-rm spiffy_$(VERSION)/*.tgz
	-rm -r spiffy_$(VERSION)/wbits
	tar -czf spiffy_$(VERSION).tgz spiffy_$(VERSION)/
	rm -r spiffy_$(VERSION)/

distw: all
	mkdir spiffy_w$(VERSION)
	for p in $$(ls); do cp $$p spiffy_w$(VERSION)/$$p; done;
	-for p in $$(ls wbits); do cp wbits/$$p spiffy_w$(VERSION)/$$p; done;
	cp -r buttons spiffy_w$(VERSION)/
	-rm spiffy_w$(VERSION)/*.tgz
	rm spiffy_w$(VERSION)/*.o
	rm spiffy_w$(VERSION)/spiffy
	rm spiffy_w$(VERSION)/spiffy-filechooser
	make -C spiffy_w$(VERSION) -fMakefile all

