/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	debug.c - debugger functions
*/

#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "bits.h"
#include "basic.h"

char typename(debugtype type)
{
	switch(type)
	{
		case(DEBUGTYPE_BYTE): return('b');
		case(DEBUGTYPE_WORD): return('w');
		case(DEBUGTYPE_FLOAT): return('f');
		case(DEBUGTYPE_GRID): return('8');
		case(DEBUGTYPE_ROW): return('R');
		case(DEBUGTYPE_ERR): return('E');
		default: return(0);
	}
}

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

double de_float(debugval d)
{
	switch(d.type)
	{
		case(DEBUGTYPE_BYTE): return(d.val.b);
		case(DEBUGTYPE_WORD): return(d.val.w);
		case(DEBUGTYPE_FLOAT): return(d.val.f);
		case(DEBUGTYPE_GRID): /* fallthrough */
		case(DEBUGTYPE_ROW): /* fallthrough */
		case(DEBUGTYPE_ERR): /* fallthrough */
		default: return(nan(""));
	}
}

uint16_t de_word(debugval d)
{
	switch(d.type)
	{
		case(DEBUGTYPE_BYTE): return(d.val.b);
		case(DEBUGTYPE_WORD): return(d.val.w);
		case(DEBUGTYPE_FLOAT): /* fallthrough */
		case(DEBUGTYPE_GRID): /* fallthrough */
		case(DEBUGTYPE_ROW): /* fallthrough */
		case(DEBUGTYPE_ERR): /* fallthrough */
		default: return(0);
	}
}

bool de_isn(debugtype type)
{
	switch(type)
	{
		case(DEBUGTYPE_BYTE): return(true);
		case(DEBUGTYPE_WORD): return(true);
		case(DEBUGTYPE_FLOAT): return(true);
		case(DEBUGTYPE_GRID): /* fallthrough */
		case(DEBUGTYPE_ROW): /* fallthrough */
		case(DEBUGTYPE_ERR): /* fallthrough */
		default: return(false);
	}
}

bool de_isi(debugtype type)
{
	switch(type)
	{
		case(DEBUGTYPE_BYTE): return(true);
		case(DEBUGTYPE_WORD): return(true);
		case(DEBUGTYPE_FLOAT): /* fallthrough */
		case(DEBUGTYPE_GRID): /* fallthrough */
		case(DEBUGTYPE_ROW): /* fallthrough */
		case(DEBUGTYPE_ERR): /* fallthrough */
		default: return(false);
	}
}

int de_cn(debugval *left, debugval *right)
{
	if(!de_isn(left->type)) return(1);
	if(!de_isn(right->type)) return(1);
	if((left->type==DEBUGTYPE_FLOAT)||(right->type==DEBUGTYPE_FLOAT))
	{
		left->val.f=de_float(*left);
		left->type=DEBUGTYPE_FLOAT;
		right->val.f=de_float(*right);
		right->type=DEBUGTYPE_FLOAT;
		return(0);
	}
	else if((left->type==DEBUGTYPE_WORD)||(right->type==DEBUGTYPE_WORD))
	{
		left->val.w=de_word(*left);
		left->type=DEBUGTYPE_WORD;
		right->val.w=de_word(*right);
		right->type=DEBUGTYPE_WORD;
		return(0);
	}
	else if((left->type==DEBUGTYPE_BYTE)&&(right->type==DEBUGTYPE_BYTE))
	{
		return(0);
	}
	else
		return(1);
}

