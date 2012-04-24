/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	audio.c - audio functions
*/

#include "audio.h"
#include <unistd.h>
#include <math.h>

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
			while(!a->play&&(a->rp==a->wp)) usleep(5e3);
			a->cbuf[a->rp]=a->bits[a->rp];
			a->rp=(a->rp+1)%SINCBUFLEN;
		}
		unsigned int l=a->play?SINCBUFLEN>>2:SINCBUFLEN;
		double v=0;
		for(unsigned int j=0;j<l;j++)
		{
			signed char d=a->cbuf[(a->rp+SINCBUFLEN-j)%SINCBUFLEN]-a->cbuf[(a->rp+SINCBUFLEN-j-1)%SINCBUFLEN];
			if(d)
				v+=d*sincgroups[*sinc_rate-(j%*sinc_rate)-1][j/(*sinc_rate)];
		}
		if(a->play) v*=0.2;
		Uint16 samp=floor(v*1024.0);
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
