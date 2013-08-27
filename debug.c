/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	debug.c - debugger functions
*/

#include "debug.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "bits.h"
#include "basic.h"
#include "sysvars.h"

#define DNULL	(debugaddr){.type=DEBUGADDR_NULL}

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

size_t typelength(debugtype type)
{
	switch(type)
	{
		case(DEBUGTYPE_BYTE): return(1);
		case(DEBUGTYPE_WORD): return(2);
		case(DEBUGTYPE_FLOAT): return(5);
		case(DEBUGTYPE_GRID): return(8);
		case(DEBUGTYPE_ROW): return(16);
		case(DEBUGTYPE_ERR): /* fallthru */
		default: return(0);
	}
}

void dwrite(debugaddr addr, debugval val, debugctx ctx)
{
	if(addr.type==DEBUGADDR_CPU)
	{
		if(ctx.cpu)
		{
			uint16_t value;
			switch(val.type)
			{
				case DEBUGTYPE_BYTE:
					value=val.val.b;
				break;
				case DEBUGTYPE_WORD:
					value=val.val.w;
				break;
				default:
					return;
			}
			if(addr.page)
			{
				if(addr.addr<25)
				{
					ctx.cpu->regs[addr.addr]=value;
					ctx.cpu->regs[addr.addr+1]=value>>8;
				}
			}
			else
			{
				if((addr.addr<26)&&(val.type==DEBUGTYPE_BYTE))
					ctx.cpu->regs[addr.addr]=value;
			}
		}
		return;
	}
	size_t len=typelength(val.type);
	uint8_t bytes[len];
	switch(val.type)
	{
		case DEBUGTYPE_BYTE:
			bytes[0]=val.val.b;
		break;
		case DEBUGTYPE_WORD:
			bytes[0]=val.val.w;
			bytes[1]=val.val.w>>8;
		break;
		case DEBUGTYPE_FLOAT:
			float_encode(bytes, val.val.f);
		break;
		case DEBUGTYPE_GRID: /* thru */
		case DEBUGTYPE_ROW:
			memcpy(bytes, val.val.r, len);
		break;
		default:
			return;
	}
	switch(addr.type)
	{
		case DEBUGADDR_NULL:
			return;
		case DEBUGADDR_MAIN:
			ram_write_bytes(ctx.ram, addr.addr, len, bytes);
		break;
		case DEBUGADDR_PAGE:
			if(ctx.ram)
				for(size_t i=0;i<len;i++)
				{
					uint16_t page=(addr.addr+i)>>14, offset=(addr.addr+i)&0x3fff;
					if(page<ctx.ram->banks)
						ctx.ram->bank[page][offset]=bytes[i];
				}
		break;
		case DEBUGADDR_ULAPLUS:
			if(ctx.ula)
				for(size_t i=0;i<len;i++)
				{
					uint32_t ua=addr.addr+i;
					if(ua<64)
						ctx.ula->ulaplus_regs[ua]=bytes[i];
				}
		break;
		case DEBUGADDR_AY:
			if(ctx.ay)
				for(size_t i=0;i<len;i++)
				{
					uint32_t aa=addr.addr+i;
					if(aa<16)
						ctx.ay->reg[aa]=bytes[i];
				}
		break;
		default:
			return;
	}
}

debugval dread(debugaddr addr, debugtype type, debugctx ctx)
{
	size_t len=typelength(type);
	uint8_t bytes[len];
	memset(bytes, 0xff, len);
	switch(addr.type)
	{
		case DEBUGADDR_MAIN:
			ram_read_bytes(ctx.ram, addr.addr, len, bytes);
		break;
		case DEBUGADDR_PAGE:
			if(ctx.ram)
				for(size_t i=0;i<len;i++)
				{
					uint16_t page=(addr.addr+i)>>14, offset=(addr.addr+i)&0x3fff;
					if(page<ctx.ram->banks)
						bytes[i]=ctx.ram->bank[page][offset];
				}
		break;
		case DEBUGADDR_ULAPLUS:
			if(ctx.ula)
				for(size_t i=0;i<len;i++)
				{
					uint32_t ua=addr.addr+i;
					if(ua<64)
						bytes[i]=ctx.ula->ulaplus_regs[ua];
				}
		break;
		case DEBUGADDR_AY:
			if(ctx.ay)
				for(size_t i=0;i<len;i++)
				{
					uint32_t aa=addr.addr+i;
					if(aa<16)
						bytes[i]=ctx.ay->reg[aa];
				}
		break;
		case DEBUGADDR_NULL: /* thru */
		default:
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, addr});
	}
	switch(type)
	{
		case DEBUGTYPE_BYTE:
			return((debugval){type, (debugval_val){.b=bytes[0]}, addr});
		break;
		case DEBUGTYPE_WORD:
			return((debugval){type, (debugval_val){.w=bytes[0]|(bytes[1]<<8)}, addr});
		break;
		case DEBUGTYPE_FLOAT:
			return((debugval){type, (debugval_val){.f=float_decode(bytes)}, addr});
		break;
		case DEBUGTYPE_GRID: /* thru */
		case DEBUGTYPE_ROW:
		{
			debugval val=(debugval){type, (debugval_val){.b=0}, addr};
			memcpy(val.val.r, bytes, len);
			return(val);
		}
		break;
		default:
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, addr});
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

