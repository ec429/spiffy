#include "vchips.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "bits.h"

void do_ram(unsigned char RAM[65536], bus_t *bus, bool wrom)
{
	if(bus->mreq&&(bus->tris==TRIS_IN))
	{
		if(likely(bus->addr<ramtop))
		{
			bus->data=RAM[bus->addr];
		}
		else
		{
			bus->data=floor(rand()*256.0/RAND_MAX);
		}
	}
	else if(bus->mreq&&(bus->tris==TRIS_OUT))
	{
		if((wrom||(bus->addr&0xC000))&&likely(bus->addr<ramtop))
			RAM[bus->addr]=bus->data;
	}
}

int init_keyboard(void)
{
	nkmaps=0;
	kmap=NULL;
	FILE *f=configopen("keymap", "r");
	if(!f) return(-1);
	while(!feof(f))
	{
		int c=fgetc(f);
		if(c==EOF) break;
		keymap new={.key=c, .twokey=false};
		c=fgetc(f);
		if(c==EOF) break;
		if(c!='=')
		{
			while((c=fgetc(f))!='\n')
				if(c==EOF) break;
			if(c==EOF) break;
			continue;
		}
		int r=fgetc(f);
		if(r==EOF) break;
		c=fgetc(f);
		if(c==EOF) break;
		new.row[0]=r-'0'; // XXX no error checking!
		new.col[0]=c-'0';
		r=fgetc(f);
		if(!((r=='\n')||(r==EOF)))
		{
			c=fgetc(f);
			if(c==EOF) break;
			new.row[1]=r-'0'; // XXX no error checking!
			new.col[1]=c-'0';
			new.twokey=true;
			while((r=fgetc(f))!='\n');
				if(r==EOF) break;
		}
		unsigned int n=nkmaps++;
		keymap *nk=realloc(kmap, nkmaps*sizeof(keymap));
		if(!nk)
		{
			free(kmap);
			fclose(f);
			return(-1);
		}
		(kmap=nk)[n]=new;
		if(r==EOF) break;
	}
	fclose(f);
	return(0);
}

void mapk(unsigned char k, bool kstate[8][5], bool down)
{
	for(unsigned int i=0;i<nkmaps;i++)
	{
		if(kmap[i].key==k)
		{
			kstate[kmap[i].row[0]][kmap[i].col[0]]=down;
			if(kmap[i].twokey)
				kstate[kmap[i].row[1]][kmap[i].col[1]]=down;
		}
	}
}
