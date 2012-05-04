/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	sysvars.c - BASIC system variables
*/

#include "sysvars.h"
#include <string.h>

/* Note: this table is only valid for the 48k Spectrum; other machines may differ */
static struct sysvar sysvartbl[]={
{23552, 8, "KSTATE", SVT_BYTES},
{23560, 1, "LAST_K", SVT_CHAR},
{23561, 1, "REPDEL", SVT_U8},
{23562, 1, "REPPER", SVT_U8},
{23563, 2, "DEFADD", SVT_ADDR},
{23565, 1, "K_DATA", SVT_CHAR},
{23566, 2, "TVDATA", SVT_CHAR},
{23568, 38, "STRMS", SVT_BYTES},
{23606, 2, "CHARS", SVT_ADDR},
{23608, 1, "RASP", SVT_U8},
{23609, 1, "PIP", SVT_U8},
{23610, 1, "ERR_NR", SVT_U8},
{23611, 1, "FLAGS", SVT_FLAGS},
{23612, 1, "TV_FLAG", SVT_FLAGS},
{23613, 2, "ERR_SP", SVT_ADDR},
{23615, 2, "LIST_SP", SVT_ADDR},
{23617, 1, "MODE", SVT_U8},
{23618, 2, "NEWPPC", SVT_U16},
{23620, 1, "NSPPC", SVT_U8},
{23621, 2, "PPC", SVT_U16},
{23623, 1, "SUBPPC", SVT_U8},
{23624, 1, "BORDCR", SVT_FLAGS},
{23625, 2, "E_PPC", SVT_U16},
{23627, 2, "VARS", SVT_ADDR},
{23629, 2, "DEST", SVT_ADDR},
{23631, 2, "CHANS", SVT_ADDR},
{23633, 2, "CURCHL", SVT_ADDR},
{23635, 2, "PROG", SVT_ADDR},
{23637, 2, "NEXTLN", SVT_ADDR},
{23639, 2, "DATADD", SVT_ADDR},
{23641, 2, "E_LINE", SVT_ADDR},
{23643, 2, "K_CUR", SVT_ADDR},
{23645, 2, "CH_ADD", SVT_ADDR},
{23647, 2, "X_PTR", SVT_ADDR},
{23649, 2, "WORKSP", SVT_ADDR},
{23651, 2, "STKBOT", SVT_ADDR},
{23653, 2, "STKEND", SVT_ADDR},
{23655, 1, "BREG", SVT_U8},
{23656, 2, "MEM", SVT_ADDR},
{23658, 1, "FLAGS2", SVT_FLAGS},
{23659, 1, "DF_SZ", SVT_U8},
{23660, 2, "S_TOP", SVT_U16},
{23662, 2, "OLDPPC", SVT_U16},
{23664, 1, "OSPPC", SVT_U8},
{23665, 1, "FLAGX", SVT_FLAGS},
{23666, 2, "STRLEN", SVT_U16},
{23668, 2, "T_ADDR", SVT_ADDR},
{23670, 2, "SEED", SVT_U16},
{23672, 3, "FRAMES", SVT_U24},
{23675, 2, "UDG", SVT_ADDR},
{23677, 2, "COORDS", SVT_XY},
{23679, 1, "P_POSN", SVT_U8},
{23680, 2, "PR_CC", SVT_ADDR},
{23682, 2, "ECHO_E", SVT_XY},
{23684, 2, "DF_CC", SVT_ADDR},
{23686, 2, "DFCCL", SVT_ADDR},
{23688, 2, "S_POSN", SVT_XY},
{23690, 2, "SPOSNL", SVT_XY},
{23692, 1, "SCR_CT", SVT_U8},
{23693, 1, "ATTR_P", SVT_FLAGS},
{23694, 1, "MASK_P", SVT_FLAGS},
{23695, 1, "ATTR_T", SVT_FLAGS},
{23696, 1, "MASK_T", SVT_FLAGS},
{23697, 1, "P_FLAG", SVT_FLAGS},
{23698, 30, "MEMBOT", SVT_BYTES},
{23728, 2, "NMIADD", SVT_ADDR},
{23730, 2, "RAMTOP", SVT_ADDR},
{23732, 2, "P_RAMT", SVT_ADDR},
{0, 0, NULL, SVT_BYTES}
};

const struct sysvar *sysvarbyname(const char *name)
{
	unsigned int i=0;
	while(sysvartbl[i].name)
	{
		if(strcasecmp(sysvartbl[i].name, name)==0) return(sysvartbl+i);
		i++;
	}
	return(NULL);
}

const struct sysvar *sysvars()
{
	return(sysvartbl);
}
