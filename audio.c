/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	audio.c - audio functions
*/

#include "audio.h"
#include <unistd.h>
#include <math.h>
#ifdef WINDOWS
#include <windef.h>
#include <winbase.h>
#endif

unsigned char internal_sinc_rate=12, *sinc_rate=&internal_sinc_rate;

unsigned char *get_sinc_rate(void)
{
	return(sinc_rate);
}

void mixaudio(void *abuf, Uint8 *stream, int len)
{
	audiobuf *a=abuf;
	for(int i=0;i<len;i+=2)
	{
		for(unsigned int g=0;g<*sinc_rate;g++)
		{
			unsigned int waits=0;
			while(!a->play&&(a->rp==a->wp))
			{
				usleep(5e3);
				if(waits++>40) break;
			}
			a->cbuf[a->rp]=a->bits[a->rp];
			a->rp=(a->rp+1)%SINCBUFLEN;
		}
		unsigned int l=a->play?SINCBUFLEN>>2:SINCBUFLEN;
		double v=0;
		for(unsigned int j=0;j<l;j++)
		{
			signed int d=a->cbuf[(a->rp+SINCBUFLEN-j)%SINCBUFLEN]-a->cbuf[(a->rp+SINCBUFLEN-j-1)%SINCBUFLEN];
			if(d)
				v+=d*sincgroups[*sinc_rate-(j%*sinc_rate)-1][j/(*sinc_rate)];
		}
		if(a->play) v*=0.6;
		Uint16 samp=floor(v*4.0);
		stream[i]=samp;
		stream[i+1]=samp>>8;
		if(a->record)
		{
			fputc(stream[i], a->record);
			fputc(stream[i+1], a->record);
		}
	}
}

void update_sinc(unsigned char filterfactor)
{
	double sinc[SINCBUFLEN];
	for(unsigned int i=0;i<(unsigned int)SINCBUFLEN;i++)
	{
		double v=filterfactor*(i/(double)SINCBUFLEN-0.5);
		sinc[i]=(v?sin(v)/v:1)*16.0/(double)*sinc_rate;
	}
	for(unsigned int g=0;g<*sinc_rate;g++)
	{
		for(unsigned int j=0;j<AUDIOBUFLEN;j++)
			sincgroups[g][j]=0;
		for(unsigned int i=0;i<(unsigned int)SINCBUFLEN;i++)
		{
			unsigned int j=(i+g)/(*sinc_rate);
			if(j<AUDIOBUFLEN)
				sincgroups[g][j]+=sinc[i];
		}
	}
}

void wavheader(FILE *a)
{
	fwrite("RIFF", 1, 4, a);
	fwrite("\377\377\377\377", 1, 4, a);
	fwrite("WAVEfmt ", 1, 8, a);
	fputc(16, a);
	fputc(0, a);
	fputc(0, a);
	fputc(0, a);
	fputc(1, a);
	fputc(0, a);
	fputc(1, a);
	fputc(0, a);
	fputc(SAMPLE_RATE, a);
	fputc(SAMPLE_RATE>>8, a);
	fputc(SAMPLE_RATE>>16, a);
	fputc(SAMPLE_RATE>>24, a);
	fputc(SAMPLE_RATE<<1, a);
	fputc(SAMPLE_RATE>>7, a);
	fputc(SAMPLE_RATE>>15, a);
	fputc(SAMPLE_RATE>>23, a);
	fputc(2, a);
	fputc(0, a);
	fputc(16, a);
	fputc(0, a);
	fwrite("data", 1, 4, a);
	fwrite("\377\377\377\377", 1, 4, a);
}

void ay_init(ay_t *ay)
{
	for(unsigned int r=0;r<16;r++)
		ay->reg[r]=0;
	ay->reg[7]=0xff;
	ay->regsel=0;
	ay->bit[0]=ay->bit[1]=ay->bit[2]=false;
	ay->count[0]=ay->count[1]=ay->count[2]=0;
	ay->envcount=0;
	ay->env=0;
	ay->envstop=ay->envrev=false;
	ay->out[0]=ay->out[1]=ay->out[2]=0;
	ay->noise=ay->noisecount=0;
}

unsigned char ay_vol_tbl[16]={0, 2, 5, 7, 10, 14, 19, 29, 40, 56, 80, 103, 131, 161, 197, 236};

void ay_tstep(ay_t *ay, unsigned int steps)
{
	if(!steps)
	{
		unsigned int ec=ay->reg[11]+(ay->reg[12]<<8);
		if(!ec) ec=0x1000;
		if(++ay->envcount>=ec)
		{
			ay->envcount=0;
			if(!ay->envstop)
			{
				bool inc=ay->envrev;
				if(ay->reg[13]&0x04) inc=!inc;
				if(inc)
				{
					if(++ay->env>15)
					{
						if(ay->reg[13]&0x08)
						{
							if(ay->reg[13]&0x04)
							{
								ay->envrev=false;
								ay->env=14;
							}
							else if(ay->reg[13]&0x01)
							{
								ay->env=(ay->reg[13]&0x02)?0:15;
								ay->envstop=true;
							}
							else if(ay->reg[13]&0x02)
							{
								ay->envrev=true;
								ay->env=14;
							}
							else
							{
								ay->env=0;
							}
						}
						else
						{
							ay->env=0;
							ay->envstop=true;
						}
					}
				}
				else
				{
					if(!ay->env--)
					{
						if(ay->reg[13]&0x08)
						{
							if(ay->reg[13]&0x04)
							{
								ay->envrev=false;
								ay->env=1;
							}
							else if(ay->reg[13]&0x01)
							{
								ay->env=(ay->reg[13]&0x02)?15:0;
								ay->envstop=true;
							}
							else if(ay->reg[13]&0x02)
							{
								ay->envrev=true;
								ay->env=1;
							}
							else
							{
								ay->env=15;
							}
						}
						else
						{
							ay->env=0;
							ay->envstop=true;
						}
					}
				}
			}
		}
	}
	
	unsigned int nc=ay->reg[6]&0x1f;
	if(!nc) nc=0x20;
	if(++ay->noisecount>=nc)
	{
		ay->noisecount=0;
		// Arbitrary setup value
		if(!ay->noise) ay->noise=0xAAAA;
		// Based on some random webpage, may not be accurate
		ay->noise = (ay->noise >> 1) ^ ((ay->noise & 1) ? 0x14000 : 0);
	}
	
	for(unsigned int i=0;i<3;i++)
	{
		unsigned int rc=ay->reg[i<<1]+((ay->reg[(i<<1)+1]&0xf)<<8);
		if(!rc) rc=0x1000;
		if(++ay->count[i]>=rc)
		{
			ay->bit[i]=!ay->bit[i];
			ay->count[i]=0;
		}
		unsigned char vol=ay_vol_tbl[(ay->reg[8+i]&0x10)?ay->env&0x0f:(ay->reg[8+i]&0xf)];
		if(!(ay->reg[7]&(1<<i)))
			ay->out[i]=ay->bit[i]?(vol>>1)+(vol>>2):0;
		if((ay->noise&1)&&!(ay->reg[7]&(8<<i)))
			ay->out[i]+=(vol>>2);
	}
}
