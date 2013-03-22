/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	basic.h - BASIC debugging functions
*/

#include "bits.h"

typedef struct
{
	unsigned short int number;
	string line;
	unsigned short int addr;
}
bas_line;
int compare_bas_line(const void *, const void *);

const char *baschar(unsigned char c);
double float_decode(const unsigned char *RAM, unsigned int addr);
void float_encode(unsigned char *RAM, unsigned int addr, double val);
