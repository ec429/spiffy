/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	sysvars.c - BASIC system variables
*/

#include "sysvars.h"
#include <string.h>

/* Note: this table is only valid for the 48k Spectrum; other machines may differ */
struct sysvar sysvartbl[]={
{23552, 8, "KSTATE"},
{23560, 1, "LAST_K"},
{23561, 1, "REPDEL"},
{23562, 1, "REPPER"},
{23563, 2, "DEFADD"},
{23565, 1, "K_DATA"},
{23566, 2, "TVDATA"},
{23568, 38, "STRMS"},
{23606, 2, "CHARS"},
{23608, 1, "RASP"},
{23609, 1, "PIP"},
{23610, 1, "ERR_NR"},
{23611, 1, "FLAGS"},
{23612, 1, "TV_FLAG"},
{23613, 2, "ERR_SP"},
{23615, 2, "LIST_SP"},
{23617, 1, "MODE"},
{23618, 2, "NEWPPC"},
{23620, 1, "NSPPC"},
{23621, 2, "PPC"},
{23623, 1, "SUBPPC"},
{23624, 1, "BORDCR"},
{23625, 2, "E_PPC"},
{23627, 2, "VARS"},
{23629, 2, "DEST"},
{23631, 2, "CHANS"},
{23633, 2, "CURCHL"},
{23635, 2, "PROG"},
{23637, 2, "NEXTLN"},
{23639, 2, "DATADD"},
{23641, 2, "E_LINE"},
{23643, 2, "K_CUR"},
{23645, 2, "CH_ADD"},
{23647, 2, "X_PTR"},
{23649, 2, "WORKSP"},
{23651, 2, "STKBOT"},
{23653, 2, "STKEND"},
{23655, 1, "BREG"},
{23656, 2, "MEM"},
{23658, 1, "FLAGS2"},
{23659, 1, "DF_SZ"},
{23660, 2, "S_TOP"},
{23662, 2, "OLDPPC"},
{23664, 1, "OSPPC"},
{23665, 1, "FLAGX"},
{23666, 2, "STRLEN"},
{23668, 2, "T_ADDR"},
{23670, 2, "SEED"},
{23672, 3, "FRAMES"},
{23675, 2, "UDG"},
{23677, 2, "COORDS"},
{23679, 1, "P_POSN"},
{23680, 2, "PR_CC"},
{23682, 2, "ECHO_E"},
{23684, 2, "DF_CC"},
{23686, 2, "DFCCL"},
{23688, 2, "S_POSN"},
{23690, 2, "SPOSNL"},
{23692, 1, "SCR_CT"},
{23693, 1, "ATTR_P"},
{23694, 1, "MASK_P"},
{23695, 1, "ATTR_T"},
{23696, 1, "MASK_T"},
{23697, 1, "P_FLAG"},
{23698, 30, "MEMBOT"},
{23728, 2, "NMIADD"},
{23730, 2, "RAMTOP"},
{23732, 2, "P_RAMT"},
{0, 0, NULL}
};

const struct sysvar *sysvarbyname(const char *name)
{
	unsigned int i=0;
	while(sysvartbl[i].name)
	{
		if(strcmp(sysvartbl[i].name, name)==0) return(sysvartbl+i);
		i++;
	}
	return(NULL);
}
