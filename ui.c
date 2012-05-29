/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	ui.c - User Interface
*/

#include "ui.h"
#include "bits.h"
#include "pbm.h"
#include <SDL_image.h>

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

void ui_offsets(bool keyboard, bool printer)
{
	y_cntl=296;
	y_keyb=y_cntl+80;
	if(keyboard) y_prnt=y_keyb+161;
	else y_prnt=y_keyb;
	if(printer) y_end=y_prnt+120;
	else y_end=y_prnt;
}

const char * const kmn[10]={"keyb_asst/keyb.png", "keyb_asst/km_K.png", "keyb_asst/km_KC.png", "keyb_asst/km_L.png", "keyb_asst/km_C.png", "keyb_asst/km_SS.png", "keyb_asst/km_E.png", "keyb_asst/km_ES.png", "keyb_asst/km_ESS.png", "keyb_asst/km_G.png"};
SDL_Surface *km[10];

void keyb_update(SDL_Surface *screen, unsigned int keyb_mode)
{
	if(km[keyb_mode])
		SDL_BlitSurface(km[keyb_mode], NULL, screen, &(SDL_Rect){0, y_keyb+1, screen->w, 160});
	else if(km[0])
		SDL_BlitSurface(km[0], NULL, screen, &(SDL_Rect){0, y_keyb+1, screen->w, 160});
}

SDL_Surface *configIMG_Load(const char *name)
{
	char *fullname=malloc(strlen(PREFIX)+16+strlen(name));
	if(fullname)
	{
		sprintf(fullname, PREFIX"/share/spiffy/%s", name);
		SDL_Surface *fp=IMG_Load(fullname);
		if(fp) return(fp);
	}
	return(IMG_Load(name));
}

