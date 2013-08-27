/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	filters.h - graphics filters
*/

#include <stdbool.h>
#include <stdint.h>

#define FILT_BW		0x01
#define FILT_SCAN	0x02
#define FILT_BLUR	0x04
#define FILT_VBLUR	0x08
#define FILT_MISG	0x10
#define FILT_SLOW	0x20
#define FILT_PAL	0x40

const char *filter_name(unsigned int filt_id);
void filter_pix(unsigned int filt_mask, unsigned int x, unsigned int y, uint8_t *r, uint8_t *g, uint8_t *b);
