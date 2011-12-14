#include "vchips.h"
#include <stdlib.h>
#include <math.h>

void do_ram(unsigned char RAM[65536], bus_t *bus, bool wrom)
{
	if(bus->mreq&&(bus->tris==IN))
	{
		if(bus->addr<ramtop)
		{
			bus->data=RAM[bus->addr];
		}
		else
		{
			bus->data=floor(rand()*256.0/RAND_MAX);
		}
	}
	else if(bus->mreq&&(bus->tris==OUT))
	{
		if((wrom||(bus->addr&0xC000))&&(bus->addr<ramtop))
			RAM[bus->addr]=bus->data;
	}
}
