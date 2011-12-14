#include "z80.h"

unsigned long int ramtop;

void do_ram(unsigned char RAM[65536], bus_t *bus, bool wrom);
