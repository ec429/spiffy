#include "vchips.h"
#include <stdlib.h>
#include <math.h>
#include "bits.h"

int ram_init(ram_t *ram, FILE *rom, machine m)
{
	if(!ram) return(1);
	ram->plock=false;
	bool p128=cap_128_paging(m);
	ram->banks=p128?10:4;
	if(!(ram->write=malloc(ram->banks*sizeof(bool))))
	{
		perror("malloc");
		return(1);
	}
	if(!(ram->bank=malloc(ram->banks*sizeof(*ram->bank))))
	{
		perror("malloc");
		return(1);
	}
	for(unsigned int i=0;i<ram->banks;i++)
		if(p128)
			ram->write[i]=i>1;
		else
			ram->write[i]=i>0;
	if(rom)
	{
		if(fread(ram->bank[0], 1, 0x4000, rom)!=0x4000)
		{
			fprintf(stderr, "Failed to read in ROM file\n");
			return(1);
		}
		if(p128&&(fread(ram->bank[1], 1, 0x4000, rom)!=0x4000))
		{
			fprintf(stderr, "Failed to read in ROM file\n");
			return(1);
		}
	}
	if(p128)
	{
		ram->paged[0]=0; // ROM0
		ram->paged[1]=7; // RAM5
		ram->paged[2]=4; // RAM2
		ram->paged[3]=2; // RAM0
	}
	else
		for(unsigned int i=0;i<4;i++)
			ram->paged[i]=i;
	return(0);
}

uint8_t ram_read(const ram_t *ram, uint16_t addr)
{
	bus_t bus;
	bus.mreq=true;
	bus.tris=TRIS_IN;
	bus.addr=addr;
	do_ram(ram, &bus);
	return(bus.data);
}

void ram_write(ram_t *ram, uint16_t addr, uint8_t val)
{
	bus_t bus;
	bus.mreq=true;
	bus.tris=TRIS_OUT;
	bus.addr=addr;
	bus.data=val;
	do_ram(ram, &bus);
}

uint16_t ram_read_word(const ram_t *ram, uint16_t addr)
{
	uint8_t l=ram_read(ram, addr),
	        h=ram_read(ram, addr+1);
	return(l|(h<<8));
}

void ram_write_word(ram_t *ram, uint16_t addr, uint16_t val)
{
	uint8_t l=val&0xff, h=val>>8;
	ram_write(ram, addr, l);
	ram_write(ram, addr+1, h);
}

void ram_read_bytes(const ram_t *ram, uint16_t addr, uint16_t len, uint8_t *buf)
{
	for(uint16_t i=0;i<len;i++)
		buf[i]=ram_read(ram, addr+i);
}

void ram_write_bytes(ram_t *ram, uint16_t addr, uint16_t len, uint8_t *buf)
{
	for(uint16_t i=0;i<len;i++)
		ram_write(ram, addr+i, buf[i]);
}

void do_ram(const ram_t *ram, bus_t *bus)
{
	if(unlikely(!ram)) return;
	if(unlikely(!bus)) return;
	if(!bus->mreq) return;
	unsigned int page=bus->addr>>14, sel=ram->paged[0];
	if(page)
	{
		sel=ram->paged[page];
	}
	if(likely(sel<ram->banks))
	{
		if(bus->tris==TRIS_IN)
		{
			bus->data=ram->bank[sel][bus->addr&0x3fff];
		}
		else if(bus->tris==TRIS_OUT)
		{
			if(ram->write[sel])
			{
				ram->bank[sel][bus->addr&0x3fff]=bus->data;
			}
		}
	}
	else if(bus->tris==TRIS_IN)
	{
		bus->data=floor(rand()*256.0/RAND_MAX);
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

void mapk(uint8_t k, bool kstate[8][5], bool down)
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
