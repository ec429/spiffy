/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	coretest.h - Z80 core tests
	
	Acknowledgements: Based on the FUSE coretests by Phil Kendall <philip@shadowmagic.org.uk>
		<http://fuse-emulator.sourceforge.net/>
*/

#include <stdio.h>
#include "z80.h"

int read_test( FILE *f, unsigned int *end_tstates, z80 *cpu, unsigned char *memory);
void dump_z80_state( z80 *cpu, unsigned int tstates );
void dump_memory_state( unsigned char *memory, unsigned char *initial_memory );
int run_test( FILE *f );