debugval de_recursive(FILE *f, int *e, int ec, const char *const ev[256], debugctx ctx)
{
	if((*e>=ec)||(!ev[*e]))
	{
		fprintf(f, "error: Stack underflow\n");
		return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
	}
	if(isxdigit(ev[*e][0]))
	{
		size_t l=0;
		while(isxdigit(ev[*e][++l]));
		unsigned int v;
		sscanf(ev[*e], "%x", &v);
		(*e)++;
		if(l<=2)
			return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=v}, DNULL});
		if(l<=4)
			return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=v}, DNULL});
		fprintf(f, "error: Hex literal exceeds 2 bytes: %s\n", ev[*e]);
		return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
	}
	else switch(ev[*e][0])
	{
		case '_':
		{
			const char *v=ev[*e]+1;
			(*e)++;
			double d;
			if(sscanf(v, "%lf", &d)!=1)
			{
				fprintf(f, "error: _: Value is not float literal: %s\n", v);
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=d}, DNULL});
		}
		break;
		case '~':
		{
			(*e)++;
			debugval val=de_recursive(f, e, ec, ev, ctx);
			switch(val.type)
			{
				case DEBUGTYPE_BYTE:
					return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=val.val.b^0xFF}, DNULL});
				case DEBUGTYPE_WORD:
					return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=val.val.w^0xFFFF}, DNULL});
				case DEBUGTYPE_GRID:
				{
					debugval rv;
					rv.type=DEBUGTYPE_GRID;
					rv.addr=DNULL;
					for(unsigned int i=0;i<8;i++)
						rv.val.r[i]=val.val.r[i]^0xFF;
					return(rv);
				}
				case DEBUGTYPE_ROW:
				{
					debugval rv;
					rv.type=DEBUGTYPE_ROW;
					rv.addr=DNULL;
					for(unsigned int i=0;i<16;i++)
						rv.val.r[i]=val.val.r[i]^0xFF;
					return(rv);
				}
				case DEBUGTYPE_FLOAT:
					fprintf(f, "error: ~: invalid type %c\n", typename(val.type));
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
				case DEBUGTYPE_ERR:
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
				default:
					fprintf(f, "error: ~: unrecognised type %c\n", typename(val.type));
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
		}
		break;
		case '+':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: +: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)+de_float(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)+de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b+right.val.b}, DNULL});
			fprintf(f, "error: +: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '-':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: -: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)-de_float(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)-de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b-right.val.b}, DNULL});
			fprintf(f, "error: -: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '*':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: *: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)*de_float(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)*de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b*right.val.b}, DNULL});
			fprintf(f, "error: *: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '/':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(de_cn(&left, &right))
			{
				fprintf(f, "error: /: could not find common numeric type for %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(left.type==DEBUGTYPE_FLOAT)
			{
				return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(left)/de_float(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_WORD)
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)/de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b/right.val.b}, DNULL});
			fprintf(f, "error: /: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '%':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: %%: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: %%: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)%de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b%right.val.b}, DNULL});
			fprintf(f, "error: %%: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '&':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: &: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: &: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)&de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b&right.val.b}, DNULL});
			fprintf(f, "error: &: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '|':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: |: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: |: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)|de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b|right.val.b}, DNULL});
			fprintf(f, "error: |: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '^':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(!de_isi(left.type))
			{
				fprintf(f, "error: ^: first operand has invalid type %c\n", typename(left.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(!de_isi(right.type))
			{
				fprintf(f, "error: ^: second operand has invalid type %c\n", typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if((left.type==DEBUGTYPE_WORD)||(right.type==DEBUGTYPE_WORD))
			{
				return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=de_word(left)^de_word(right)}, DNULL});
			}
			if(left.type==DEBUGTYPE_BYTE)
				return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=left.val.b^right.val.b}, DNULL});
			fprintf(f, "error: ^: Unrecognised types %c,%c\n", typename(left.type), typename(right.type)); // should be impossible
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '=':
		{
			(*e)++;
			debugval left=de_recursive(f, e, ec, ev, ctx);
			debugval right=de_recursive(f, e, ec, ev, ctx);
			if((left.type==DEBUGTYPE_ERR)||(right.type==DEBUGTYPE_ERR))
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(!left.addr.type)
			{
				fprintf(f, "error: =: first operand is not an lvalue\n");
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			if(left.type!=right.type) // TODO try to cast right operand to left.type
			{
				fprintf(f, "error: =: type mismatch %c,%c\n", typename(left.type), typename(right.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			dwrite(left.addr, right, ctx);
			switch(left.type)
			{
				case DEBUGTYPE_BYTE:
					left.val.b=right.val.b;
					return(left);
				case DEBUGTYPE_WORD:
					left.val.w=right.val.w;
					return(left);
				case DEBUGTYPE_FLOAT:
					left.val.f=right.val.f;
					return(left);
				case DEBUGTYPE_GRID:
					memcpy(left.val.r, right.val.r, 8);
					return(left);
				case DEBUGTYPE_ROW:
					memcpy(left.val.r, right.val.r, 16);
					return(left);
				default:
					fprintf(f, "error: =: first operand has unrecognised type %c\n", typename(left.type));
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
		}
		break;
		case '\'':
		{
			char type=ev[*e][1];
			(*e)++;
			debugval val=de_recursive(f, e, ec, ev, ctx);
			if(val.type==DEBUGTYPE_ERR)
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			switch(type)
			{
				case 'b':
					if(de_isn(val.type))
						return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=floor(de_float(val))}, DNULL});
					else if((val.type==DEBUGTYPE_GRID)||(val.type==DEBUGTYPE_ROW))
						return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=val.val.r[0]}, DNULL});
				break;
				case 'w':
					if(de_isn(val.type))
						return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=floor(de_float(val))}, DNULL});
					else if((val.type==DEBUGTYPE_GRID)||(val.type==DEBUGTYPE_ROW))
						return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=val.val.r[0]|(val.val.r[1]<<8)}, DNULL});
				break;
				case 'f':
					if(de_isn(val.type))
						return((debugval){DEBUGTYPE_FLOAT, (debugval_val){.f=de_float(val)}, DNULL});
					else if((val.type==DEBUGTYPE_GRID)||(val.type==DEBUGTYPE_ROW))
					{
						fprintf(f, "error: ': cannot cast %c to f\n", typename(val.type));
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
					}
				break;
				case '8':
					if(val.type==DEBUGTYPE_GRID) return(val);
					else if(val.type==DEBUGTYPE_ROW)
					{
						debugval rv;
						rv.type=DEBUGTYPE_GRID;
						memcpy(rv.val.r, val.val.r, 8);
						rv.addr=DNULL;
						return(rv);
					}
					else if(de_isn(val.type))
					{
						fprintf(f, "error: ': cannot cast %c to 8\n", typename(val.type));
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
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
						rv.addr=DNULL;
						return(rv);
					}
					else if(de_isn(val.type))
					{
						fprintf(f, "error: ': cannot cast %c to R\n", typename(val.type));
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
					}
				break;
				default:
					fprintf(f, "error: ': unrecognised type %c\n", type);
					return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
				break;
			}
			fprintf(f, "error: ': unrecognised operand type %c\n", typename(val.type));
			return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
		}
		break;
		case '.':
		{
			char type=ev[*e][1];
			const char *far=ev[*e]+2;
			debugaddr daddr={.type=DEBUGADDR_MAIN, .page=0};
			if(*far==':')
			{
				far++;
				if(strcasecmp(far, "AY")==0)
				{
					if(!ay_enabled)
					{
						fprintf(f, "error: .: AY is not enabled\n");
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
					}
					daddr.type=DEBUGADDR_AY;
				}
				else if(strcasecmp(far, "ULAPLUS")==0)
				{
					if(!(ctx.ula&&ctx.ula->ulaplus_enabled))
					{
						fprintf(f, "error: .: ULAPLUS is not enabled\n");
						return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
					}
					daddr.type=DEBUGADDR_ULAPLUS;
				}
			}
			(*e)++;
			debugval addr=de_recursive(f, e, ec, ev, ctx);
			if(addr.type==DEBUGTYPE_ERR)
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			if(addr.type==DEBUGTYPE_BYTE)
			{
				addr.type=DEBUGTYPE_WORD;
				addr.val.w=addr.val.b; // extend it
			}
			if(addr.type==DEBUGTYPE_WORD)
			{
				daddr.addr=addr.val.w;
				debugtype dtype=DEBUGTYPE_ERR;
				switch(type)
				{
					case 'b':
						dtype=DEBUGTYPE_BYTE;
					break;
					case 'w':
						dtype=DEBUGTYPE_WORD;
					break;
					case 'f':
						dtype=DEBUGTYPE_FLOAT;
					break;
					case '8':
						dtype=DEBUGTYPE_GRID;
					break;
					case 'R':
						dtype=DEBUGTYPE_ROW;
					break;
				}
				return(dread(daddr, dtype, ctx));
			}
			else
			{
				fprintf(f, "error: .: Address is not of word (or byte) type, is %c\n", typename(addr.type));
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
		}
		break;
		case '@':
		{
			const char *s=ev[*e]+1;
			(*e)++;
			const struct sysvar *var=sysvarbyname(s);
			if(!var)
			{
				fprintf(f, "error: @: Unrecognised sysvar %s\n", s);
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
			return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=var->addr}, DNULL});
		}
		break;
		case '#':
		if(ctx.cpu)
		{
			z80 *cpu=ctx.cpu;
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
					return((debugval){DEBUGTYPE_WORD, (debugval_val){.w=cpu->regs[reg]|(cpu->regs[reg+1]<<8)}, (debugaddr){.type=DEBUGADDR_CPU, .addr=reg, .page=1}});
				else
					return((debugval){DEBUGTYPE_BYTE, (debugval_val){.b=cpu->regs[reg]}, (debugaddr){.type=DEBUGADDR_CPU, .addr=reg, .page=0}});
			}
			else
			{
				fprintf(f, "error: #: No such register %s\n", rn);
				return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
			}
		}
		break;
	}
	fprintf(f, "error: Unrecognised item %s\n", ev[*e]);
	(*e)++;
	return((debugval){DEBUGTYPE_ERR, (debugval_val){.b=0}, DNULL});
}

