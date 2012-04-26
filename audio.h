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
	unsigned char bits[MAX_SINCBUFLEN];
	unsigned char cbuf[MAX_SINCBUFLEN];
	unsigned int rp, wp; // read & write pointers for 'bits' circular buffer
	bool play; // true if tape is playing (we mute and allow skipping)
	FILE *record;
}
audiobuf;

double sincgroups[MAX_SINC_RATE][AUDIOBUFLEN];

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
}
ay_t;

ay_t ay;

void ay_init(ay_t *ay);
void ay_tstep(ay_t *ay, unsigned int steps);
