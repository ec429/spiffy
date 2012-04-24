#include "basic.h"
#include <stdio.h>

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