debugval debugger_expr(FILE *f, int ec, const char *const ev[256], debugctx ctx)
{
	int e=0;
	debugval rv=de_recursive(f, &e, ec, ev, ctx);
	if(e<ec) fprintf(f, "warning: %d input items were not consumed\n", ec-e);
	return(rv);
}

void show_state(debugctx ctx)
{
	int Tstates=ctx.Tstates;
	z80 *cpu=ctx.cpu;
	if(!cpu)
	{
		printf("\nMissing ctx.cpu\n");
		return;
	}
	bus_t *bus=ctx.bus;
	if(!bus)
	{
		printf("\nMissing ctx.bus\n");
		return;
	}
	ram_t *ram=ctx.ram;
	if(!ram)
	{
		printf("\nMissing ctx.ram\n");
		return;
	}
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
		uint16_t off = (*PC) + 8*(i-2);
		uint8_t RAM[8];
		off-=off%8;
		ram_read_bytes(ram, off, 8, RAM);
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (uint16_t)off, RAM[0], RAM[1], RAM[2], RAM[3], RAM[4], RAM[5], RAM[6], RAM[7]);
		off = (*HL) + 8*(i-2);
		off-=off%8;
		ram_read_bytes(ram, off, 8, RAM);
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (uint16_t)off, RAM[0], RAM[1], RAM[2], RAM[3], RAM[4], RAM[5], RAM[6], RAM[7]);
		off = (*SP) + 2*(i);
		ram_read_bytes(ram, off, 2, RAM);
		printf("%04x: %02x%02x\n", (uint16_t)off, RAM[1], RAM[0]);
	}
	printf("\t- near (BC), (DE) and (nn):\n");
	for(i=0;i<5;i++)
	{
		uint16_t off = (*BC) + 8*(i-2);
		uint8_t RAM[8];
		off-=off%8;
		ram_read_bytes(ram, off, 8, RAM);
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (uint16_t)off, RAM[0], RAM[1], RAM[2], RAM[3], RAM[4], RAM[5], RAM[6], RAM[7]);
		off = (*DE) + 8*(i-2);
		off-=off%8;
		ram_read_bytes(ram, off, 8, RAM);
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (uint16_t)off, RAM[0], RAM[1], RAM[2], RAM[3], RAM[4], RAM[5], RAM[6], RAM[7]);
		off = I16 + 4*(i-2);
		off-=off%4;
		ram_read_bytes(ram, off, 4, RAM);
		printf("%04x: %02x%02x %02x%02x\n", (uint16_t)off, RAM[0], RAM[1], RAM[2], RAM[3]);
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
			fprintf(f, "grid:\n");
			for(unsigned int i=0;i<8;i++)
			{
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
