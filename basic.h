/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	basic.h - BASIC debugging functions
*/

#include "bits.h"
#include "vchips.h"

typedef struct
{
	uint16_t number;
	string line;
	uint16_t addr;
}
bas_line;
int compare_bas_line(const void *, const void *);

const char *baschar(uint8_t c);
double float_decode(const uint8_t *buf);
void float_encode(uint8_t *buf, double val);
