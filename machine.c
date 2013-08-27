/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	machine.c - machine variants
*/

#include "machine.h"
#include <stdint.h>
#include <string.h>

const char *name[_MACHINES]={	[MACHINE_48]="48",
								[MACHINE_128]="128",
							};
machine machine_from_name(const char *n)
{
	for(machine m=0;m<_MACHINES;m++)
		if(!strcmp(n, name[m])) return(m);
	return(_MACHINES);
}

const char *name_from_machine(machine m)
{
	if(m<_MACHINES) return(name[m]);
	return(NULL);
}

#define CAP_128_PAGING	0x0001
#define CAP_AY			0x0002

uint32_t capabilities[_MACHINES]={	[MACHINE_48]=0,
									[MACHINE_128]=CAP_128_PAGING|CAP_AY,
								};
int rom_len[_MACHINES]={[MACHINE_48]=1, [MACHINE_128]=2,};
const char *def_rom[_MACHINES]={[MACHINE_48]="48.rom", [MACHINE_128]="128.rom",};

bool cap_128_paging(machine m)
{
	return(capabilities[m]&CAP_128_PAGING);
}

bool cap_ay(machine m)
{
	return(capabilities[m]&CAP_AY);
}

int rom_length(machine m)
{
	return(rom_len[m]);
}

const char *default_rom(machine m)
{
	return(def_rom[m]);
}
