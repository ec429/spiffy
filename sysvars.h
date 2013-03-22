/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	sysvars.h - BASIC system variables
*/

enum sysvartype
{
	SVT_CHAR,
	SVT_FLAGS,
	SVT_ADDR,
	SVT_BYTES,
	SVT_U8,
	SVT_U16,
	SVT_U24,
	SVT_XY
};

const struct sysvar {unsigned short int addr; unsigned short int len; const char *name; enum sysvartype type;} *sysvarbyname(const char *name);

const struct sysvar *sysvars();