void ui_init(SDL_Surface *screen, button **buttons, bool edgeload, bool pause, bool keyboard, bool printer)
{
	static button btn[nbuttons];
	*buttons=btn;
	for(unsigned int i=0;i<nbuttons;i++)
	{
		btn[i].img=NULL;
		btn[i].tooltip=NULL;
	}
	SDL_WM_SetCaption("Spiffy - ZX Spectrum 48k", "Spiffy");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_FillRect(screen, &(SDL_Rect){0, y_cntl, screen->w, 1}, SDL_MapRGB(screen->format, 255, 255, 255));
	SDL_FillRect(screen, &(SDL_Rect){0, y_cntl+1, screen->w, 63}, SDL_MapRGB(screen->format, 0, 0, 0)); // controls area
	SDL_FillRect(screen, &(SDL_Rect){0, y_cntl+64, screen->w, 16}, SDL_MapRGB(screen->format, 63, 63, 63)); // status bar
	if(keyboard)
	{
		for(unsigned int i=0;i<10;i++)
			km[i]=configIMG_Load(kmn[i]);
		SDL_FillRect(screen, &(SDL_Rect){0, y_keyb, screen->w, 1}, SDL_MapRGB(screen->format, 255, 255, 255));
		SDL_FillRect(screen, &(SDL_Rect){0, y_keyb+1, screen->w, 160}, SDL_MapRGB(screen->format, 0, 0, 0));
		if(km[0])
			SDL_BlitSurface(km[0], NULL, screen, &(SDL_Rect){0, y_keyb+1, screen->w, 160});
	}
	if(printer)
	{
		SDL_FillRect(screen, &(SDL_Rect){0, y_prnt, screen->w, 1}, SDL_MapRGB(screen->format, 255, 255, 255));
		SDL_FillRect(screen, &(SDL_Rect){0, y_prnt+1, screen->w, 119}, SDL_MapRGB(screen->format, 31, 31, 31)); // printer area
		SDL_FillRect(screen, &(SDL_Rect){32, y_prnt+100, 256, 20}, SDL_MapRGB(screen->format, 191, 191, 195)); // printer paper
	}
	FILE *fimg;
	string img;
	fimg=configopen("buttons/load.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[0]=(button){.img=pbm_string(img), .posn={116, y_cntl+2, 17, 17}, .col=0x8f8f1f, .tooltip="Load a tape image or snapshot"};
	free_string(&img);
	fimg=configopen("buttons/flash.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[1]=(button){.img=pbm_string(img), .posn={136, y_cntl+2, 17, 17}, .col=edgeload?0xffffff:0x1f1f1f, .tooltip=edgeload?"Disable edge-loader":"Enable edge-loader"};
	free_string(&img);
	fimg=configopen("buttons/play.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[2]=(button){.img=pbm_string(img), .posn={156, y_cntl+2, 17, 17}, .col=0x3fbf5f, .tooltip="Play the virtual tape"};
	free_string(&img);
	fimg=configopen("buttons/next.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[3]=(button){.img=pbm_string(img), .posn={176, y_cntl+2, 17, 17}, .col=0x07079f, .tooltip="Skip to the next tape block"};
	free_string(&img);
	fimg=configopen("buttons/stop.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[4]=(button){.img=pbm_string(img), .posn={196, y_cntl+2, 17, 17}, .col=0x3f0707, .tooltip="Stop the tape at the end of each block"};
	free_string(&img);
	fimg=configopen("buttons/rewind.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[5]=(button){.img=pbm_string(img), .posn={216, y_cntl+2, 17, 17}, .col=0x7f076f, .tooltip="Rewind to the beginning of the tape"};
	free_string(&img);
	fimg=configopen("buttons/pause.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[6]=(button){.img=pbm_string(img), .posn={8, y_cntl+44, 17, 17}, .col=pause?0xbf6f07:0x7f6f07, .tooltip=pause?"Unpause the emulation":"Pause the emulation"};
	free_string(&img);
	fimg=configopen("buttons/reset.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[7]=(button){.img=pbm_string(img), .posn={28, y_cntl+44, 17, 17}, .col=0xffaf07, .tooltip="Reset the Spectrum"};
	free_string(&img);
	fimg=configopen("buttons/bug.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[8]=(button){.img=pbm_string(img), .posn={48, y_cntl+44, 17, 17}, .col=0xbfff3f, .tooltip="Start the debugger"};
	free_string(&img);
#ifdef AUDIO
	btn[9].posn=(SDL_Rect){76, 321, 7, 6};
	btn[10].posn=(SDL_Rect){76, 328, 7, 6};
	btn[11].posn=(SDL_Rect){136, 321, 7, 6};
	btn[12].posn=(SDL_Rect){136, 328, 7, 6};
	fimg=configopen("buttons/record.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[13]=(button){.img=pbm_string(img), .posn={8, y_cntl+24, 17, 17}, .col=0x7f0707, .tooltip="Record audio"};
	drawbutton(screen, btn[13]);
	free_string(&img);
#endif /* AUDIO */
	fimg=configopen("buttons/snap.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[14]=(button){.img=pbm_string(img), .posn={68, y_cntl+44, 17, 17}, .col=0xbfbfbf, .tooltip="Save a snapshot"};
	drawbutton(screen, btn[14]);
	free_string(&img);
	fimg=configopen("buttons/trec.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[15]=(button){.img=pbm_string(img), .posn={236, y_cntl+2, 17, 17}, .col=0x4f0f0f, .tooltip="Record tape"};
	drawbutton(screen, btn[15]);
	free_string(&img);
	fimg=configopen("buttons/feed.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[16]=(button){.img=pbm_string(img), .posn={88, y_cntl+44, 17, 17}, .col=printer?0x3f276f:0x170f1f, .tooltip=printer?"ZX Printer: Paper feed":"ZX Printer is disabled"};
	drawbutton(screen, btn[16]);
	free_string(&img);
	fimg=configopen("buttons/js_c.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[17]=(button){.img=pbm_string(img), .posn={108, y_cntl+44, 9, 9}, .col=0x3fff3f, .tooltip="Select CURSOR keystick"};
	free_string(&img);
	fimg=configopen("buttons/js_s.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[18]=(button){.img=pbm_string(img), .posn={116, y_cntl+44, 9, 9}, .col=0x3f3f3f, .tooltip="Select SINCLAIR keystick"};
	free_string(&img);
	fimg=configopen("buttons/js_k.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[19]=(button){.img=pbm_string(img), .posn={108, y_cntl+52, 9, 9}, .col=0x3f3f3f, .tooltip="Select KEMPSTON keystick"};
	free_string(&img);
	fimg=configopen("buttons/js_x.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[20]=(button){.img=pbm_string(img), .posn={116, y_cntl+52, 9, 9}, .col=0x3f3f3f, .tooltip="Disable keystick"};
	free_string(&img);
	ksupdate(screen, *buttons, JS_C);
	fimg=configopen("buttons/bw.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[21]=(button){.img=pbm_string(img), .posn={128, y_cntl+44, 17, 17}, .col=0x9f9f9f, .tooltip="Enable Black&White filter"};
	drawbutton(screen, btn[21]);
	free_string(&img);
	fimg=configopen("buttons/scan.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[22]=(button){.img=pbm_string(img), .posn={148, y_cntl+44, 17, 17}, .col=0x5f5fcf, .tooltip="Enable TV Scanlines filter"};
	drawbutton(screen, btn[22]);
	free_string(&img);
	fimg=configopen("buttons/blur.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[23]=(button){.img=pbm_string(img), .posn={168, y_cntl+44, 17, 17}, .col=0xbf3f3f, .tooltip="Enable Horizontal Blur filter"};
	drawbutton(screen, btn[23]);
	free_string(&img);
	fimg=configopen("buttons/vblur.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[24]=(button){.img=pbm_string(img), .posn={188, y_cntl+44, 17, 17}, .col=0xbf3f3f, .tooltip="Enable Vertical Blur filter"};
	drawbutton(screen, btn[24]);
	free_string(&img);
	fimg=configopen("buttons/misg.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[25]=(button){.img=pbm_string(img), .posn={208, y_cntl+44, 17, 17}, .col=0x1f5f1f, .tooltip="Enable Misaligned Green filter"};
	drawbutton(screen, btn[25]);
	free_string(&img);
	fimg=configopen("buttons/slow.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	btn[26]=(button){.img=pbm_string(img), .posn={228, y_cntl+44, 17, 17}, .col=0x8f8faf, .tooltip="Enable Slow Fade filter"};
	drawbutton(screen, btn[26]);
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

int dtext(SDL_Surface * scrn, int x, int y, int w, const char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b, unsigned char br, unsigned char bg, unsigned char bb)
{
	SDL_Color clrFg = {r, g, b, 0};
	SDL_Rect rcDest = {x, y, w, 16};
	SDL_FillRect(scrn, &rcDest, SDL_MapRGB(scrn->format, br, bg, bb));
	if(text&&font)
	{
		SDL_Surface *sText = TTF_RenderText_Solid(font, text, clrFg);
		SDL_BlitSurface(sText, NULL, scrn, &rcDest);
		SDL_FreeSurface(sText);
	}
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
