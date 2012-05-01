/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	debug.c - debugger functions
*/

#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bits.h"
#include "basic.h"

void debugger_tokenise(char *line, int *drgc, char *drgv[256])
{
	*drgc=0;
	if(!line)
	{
		drgv[0]=NULL;
	}
	else
	{
		char *p=strtok(line, " ");
		while(p)
		{
			drgv[(*drgc)++]=p;
			p=strtok(NULL, " ");
		}
		drgv[*drgc]=NULL;
	}
}

debugval de_recursive(FILE *f, int *e, int ec, const char *const ev[256], unsigned char *RAM)
{
	if((*e>=ec)||(!ev[*e]))
	{
		fprintf(f, "error: Stack underflow\n");
		return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
	}
	if(isxdigit(ev[*e][0]))
	{
		size_t l=0;
		while(isxdigit(ev[*e][++l]));
		unsigned int v;
		sscanf(ev[*e], "%x", &v);
		(*e)++;
		if(l<=2)
			return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=v}, NULL});
		if(l<=4)
			return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=v}, NULL});
		fprintf(f, "error: Hex literal exceeds 2 bytes: %s\n", ev[*e]);
		return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
	}
	else switch(ev[*e][0])
	{
		case '~':
		{
			(*e)++;
			debugval val=de_recursive(f, e, ec, ev, RAM);
			switch(val.type)
			{
				case DEBUGTYPE_BYTE:
					return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=val.val.b^0xFF}, NULL});
				case DEBUGTYPE_WORD:
					return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=val.val.w^0xFFFF}, NULL});
				case DEBUGTYPE_GRID:
				{
					debugval rv;
					rv.type=DEBUGTYPE_GRID;
					rv.p=NULL;
					for(unsigned int i=0;i<8;i++)
						rv.val.r[i]=val.val.r[i]^0xFF;
					return(rv);
				}
				case DEBUGTYPE_ROW:
				{
					debugval rv;
					rv.type=DEBUGTYPE_ROW;
					rv.p=NULL;
					for(unsigned int i=0;i<16;i++)
						rv.val.r[i]=val.val.r[i]^0xFF;
					return(rv);
				}
				case DEBUGTYPE_FLOAT:
					fprintf(f, "error: Can't ~ a float\n");
					/* fallthrough */
				default:
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
		}
		break;
		case '.':
		{
			char type=ev[*e][1];
			(*e)++;
			debugval addr=de_recursive(f, e, ec, ev, RAM);
			if(addr.type==DEBUGTYPE_BYTE)
			{
				addr.type=DEBUGTYPE_WORD;
				addr.val.w=addr.val.b; // extend it
			}
			if(addr.type==DEBUGTYPE_WORD)
			{
				switch(type)
				{
					case 'b':
						return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=RAM[addr.val.w]}, RAM+addr.val.w});
					case 'w':
						return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=peek16(addr.val.w)}, RAM+addr.val.w});
					case 'f':
						return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=float_decode(RAM, addr.val.w)}, RAM+addr.val.w});
					case '8':
					{
						debugval rv;
						rv.type=DEBUGTYPE_GRID;
						rv.p=RAM+addr.val.w;
						memcpy(rv.val.r, rv.p, 8);
						return(rv);
					}
					case 'R':
					{
						debugval rv;
						rv.type=DEBUGTYPE_ROW;
						rv.p=RAM+addr.val.w;
						memcpy(rv.val.r, rv.p, 16);
						return(rv);
					}
					default:
						fprintf(f, "error: Unrecognised type %s\n", ev[*e]);
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
				}
			}
		}
		break;
	}
	fprintf(f, "error: Unrecognised item %s\n", ev[*e]);
	(*e)++;
	return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
}

debugval debugger_expr(FILE *f, int ec, const char *const ev[256], unsigned char *RAM)
{
	int e=0;
	debugval rv=de_recursive(f, &e, ec, ev, RAM);
	if(e<ec) fprintf(f, "warning: %d input items were not consumed\n", ec-e);
	return(rv);
}

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

void debugval_display(FILE *f, debugval val)
{
	switch(val.type)
	{
		case DEBUGTYPE_BYTE:
			fprintf(f, "%02x\n", val.val.b);
		break;
		case DEBUGTYPE_WORD:
			fprintf(f, "%04x\n", val.val.w);
		break;
		case DEBUGTYPE_FLOAT:
			fprintf(f, "%g\n", val.val.f);
		break;
		case DEBUGTYPE_GRID:
			for(unsigned int i=0;i<8;i++)
			{
				fprintf(f, "grid:\n");
				fprintf(f, "%02x = ", val.val.r[i]);
				for(unsigned int b=0;b<8;b++)
					fputc(((val.val.r[i]<<b)&0x80)?'1':'0', f);
				fprintf(f, "b\n");
			}
		break;
		case DEBUGTYPE_ROW:
			for(unsigned int i=0;i<16;i++)
			{
				if(i)
					fputc(' ', f);
				if(!(i&3))
					fputc(' ', f);
				fprintf(f, "%02x", val.val.r[i]);
			}
			fputc('\n', f);
		break;
		case DEBUGTYPE_ERR:
			fprintf(f, "(Error)\n");
		break;
	}
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
