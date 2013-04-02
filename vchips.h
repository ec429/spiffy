#pragma once
#include "z80.h"

typedef struct
{
	bool memwait;
	bool iowait;
	bool t1;
	bool ulaplus_enabled;
	unsigned char ulaplus_regsel; // only used when ULAplus enabled
	unsigned char ulaplus_regs[64]; // only used when ULAplus enabled
	unsigned char ulaplus_mode; // only used when ULAplus enabled
	bool timex_enabled; // 8x1 attribute mode, *rampantly* inaccurate no doubt
}
ula_t;

typedef struct
{
	unsigned char key;
	bool twokey;
	unsigned char row[2], col[2];
}
keymap;

unsigned long int ramtop;
unsigned int nkmaps;
keymap *kmap;

void do_ram(unsigned char RAM[65536], bus_t *bus, bool wrom);
int init_keyboard(void);
void mapk(unsigned char k, bool kstate[8][5], bool down);

#define PORTFE_MIC	0x08
#define PORTFE_SPEAKER	0x10
