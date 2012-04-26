/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	filters.h - graphics filters
*/

#include <stdbool.h>

#define FILT_BW		1
#define FILT_SCAN	2

const char *filter_name(unsigned int filt_id);
void filter_pix(unsigned int filt_mask, unsigned int x, unsigned int y, unsigned char pix, bool bright, unsigned char *r, unsigned char *g, unsigned char *b);
