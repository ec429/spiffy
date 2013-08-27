/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	coretest.c - Z80 core tests
	
	Acknowledgements: Based on the FUSE coretests by Phil Kendall <philip@shadowmagic.org.uk>
		<http://fuse-emulator.sourceforge.net/>
*/

#include "coretest.h"
#include <string.h>
#include <errno.h>
#include "ops.h"
#include "vchips.h"

int read_test( FILE *f, unsigned int *end_tstates, z80 *cpu, uint8_t *memory )
{
	const char *progname="spiffy";
	unsigned af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc;
	unsigned i, r, iff1, iff2, im;
	unsigned end_tstates2;
	unsigned address;
	char test_name[ 80 ];

	do {

		if( !fgets( test_name, sizeof( test_name ), f ) ) {

			if( feof( f ) ) return 1;

			fprintf( stderr, "%s: error reading test description from file: %s\n",
				 progname, strerror( errno ) );
			return 1;
		}

	} while( test_name[0] == '\n' );

	if( fscanf( f, "%x %x %x %x %x %x %x %x %x %x %x %x", &af, &bc,
				&de, &hl, &af_, &bc_, &de_, &hl_, &ix, &iy, &sp, &pc ) != 12 ) {
		fprintf( stderr, "%s: first registers line in file corrupt\n", progname);
		return 1;
	}

	*AF	= af;	*BC	= bc;	*DE	= de;	*HL	= hl;
	*AF_ = af_; *BC_ = bc_; *DE_ = de_; *HL_ = hl_;
	*Ix	= ix;	*Iy	= iy;	*SP	= sp;	*PC	= pc;

	int halted;
	if( fscanf( f, "%x %x %u %u %u %d %u", &i, &r, &iff1, &iff2, &im,
				&halted, &end_tstates2 ) != 7 ) {
		fprintf( stderr, "%s: second registers line in file corrupt\n", progname);
		return 1;
	}

	*Intvec = i; *Refresh = r; cpu->IFF[0] = iff1; cpu->IFF[1] = iff2; cpu->intmode = im; cpu->halt=halted;
	*end_tstates = end_tstates2;

	while( 1 ) {

		if( fscanf( f, "%x", &address ) != 1 ) {
			fprintf( stderr, "%s: no address found in file\n", progname);
			return 1;
		}

		if( address >= 0x10000 ) break;

		while( 1 ) {

			unsigned byte;

			if( fscanf( f, "%x", &byte ) != 1 ) {
	fprintf( stderr, "%s: no data byte found in file\n", progname);
	return 1;
			}
		
			if( byte >= 0x100 ) break;

			memory[ address++ ] = byte;

		}
	}

	printf( "%s", test_name );

	return 0;
}

void dump_z80_state( z80 *cpu, unsigned int tstates )
{
	printf( "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
		*AF, *BC, *DE, *HL, *AF_, *BC_, *DE_, *HL_, *Ix, *Iy, *SP, *PC );
	printf( "%02x %02x %d %d %d %d %d\n", *Intvec, *Refresh,
		cpu->IFF[0], cpu->IFF[1], cpu->intmode, cpu->halt, tstates );
}

void dump_memory_state( uint8_t *memory, uint8_t *initial_memory )
{
	size_t i;

	for( i = 0; i < 0x10000; i++ ) {

		if( memory[ i ] == initial_memory[ i ] ) continue;

		printf( "%04x ", (unsigned)i );

		while( i < 0x10000 && memory[ i ] != initial_memory[ i ] )
			printf( "%02x ", memory[ i++ ] );

		printf( "-1\n" );
	}
}

int run_test(FILE *f)
{
	size_t i;
	unsigned int tstates=0;
	uint8_t memory[0x10000], initial_memory[0x10000];
	for( i = 0; i < 0x10000; i += 4 ) {
		memory[ i     ] = 0xde; memory[ i + 1 ] = 0xad;
		memory[ i + 2 ] = 0xbe; memory[ i + 3 ] = 0xef;
	}

	z80 _cpu, *cpu=&_cpu;
	bus_t _bus, *bus=&_bus;
	ram_t _ram, *ram=&_ram;
	z80_reset(cpu, bus);
	ram_init(ram, NULL, MACHINE_48);
	unsigned int end_tstates;
	if( read_test( f, &end_tstates, cpu, memory ) ) return 0;
	/* fill in the ram_t */
	for(unsigned int i=0;i<0x10000;i++)
		ram->bank[i>>14][i&0x3fff]=memory[i];

	/* Grab a copy of the memory for comparison at the end */
	memcpy( initial_memory, memory, 0x10000 );

	int errupt=0;
	while(!errupt)
	{
		do_ram(ram, bus);
		fflush(stdout);
		if(cpu->nothing)
		{
			cpu->nothing--;
			if(cpu->steps)
				cpu->steps--;
			cpu->dT++;
		}
		else
			errupt=z80_tstep(cpu, bus, errupt);
		fflush(stderr);
		if(++tstates>=end_tstates)
		{
			if((cpu->M==0)&&(cpu->dT==0)&&!cpu->block_ints)
				errupt++;
		}
	}
	/* copy memory back out of the ram_t */
	for(unsigned int i=0;i<0x10000;i++)
		memory[i]=ram->bank[i>>14][i&0x3fff];

	/* And dump our final state */
	dump_z80_state(cpu, tstates);
	dump_memory_state(memory, initial_memory);

	printf( "\n" );

	return 1;
}