debugval de_recursive(FILE *f, int *e, int ec, const char *const ev[256], unsigned char *RAM, z80 *cpu)
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
			debugval val=de_recursive(f, e, ec, ev, RAM, cpu);
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
					fprintf(f, "error: ~: invalid type %c\n", typename(val.type));
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
				case DEBUGTYPE_ERR:
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
				default:
					fprintf(f, "error: ~: unrecognised type %c\n", typename(val.type));
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
		}
		break;
		case '+':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: +: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)+de_float(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)+de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b+right.val.b}, NULL});
			fprintf(f, "error: +: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '-':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: -: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)-de_float(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)-de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b-right.val.b}, NULL});
			fprintf(f, "error: -: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '*':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: *: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)*de_float(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)*de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b*right.val.b}, NULL});
			fprintf(f, "error: *: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '/':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: /: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)/de_float(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)/de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b/right.val.b}, NULL});
			fprintf(f, "error: /: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '%':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: %%: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: %%: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)%de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b%right.val.b}, NULL});
			fprintf(f, "error: %%: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '&':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: &: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: &: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)&de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b&right.val.b}, NULL});
			fprintf(f, "error: &: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '|':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: |: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: |: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)|de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b|right.val.b}, NULL});
			fprintf(f, "error: |: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '^':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, RAM, cpu);
			debugval right=de_recursive(f, e, ec, ev, RAM, cpu);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: ^: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: ^: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)^de_word(right)}, NULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b^right.val.b}, NULL});
			fprintf(f, "error: ^: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '\'':
		{
			char type=ev[*e][1];
			(*e)++;
			debugval val=de_recursive(f, e, ec, ev, RAM, cpu);
			if(val.type==DEBUGTYPE_ERR)
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			switch(type)
			{
				case 'b':
					if(de_isn(val.type))
						return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=floor(de_float(val))}, NULL});
					else if((val.type==DEBUGTYPE_GRID)||(val.type==DEBUGTYPE_ROW))
						return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=val.val.r[0]}, val.p});
				break;
				case 'w':
					if(de_isn(val.type))
						return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=floor(de_float(val))}, NULL});
					else if((val.type==DEBUGTYPE_GRID)||(val.type==DEBUGTYPE_ROW))
						return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=val.val.r[0]|(val.val.r[1]<<8)}, val.p});
				break;
				case 'f':
					if(de_isn(val.type))
						return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(val)}, NULL});
					else if((val.type==DEBUGTYPE_GRID)||(val.type==DEBUGTYPE_ROW))
					{
						fprintf(f, "error: ': cannot cast %c to f\n", typename(val.type));
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
					}
				break;
				case '8':
					if(val.type==DEBUGTYPE_GRID) return(val);
					else if(val.type==DEBUGTYPE_ROW)
					{
						debugval rv;
						rv.type=DEBUGTYPE_GRID;
						memcpy(rv.val.r, val.val.r, 8);
						rv.p=val.p;
						return(rv);
					}
					else if(de_isn(val.type))
					{
						fprintf(f, "error: ': cannot cast %c to 8\n", typename(val.type));
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
					}
				break;
				case 'R':
					if(val.type==DEBUGTYPE_ROW) return(val);
					else if(val.type==DEBUGTYPE_GRID)
					{
						debugval rv;
						rv.type=DEBUGTYPE_ROW;
						memcpy(rv.val.r, val.val.r, 8);
						memset(rv.val.r+8, 0, 8);
						rv.p=NULL;
						return(rv);
					}
				break;
				default:
					fprintf(f, "error: ': unrecognised type %c\n", type);
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
				break;
			}
			fprintf(f, "error: ': unrecognised operand type %c\n", typename(val.type));
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
		}
		break;
		case '.':
		{
			char type=ev[*e][1];
			(*e)++;
			debugval addr=de_recursive(f, e, ec, ev, RAM, cpu);
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
						fprintf(f, "error: .: Unrecognised type %c\n", type);
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
				}
			}
			else
			{
				fprintf(f, "error: .: Address is not of word (or byte) type, is %c\n", typename(addr.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
		}
		break;
		case '#':
		{
			const char *rn=ev[*e]+1;
			(*e)++;
			const char *reglist="AFBCDEHLXxYyIRSPafbcdehl";
			int reg=reg16(rn);
			bool is16=(reg>=0);
			if(strlen(rn)==1)
			{
				const char *p=strchr(reglist, *rn);
				if(p)
					reg=(p+2-reglist)^1;
				is16=false;
			}
			if(reg>=0)
			{
				if(is16)
					return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=cpu->regs[reg]|(cpu->regs[reg+1]<<8)}, cpu->regs+reg});
				else
					return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=cpu->regs[reg]}, cpu->regs+reg});
			}
			else
			{
				fprintf(f, "error: #: No such register %s\n", rn);
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
			}
		}
		break;
	}
	fprintf(f, "error: Unrecognised item %s\n", ev[*e]);
	(*e)++;
	return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, NULL});
}

debugval debugger_expr(FILE *f, int ec, const char *const ev[256], unsigned char *RAM, z80 *cpu)
{
	int e=0;
	debugval rv=de_recursive(f, &e, ec, ev, RAM, cpu);
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
