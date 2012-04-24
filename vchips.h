#include "z80.h"

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

#define PORTFE_MIC	0x08
#define PORTFE_SPEAKER	0x10
