/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	debug.c - debugger functions
*/

#include "debug.h"
#include <stdio.h>
#include <string.h>
#include "bits.h"
#include "basic.h"

void show_state(const unsigned char * RAM, const z80 *cpu, int Tstates, const bus_t *bus)
{
	int i;
	printf("\nState: %c%c%c%c%c%c%c%c\n", cpu->regs[2]&0x80?'S':'-', cpu->regs[2]&0x40?'Z':'-', cpu->regs[2]&0x20?'5':'-', cpu->regs[2]&0x10?'H':'-', cpu->regs[2]&0x08?'3':'-', cpu->regs[2]&0x04?'P':'-', cpu->regs[2]&0x02?'N':'-', cpu->regs[2]&0x01?'C':'-');
	printf("P C  A F  B C  D E  H L  I x  I y  I R  S P  a f  b c  d e  h l  IFF IM\n");
	for(i=0;i<26;i+=2)
	{
		printf("%02x%02x ", cpu->regs[i+1], cpu->regs[i]);
	}
	printf("%c %c %1x\n", cpu->IFF[0]?'1':'0', cpu->IFF[1]?'1':'0', cpu->intmode);
	printf("\n");
	printf("Memory\t- near (PC), (HL) and (SP):\n");
	for(i=0;i<5;i++)
	{
		unsigned short int off = (*PC) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = (*HL) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = (*SP) + 2*(i);
		printf("%04x: %02x%02x\n", (unsigned short int)off, RAM[(off+1)%(1<<16)], RAM[off]);
	}
	printf("\t- near (BC), (DE) and (nn):\n");
	for(i=0;i<5;i++)
	{
		unsigned short int off = (*BC) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = (*DE) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = I16 + 4*(i-2);
		off-=off%4;
		printf("%04x: %02x%02x %02x%02x\n", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3]);
	}
	printf("\n");
	printf("T-states: %u\tM-cycle: %u[%d]\tInternal regs: %02x-%02x-%02x\tShift state: %u", Tstates, cpu->M, cpu->dT, cpu->internal[0], cpu->internal[1], cpu->internal[2], cpu->shiftstate);
	if(cpu->nmiacc) printf(" NMI!");
	else if(cpu->intacc) printf(" INT!");
	printf("\n");
	printf("Bus: A=%04x\tD=%02x\t%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", bus->addr, bus->data, bus->tris==TRIS_OUT?"WR":"wr", bus->tris==TRIS_IN?"RD":"rd", bus->mreq?"MREQ":"mreq", bus->iorq?"IORQ":"iorq", bus->m1?"M1":"m1", bus->rfsh?"RFSH":"rfsh", bus->waitline?"WAIT":"wait", bus->irq?"INT":"int", bus->nmi?"NMI":"nmi", bus->reset?"RESET":"reset", bus->halt?"HALT":"halt");
}

void mdisplay(unsigned char *RAM, unsigned int addr, const char *what, const char *rest)
{
	if(strcmp(what, "r")==0)
		fprintf(stderr, "[%04x.b]=%02x\n", addr, RAM[addr]);
	else if(strcmp(what, "w")==0)
	{
		unsigned int val;
		if(!(rest&&(sscanf(rest, "%x", &val)==1)))
			val=0;
		RAM[addr]=val;
	}
	else if(strcmp(what, "lr")==0)
		fprintf(stderr, "[%04x.w]=%04x\n", addr, peek16(addr));
	else if(strcmp(what, "lw")==0)
	{
		unsigned int val;
		if(!(rest&&(sscanf(rest, "%x", &val)==1)))
			val=0;
		poke16(addr, val);
	}
	else if(strcmp(what, "fr")==0)
		fprintf(stderr, "[%04x.f]=%g\n", addr, float_decode(RAM, addr));
	else if(strcmp(what, "fw")==0)
	{
		double val;
		if(!(rest&&(sscanf(rest, "%lg", &val)==1)))
			fprintf(stderr, "memory: missing value\n");
		else
			float_encode(RAM, addr, val);
	}
	else
		fprintf(stderr, "memory: bad mode (see 'h m')\n");
}

int reg16(const char *name)
{
	if(!name) return(-1);
	if(strcasecmp(name, "PC")==0) return(0);
	if(strcasecmp(name, "AF")==0) return(2);
	if(strcasecmp(name, "BC")==0) return(4);
	if(strcasecmp(name, "DE")==0) return(6);
	if(strcasecmp(name, "HL")==0) return(8);
	if(strcasecmp(name, "IX")==0) return(10);
	if(strcasecmp(name, "IY")==0) return(12);
	if(strcasecmp(name, "SP")==0) return(16);
	if(strcasecmp(name, "AF'")==0) return(18);
	if(strcasecmp(name, "BC'")==0) return(20);
	if(strcasecmp(name, "DE'")==0) return(22);
	if(strcasecmp(name, "HL'")==0) return(24);
	return(-1);
}
