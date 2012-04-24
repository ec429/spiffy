/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	audio.h - audio functions
*/

#ifdef AUDIO
#include <stdio.h>
#include <stdbool.h>
#include <SDL.h>
#define MAX_SINC_RATE	32
#define SAMPLE_RATE		8000 // Audio sample rate, Hz
#define AUDIOBUFLEN		(SAMPLE_RATE/100)
#define MAX_SINCBUFLEN	(AUDIOBUFLEN*MAX_SINC_RATE)
#define SINCBUFLEN		(AUDIOBUFLEN**sinc_rate)
unsigned char *get_sinc_rate(void);
void update_sinc(unsigned char filterfactor);
void mixaudio(void *abuf, Uint8 *stream, int len);
typedef struct
{
	bool bits[MAX_SINCBUFLEN];
	bool cbuf[MAX_SINCBUFLEN];
	unsigned int rp, wp; // read & write pointers for 'bits' circular buffer
	bool play; // true if tape is playing (we mute and allow skipping)
	FILE *record;
}
audiobuf;

double sincgroups[MAX_SINC_RATE][AUDIOBUFLEN];

void wavheader(FILE *a);
#endif /* AUDIO */
