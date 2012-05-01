/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	ui.c - User Interface
*/

#include "ui.h"
#include "bits.h"
#include "pbm.h"

SDL_Surface * gf_init(unsigned int x, unsigned int y)
{
	SDL_Surface * screen;
	if(SDL_Init(SDL_INIT_VIDEO)<0)
	{
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return(NULL);
	}
	atexit(SDL_Quit);
	if((screen = SDL_SetVideoMode(x, y, 32, SDL_HWSURFACE))==0)
	{
		fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
		return(NULL);
	}
	return(screen);
}

void ui_init(SDL_Surface *screen, button **buttons, bool edgeload, bool pause, bool printer)
{
	static button btn[nbuttons];
	*buttons=btn;
	SDL_WM_SetCaption("Spiffy - ZX Spectrum 48k", "Spiffy");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_FillRect(screen, &(SDL_Rect){0, 296, screen->w, 1}, SDL_MapRGB(screen->format, 255, 255, 255));
	SDL_FillRect(screen, &(SDL_Rect){0, 297, screen->w, 63}, SDL_MapRGB(screen->format, 0, 0, 0)); // controls area
	if(printer)
	{
		SDL_FillRect(screen, &(SDL_Rect){0, 359, screen->w, 1}, SDL_MapRGB(screen->format, 255, 255, 255));
		SDL_FillRect(screen, &(SDL_Rect){0, 360, screen->w, 120}, SDL_MapRGB(screen->format, 31, 31, 31)); // printer area
		SDL_FillRect(screen, &(SDL_Rect){32, 460, 256, 20}, SDL_MapRGB(screen->format, 191, 191, 195)); // printer paper
	}
	FILE *fimg;
	string img;
	fimg=configopen("buttons/load.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[0]=(button){.img=pbm_string(img), .posn={116, 298, 17, 17}, .col=0x8f8f1f};
	free_string(&img);
	fimg=configopen("buttons/flash.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[1]=(button){.img=pbm_string(img), .posn={136, 298, 17, 17}, .col=edgeload?0xffffff:0x1f1f1f};
	free_string(&img);
	fimg=configopen("buttons/play.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[2]=(button){.img=pbm_string(img), .posn={156, 298, 17, 17}, .col=0x3fbf5f};
	free_string(&img);
	fimg=configopen("buttons/next.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[3]=(button){.img=pbm_string(img), .posn={176, 298, 17, 17}, .col=0x07079f};
	free_string(&img);
	fimg=configopen("buttons/stop.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[4]=(button){.img=pbm_string(img), .posn={196, 298, 17, 17}, .col=0x3f0707};
	free_string(&img);
	fimg=configopen("buttons/rewind.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[5]=(button){.img=pbm_string(img), .posn={216, 298, 17, 17}, .col=0x7f076f};
	free_string(&img);
	fimg=configopen("buttons/pause.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[6]=(button){.img=pbm_string(img), .posn={8, 340, 17, 17}, .col=pause?0xbf6f07:0x7f6f07};
	free_string(&img);
	fimg=configopen("buttons/reset.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[7]=(button){.img=pbm_string(img), .posn={28, 340, 17, 17}, .col=0xffaf07};
	free_string(&img);
	fimg=configopen("buttons/bug.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[8]=(button){.img=pbm_string(img), .posn={48, 340, 17, 17}, .col=0xbfff3f};
	free_string(&img);
#ifdef AUDIO
	btn[9].posn=(SDL_Rect){76, 321, 7, 6};
	btn[10].posn=(SDL_Rect){76, 328, 7, 6};
	btn[11].posn=(SDL_Rect){140, 321, 7, 6};
	btn[12].posn=(SDL_Rect){140, 328, 7, 6};
	fimg=configopen("buttons/record.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[13]=(button){.img=pbm_string(img), .posn={8, 320, 17, 17}, .col=0x7f0707};
	drawbutton(screen, btn[13]);
	free_string(&img);
#endif /* AUDIO */
	fimg=configopen("buttons/snap.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[14]=(button){.img=pbm_string(img), .posn={68, 340, 17, 17}, .col=0xbfbfbf};
	drawbutton(screen, btn[14]);
	free_string(&img);
	fimg=configopen("buttons/trec.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[15]=(button){.img=pbm_string(img), .posn={236, 298, 17, 17}, .col=0x4f0f0f};
	drawbutton(screen, btn[15]);
	free_string(&img);
	fimg=configopen("buttons/feed.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[16]=(button){.img=pbm_string(img), .posn={88, 340, 17, 17}, .col=printer?0x3f276f:0x170f1f};
	drawbutton(screen, btn[16]);
	free_string(&img);
	fimg=configopen("buttons/js_c.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[17]=(button){.img=pbm_string(img), .posn={108, 340, 9, 9}, .col=0x3fff3f};
	free_string(&img);
	fimg=configopen("buttons/js_s.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[18]=(button){.img=pbm_string(img), .posn={116, 340, 9, 9}, .col=0x3f3f3f};
	free_string(&img);
	fimg=configopen("buttons/js_k.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[19]=(button){.img=pbm_string(img), .posn={108, 348, 9, 9}, .col=0x3f3f3f};
	free_string(&img);
	fimg=configopen("buttons/js_x.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[20]=(button){.img=pbm_string(img), .posn={116, 348, 9, 9}, .col=0x3f3f3f};
	free_string(&img);
	ksupdate(screen, *buttons, JS_C);
	fimg=configopen("buttons/bw.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[21]=(button){.img=pbm_string(img), .posn={128, 340, 17, 17}, .col=0x9f9f9f};
	drawbutton(screen, btn[21]);
	free_string(&img);
	fimg=configopen("buttons/scan.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[22]=(button){.img=pbm_string(img), .posn={148, 340, 17, 17}, .col=0x6f6fdf};
	drawbutton(screen, btn[22]);
	free_string(&img);
	fimg=configopen("buttons/blur.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[23]=(button){.img=pbm_string(img), .posn={168, 340, 17, 17}, .col=0xbf3f3f};
	drawbutton(screen, btn[23]);
	free_string(&img);
	fimg=configopen("buttons/vblur.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[24]=(button){.img=pbm_string(img), .posn={188, 340, 17, 17}, .col=0xbf3f3f};
	drawbutton(screen, btn[24]);
	free_string(&img);
	fimg=configopen("buttons/misg.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[25]=(button){.img=pbm_string(img), .posn={208, 340, 17, 17}, .col=0x1f5f1f};
	drawbutton(screen, btn[25]);
	free_string(&img);
	for(unsigned int i=0;i<9;i++)
		drawbutton(screen, btn[i]);
}

inline void pset(SDL_Surface * screen, int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	long int s_off = (y*screen->pitch) + x*screen->format->BytesPerPixel;
	unsigned long int pixval = SDL_MapRGB(screen->format, r, g, b),
		* pixloc = (unsigned long int *)(((unsigned char *)screen->pixels)+s_off);
	*pixloc = pixval;
}

int line(SDL_Surface * screen, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b)
{
	if(x2<x1)
	{
		int _t=x1;
		x1=x2;
		x2=_t;
		_t=y1;
		y1=y2;
		y2=_t;
	}
	int dy=y2-y1,
		dx=x2-x1;
	if(dx==0)
	{
		int cy;
		for(cy=y1;(dy>0)?cy-y2:y2-cy<=0;cy+=(dy>0)?1:-1)
		{
			pset(screen, x1, cy, r, g, b);
		}
	}
	else if(dy==0)
	{
		int cx;
		for(cx=x1;cx<=x2;cx++)
		{
			pset(screen, cx, y1, r, g, b);
		}
	}
	else
	{
		double m = (double)dy/(double)dx;
		int cx=x1, cy=y1;
		if(m>0)
		{
			while(cx<x2)
			{
				do {
					pset(screen, cx, cy, r, g, b);
					cx++;
				} while((((cx-x1) * m)<(cy-y1)) && cx<x2);
				do {
					pset(screen, cx, cy, r, g, b);
					cy++;
				} while((((cx-x1) * m)>(cy-y1)) && cy<y2);
			}
		}
		else
		{
			while(cx<x2)
			{
				do {
					pset(screen, cx, cy, r, g, b);
					cx++;
				} while((((cx-x1) * m)>(cy-y1)) && cx<x2);
				do {
					pset(screen, cx, cy, r, g, b);
					cy--;
				} while((((cx-x1) * m)<(cy-y1)) && cy>y2);
			}
		}
	}
	return(0);
}

void uparrow(SDL_Surface * screen, SDL_Rect where, unsigned long col, unsigned long bcol)
{
	SDL_FillRect(screen, &where, SDL_MapRGB(screen->format, bcol>>16, bcol>>8, bcol));
	pset(screen, where.x+1, where.y+3, col>>16, col>>8, col);
	pset(screen, where.x+2, where.y+2, col>>16, col>>8, col);
	pset(screen, where.x+3, where.y+1, col>>16, col>>8, col);
	pset(screen, where.x+4, where.y+2, col>>16, col>>8, col);
	pset(screen, where.x+5, where.y+3, col>>16, col>>8, col);
}

void downarrow(SDL_Surface * screen, SDL_Rect where, unsigned long col, unsigned long bcol)
{
	SDL_FillRect(screen, &where, SDL_MapRGB(screen->format, bcol>>16, bcol>>8, bcol));
	pset(screen, where.x+1, where.y+2, col>>16, col>>8, col);
	pset(screen, where.x+2, where.y+3, col>>16, col>>8, col);
	pset(screen, where.x+3, where.y+4, col>>16, col>>8, col);
	pset(screen, where.x+4, where.y+3, col>>16, col>>8, col);
	pset(screen, where.x+5, where.y+2, col>>16, col>>8, col);
}

int dtext(SDL_Surface * scrn, int x, int y, int w, const char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b)
{
	SDL_Color clrFg = {r, g, b,0};
	SDL_Rect rcDest = {x, y, w, 16};
	SDL_FillRect(scrn, &rcDest, SDL_MapRGB(scrn->format, 0, 0, 0));
	SDL_Surface *sText = TTF_RenderText_Solid(font, text, clrFg);
	SDL_BlitSurface(sText, NULL, scrn, &rcDest);
	SDL_FreeSurface(sText);
	return(0);
}

bool pos_rect(pos p, SDL_Rect r)
{
	return((p.x>=r.x)&&(p.y>=r.y)&&(p.x<r.x+r.w)&&(p.y<r.y+r.h));
}

void drawbutton(SDL_Surface *screen, button b)
{
	SDL_FillRect(screen, &b.posn, SDL_MapRGB(screen->format, b.col>>16, b.col>>8, b.col));
	if(b.img) SDL_BlitSurface(b.img, NULL, screen, &b.posn);
}

inline void pget(SDL_Surface * screen, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b)
{
	long int s_off = (y*screen->pitch) + x*screen->format->BytesPerPixel;
	unsigned long int *pixloc = (unsigned long int *)(((unsigned char *)screen->pixels)+s_off),
		pixval = *pixloc;
	SDL_GetRGB(pixval, screen->format, r, g, b);
}

void ksupdate(SDL_Surface * screen, button *buttons, js_type keystick)
{
	for(unsigned int i=0;i<4;i++)
		buttons[17+i].col=0x3f3f3f;
	for(unsigned int i=0;i<4;i++)
		drawbutton(screen, buttons[17+i]);
	if(keystick<4)
	{
		buttons[17+keystick].col=0x3fff3f;
		drawbutton(screen, buttons[17+keystick]);
	}
}