#pragma once
/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	machine.h - machine variants
*/

#include <stdbool.h>

typedef enum
{
	MACHINE_48, // Standard 48k Spectrum
	MACHINE_128, // Original 128k Spectrum

	_MACHINES
}
machine;

machine machine_from_name(const char *n);
const char *name_from_machine(machine m);

bool cap_128_paging(machine m);
bool cap_128_ula_timings(machine m);
bool cap_ay(machine m);
int frame_length(machine m);
int rom_length(machine m);
const char *default_rom(machine m);
