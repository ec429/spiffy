/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	filters.c - graphics filters
*/

#include "filters.h"
#include "bits.h"

const char *filter_name(unsigned int filt_id)
{
	if(filt_id==FILT_BW) return("Black & White");
	else if(filt_id==FILT_SCAN) return("TV Scanlines");
	else if(filt_id==FILT_BLUR) return("Horizontal Blur");
	else if(filt_id==FILT_VBLUR) return("Vertical Blur");
	else if(filt_id==FILT_MISG) return("Misaligned Green");
	else if(filt_id==FILT_SLOW) return("Slow Fade");
	else if(filt_id==FILT_PAL) return("PAL Chroma Distortion");
	else if(!filt_id) return("Unfiltered");
	else return("Error");
}

void filter_pix(unsigned int filt_mask, unsigned int x, unsigned int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if(!filt_mask) return;
	static uint8_t lastr, lastg, lastb, misg;
	static uint8_t rowr[320], rowg[320], rowb[320];
	static uint8_t old[320][296][3];
	static uint8_t lumx;
	static signed int oldcr[320], oldcb[320];
	static bool field;
	
	if(filt_mask&FILT_BW)
	{
		*r=*g=*b=((*r*2)+(*g*3)+*b)/6;
	}
	else if(filt_mask&FILT_PAL) // PAL doesn't make sense for a BW set
	{
		if(!(x||y)) field=!field;
		uint8_t luma=((*r*2)+(*g*3)+*b)/6;
		signed int cb=*b-luma, cr=*r-luma;
		uint8_t sc=(x&4)>>1, cc=((x-1)&4)>>1;
		signed int palcr=((sc-1)*(luma-127)<<2);
		signed int palcb=((cc-1)*(luma-127)<<2);
		signed int palxr=((sc-1)*(lumx-127)<<2);
		signed int palxb=((cc-1)*(lumx-127)<<2);
		signed int newcr=cr-(((y&1)?field:!field)?palxr-palcr:palcr-palxr), newcb=cb+(field?palxb-palcb:palcb-palxb);
		*r=min(max(luma+((newcr+oldcr[x])>>1), 0), 255);
		*b=min(max(luma+((newcb+oldcb[x])>>1), 0), 255);
		*g=min(max((luma*6-*r*2-*b)/3, 0), 255);
		oldcr[x]=newcr;oldcb[x]=newcb;
		lumx=luma;
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
	
	if(filt_mask&FILT_BLUR)
	{
		if(x)
		{
			*r=(*r/3)+(lastr*2/3);
			*g=(*g/3)+(lastg*2/3);
			*b=(*b/3)+(lastb*2/3);
		}
		lastr=*r;
		lastg=*g;
		lastb=*b;
	}
	
	if(filt_mask&FILT_VBLUR)
	{
		if(y)
		{
			*r=(*r>>1)+(rowr[x]>>1);
			*g=(*g>>1)+(rowg[x]>>1);
			*b=(*b>>1)+(rowb[x]>>1);
		}
		rowr[x]=*r;
		rowg[x]=*g;
		rowb[x]=*b;
	}
	
	if((filt_mask&FILT_MISG)&&!(filt_mask&FILT_BW)) // MISG doesn't make sense for a BW set
	{
		uint8_t tmp=*g;
		if(x)
			*g=misg;
		misg=tmp;
	}
	
	if(filt_mask&FILT_SLOW)
	{
		*r=old[x][y][0]=max(*r, (old[x][y][0]>>1)+(old[x][y][0]>>2));
		*g=old[x][y][1]=max(*g, (old[x][y][1]>>1)+(old[x][y][1]>>2));
		*b=old[x][y][2]=max(*b, (old[x][y][2]>>1)+(old[x][y][2]>>2));
	}
}
