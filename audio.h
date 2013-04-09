#pragma once
/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	audio.h - audio functions
*/

#include <stdio.h>
#include <stdbool.h>
#ifdef AUDIO
#include <SDL.h>
#define MAX_SINC_RATE	32
#define SAMPLE_RATE		8000 // Audio sample rate, Hz
#define AUDIOBUFLEN		(256)
#define AUDIOBITBUFLEN	((SAMPLE_RATE*MAX_SINC_RATE)/16)
#define AUDIOBITLEN		((SAMPLE_RATE**sinc_rate)/16)
#define AUDIOSYNCLEN	(SAMPLE_RATE/40)
#define MAX_SINCBUFLEN	(AUDIOSYNCLEN*MAX_SINC_RATE)
#define SINCBUFLEN		(AUDIOSYNCLEN**sinc_rate)
#define AUDIO_WAIT		5e3
#define AUDIO_MAXWAITS	40
unsigned char *get_sinc_rate(void);
void update_sinc(unsigned char filterfactor);
void mixaudio(void *abuf, Uint8 *stream, int len);
typedef struct
{
	unsigned char bits[AUDIOBITBUFLEN];
	unsigned char cbuf[MAX_SINCBUFLEN];
	unsigned int rp, wp; // read & write pointers for 'bits' circular buffer
	unsigned int crp, cwp; // read & write pointers for 'cbuf' circular buffer
	bool play; // true if tape is playing (we mute and allow skipping)
	FILE *record;
	bool busy[2]; // true if [core, audio] thread is using.  see file 'sound' for shutdown sequence
}
audiobuf;

double sincgroups[MAX_SINC_RATE][AUDIOSYNCLEN];

void wavheader(FILE *a);
#endif /* AUDIO */

bool ay_enabled;

typedef struct
{
	unsigned char reg[16]; // The programmable registers R0-R15
	unsigned char regsel; // the selected register for reading/writing
	bool bit[3]; // output high? A/B/C
	unsigned int count[3]; // counters A/B/C
	unsigned int envcount; // counter for envelope
	unsigned char env; // envelope magnitude
	bool envstop; // envelope stopped?
	bool envrev; // envelope direction reversed?
	unsigned char out[3]; // final output level A/B/C
	unsigned int noise; // internal noise register
	unsigned int noisecount; // counter for noise
}
ay_t;

ay_t ay;

void ay_init(ay_t *ay);
void ay_tstep(ay_t *ay, unsigned int steps);
