/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	filters.c - graphics filters
*/

#include "filters.h"
#include "bits.h"

const char *filter_name(unsigned int filt_id)
{
	if(filt_id==FILT_BW) return("Black & White");
	else if(filt_id==FILT_SCAN) return("TV Scanlines");
	else if(!filt_id) return("Unfiltered");
	else return("Error");
}

void filter_pix(unsigned int filt_mask, unsigned int __attribute__((unused)) x, unsigned int y, unsigned char pix, bool bright, unsigned char *r, unsigned char *g, unsigned char *b)
{
	unsigned char t=bright?240:200;
	*r=(pix&2)?t:0;
	*g=(pix&4)?t:0;
	*b=(pix&1)?t:0;
	if(pix==1) *b+=15;
	
	if(filt_mask&FILT_BW)
	{
		*r=*g=*b=(pix*24)+((pix&&bright)?80:0);
	}
	
	if(filt_mask&FILT_SCAN)
	{
		if(y&1)
		{
			*r=min(*r, 240)+15;
			*g=min(*g, 240)+15;
			*b=min(*b, 240)+15;
		}
		else
		{
			*r=max(*r, 15)-15;
			*g=max(*g, 15)-15;
			*b=max(*b, 15)-15;
		}
	}
}
