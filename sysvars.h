/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	sysvars.h - BASIC system variables
*/

const struct sysvar {unsigned short int addr; unsigned short int len; const char *name;} *sysvarbyname(const char *name);
