/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	basic.c - BASIC debugging functions
*/

#include "basic.h"
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

const char *toktbl[]={
"RND ", // 0xA5
"INKEY$ ",
"PI ",
"FN ",
"POINT ",
"SCREEN$ ",
"ATTR ",
"AT ",
"TAB ",
"VAL$ ",
"CODE ",
"VAL ",
"LEN ",
"SIN ",
"COS ",
"TAN ",
"ASN ",
"ACS ",
"ATN ",
"LN ",
"EXP ",
"INT ",
"SQR ",
"SGN ",
"ABS ",
"PEEK ",
"IN ",
"USR ",
"STR$ ",
"CHR$ ",
"NOT ",
"BIN ",
"OR ",
"AND ",
"<=",
">=",
"<>",
"LINE ",
"THEN ",
"TO ",
"STEP ",
"DEF FN ",
"CAT ",
"FORMAT ",
"MOVE ",
"ERASE ",
"OPEN #",
"CLOSE #",
"MERGE ",
"VERIFY ",
"BEEP ",
"CIRCLE ",
"INK ",
"PAPER ",
"FLASH ",
"BRIGHT ",
"INVERSE ",
"OVER ",
"OUT ",
"LPRINT ",
"LLIST ",
"STOP ",
"READ ",
"DATA ",
"RESTORE ",
"NEW ",
"BORDER ",
"CONTINUE ",
"DIM ",
"REM ",
"FOR ",
"GO TO ",
"GO SUB ",
"INPUT ",
"LOAD ",
"LIST ",
"LET ",
"PAUSE ",
"NEXT ",
"POKE ",
"PRINT ",
"PLOT ",
"RUN ",
"SAVE ",
"RANDOMIZE ",
"IF ",
"CLS ",
"DRAW ",
"CLEAR ",
"RETURN ",
"COPY ",
NULL
};

const char *baschar(unsigned char c)
{
	static char buf[5];
	if(c>=0xA5) return(toktbl[c-0xA5]);
	if(c>=0x90)
	{
		snprintf(buf, 5, "[%c]", c-0x4F);
		return(buf);
	}
	switch(c)
	{
		case 0x60:
			return("£");
		case 0x7F:
			return("©");
		case 0x10:
			return("[INK]");
		case 0x11:
			return("[PAPER]");
		case 0x12:
			return("[FLASH]");
		case 0x13:
			return("[BRIGHT]");
		case 0x14:
			return("[INVERSE]");
		case 0x15:
			return("[OVER]");
		case 0x16:
			return("[AT]");
		case 0x17:
			return("[TAB]");
	}
	if((c>=0x80)||(c<0x20))
	{
		snprintf(buf, 5, "\\%03o", c);
		return(buf);
	}
	buf[0]=c;
	buf[1]=0;
	return(buf);
}

double float_decode(const unsigned char *RAM, unsigned int addr)
{
	if(!RAM[addr])
	{
		if((!RAM[addr+1])||(RAM[addr+1]==0xFF))
		{
			if(!RAM[addr+4])
			{
				unsigned short int val=peek16(addr+2);
				if(RAM[addr+1]) return(val-131072);
				return(val);
			}
		}
	}
	signed char exponent=RAM[addr]-128;
	unsigned long mantissa=((RAM[addr+1]|0x80)<<24)|(RAM[addr+2]<<16)|(RAM[addr+3]<<8)|RAM[addr+4];
	bool minus=RAM[addr+1]&0x80;
	return((minus?-1.0:1.0)*mantissa*exp2(exponent-32));
}

void float_encode(unsigned char *RAM, unsigned int addr, double val)
{
	if((fabs(val)<65536)&&(ceil(val)==val))
	{
		RAM[addr]=RAM[addr+4]=0;
		signed int ival=ceil(val);
		if(signbit(val))
		{
			RAM[addr+1]=0xFF;
			ival+=131072;
		}
		else
			RAM[addr+1]=0;
		poke16(addr+2, (unsigned int)ival);
	}
	else if(isfinite(val))
	{
		signed char exponent=1+floor(log2(fabs(val)));
		double mantissa=rint(fabs(val)*exp2(32-exponent));
		if(mantissa>0xffffffff)
		{
			fprintf(stderr, "float_encode: 6 Number too big, %g\n", val);
			return;
		}
		unsigned long mi=mantissa;
		RAM[addr]=exponent+128;
		RAM[addr+1]=((mi>>24)&0x7F)|(signbit(val)?0x80:0);
		RAM[addr+2]=mi>>16;
		RAM[addr+3]=mi>>8;
		RAM[addr+4]=mi;
	}
	else
	{
		fprintf(stderr, "float_encode: cannot encode non-finite number %g\n", val);
	}
}

int compare_bas_line(const void *a, const void *b)
{
	unsigned short int na=((bas_line *)a)->number, nb=((bas_line *)b)->number;
	if(na<nb) return(-1);
	if(na>nb) return(1);
	return(0);
}
