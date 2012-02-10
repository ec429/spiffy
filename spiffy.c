/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-11
	License: GNU GPL v3+
	
	Acknowledgements (Heavily used references)
	Z80 core based on Cristian Dinu's "Decoding Z80 Opcodes" http://www.z80.info/decoding.htm
	 and Sean Young's "Z80 Undocumented Features" http://www.gaby.de/z80/z80undoc3.txt
	Timings etc. chiefly from WoS' "Z80 Instruction Information" http://www.worldofspectrum.org/Z80instructions.html
	 and CSSFAQ "16K / 48K ZX Spectrum Reference" http://www.worldofspectrum.org/faq/reference/48kreference.htm#ZXSpectrum
*/

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL.h>
#include <SDL/SDL_audio.h>
#include <SDL/SDL_ttf.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <libspectrum.h>
#include "bits.h"
#include "ops.h"
#include "z80.h"
#include "vchips.h"

// SDL surface params
#define OSIZ_X	320
#define OSIZ_Y	320
#define OBPP	32

#define AUDIO		// Activates audio

#ifdef AUDIO
#define SINC_RATE	8
#define SAMPLE_RATE	8000 // Audio sample rate, Hz
#define AUDIOBUFLEN	(SAMPLE_RATE/100)
#define SINCBUFLEN	(AUDIOBUFLEN*SINC_RATE)
#endif

#define ROM_FILE "48.rom" // Location of Spectrum ROM file (TODO: make configable)

#define VERSION_MAJ	0
#define VERSION_MIN	4
#define VERSION_REV	0

#define VERSION_MSG "spiffy %hhu.%hhu.%hhu\n\
 Copyright (C) 2010-11 Edward Cree.\n\
 License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
 This is free software: you are free to change and redistribute it.\n\
 There is NO WARRANTY, to the extent permitted by law.\n",VERSION_MAJ, VERSION_MIN, VERSION_REV

#define GPL_MSG "spiffy Copyright (C) 2010-11 Edward Cree.\n\
 This program comes with ABSOLUTELY NO WARRANTY; for details see the GPL v3.\n\
 This is free software, and you are welcome to redistribute it\n\
 under certain conditions: GPL v3+\n"

typedef struct _pos
{
	int x;
	int y;
} pos;

SDL_Surface * gf_init();
void pset(SDL_Surface * screen, int x, int y, unsigned char r, unsigned char g, unsigned char b);
int line(SDL_Surface * screen, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b);
#ifdef AUDIO
void mixaudio(void *abuf, Uint8 *stream, int len);
typedef struct
{
	bool bits[SINCBUFLEN];
	bool cbuf[SINCBUFLEN];
	unsigned int rp, wp; // read & write pointers for 'bits' circular buffer
	bool play; // true if tape is playing (we mute and allow skipping)
}
audiobuf;

double sincgroups[SINC_RATE][AUDIOBUFLEN];
#endif

typedef struct
{
	bool memwait;
	bool iowait;
	bool t1;
}
ula_t;

// helper fns
void show_state(const unsigned char * RAM, const z80 *cpu, int Tstates, const bus_t *bus);
void scrn_update(SDL_Surface *screen, int Tstates, int frames, int frameskip, int Fstate, const unsigned char *RAM, bus_t *bus, ula_t *ula);
int dtext(SDL_Surface * scrn, int x, int y, const char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b);
bool pos_rect(pos p, SDL_Rect r);

#ifdef CORETEST
static int read_test( FILE *f, unsigned int *end_tstates, z80 *cpu, unsigned char *memory);
static void dump_z80_state( z80 *cpu, unsigned int tstates );
static void dump_memory_state( unsigned char *memory, unsigned char *initial_memory );
static int run_test( FILE *f );
#endif

int main(int argc, char * argv[])
{
	TTF_Font *font=NULL;
	if(!TTF_Init())
	{
		font=TTF_OpenFont("Vera.ttf", 12);
	}
	bool debug=false; // Generate debugging info?
	bool bugstep=false; // Single-step?
	bool debugcycle=false; // Single-Tstate stepping?
	#ifdef CORETEST
	bool coretest=false; // run the core tests?
	#endif
	#ifdef AUDIO
	bool delay=true; // attempt to maintain approximately a true Speccy speed, 50fps at 69888 T-states per frame, which is 3.4944MHz
	{
		unsigned int sinclen=AUDIOBUFLEN*SINC_RATE;
		double sinc[sinclen];
		for(unsigned int i=0;i<sinclen;i++)
		{
			double v=16.0*(i/(double)sinclen-0.5);
			sinc[i]=v?sin(v)/v:1;
		}
		for(unsigned int g=0;g<SINC_RATE;g++)
		{
			for(unsigned int j=0;j<AUDIOBUFLEN;j++)
				sincgroups[g][j]=0;
			for(unsigned int i=0;i<sinclen;i++)
			{
				unsigned int j=(i+g)/SINC_RATE;
				if(j<AUDIOBUFLEN)
					sincgroups[g][j]+=sinc[i];
			}
		}
	}
	#endif
	const char *fn=NULL;
	unsigned int breakpoint=-1;
	int arg;
	for (arg=1; arg<argc; arg++)
	{
		if((strcmp(argv[arg], "--version") == 0) || (strcmp(argv[arg], "-V") == 0))
		{ // print version info and exit
			printf(VERSION_MSG);
			return(0);
		}
		else if((strcmp(argv[arg], "--debug") == 0) || (strcmp(argv[arg], "-d") == 0))
		{ // activate debugging mode
			debug=true;
		}
		else if(strncmp(argv[arg], "-b=", 3) == 0)
		{ // activate debugging mode at a breakpoint
			sscanf(argv[arg]+3, "%04x", &breakpoint);
		}
		else if((strcmp(argv[arg], "--step") == 0) || (strcmp(argv[arg], "-s") == 0))
		{ // activate single-step mode under debugger
			bugstep=true;
		}
		else if((strcmp(argv[arg], "--Tstate") == 0) || (strcmp(argv[arg], "-T") == 0))
		{ // activate single-Tstate stepping under debugger
			debugcycle=true;
		}
		else if((strcmp(argv[arg], "--no-Tstate") == 0) || (strcmp(argv[arg], "+T") == 0))
		{ // deactivate single-Tstate stepping under debugger
			debugcycle=false;
		}
		#ifdef CORETEST
		else if((strcmp(argv[arg], "--coretest") == 0) || (strcmp(argv[arg], "-c") == 0))
		{ // run the core tests
			coretest=true;
		}
		#endif
		else
		{ // unrecognised option, assume it's a filename
			fn=argv[arg];
		}
	}
	
	printf(GPL_MSG);
	bool ls=!libspectrum_init();
	
	#ifdef CORETEST
	if(coretest)
	{
		FILE *f;
		const char *progname = argv[0];
		const char *testsfile = "tests.in";
		/* Initialise the tables used by the Z80 core */
		z80_init();

		f = fopen( testsfile, "r" );
		if( !f ) {
			fprintf( stderr, "%s: couldn't open tests file `%s': %s\n", progname,
				 testsfile, strerror( errno ) );
			return 1;
		}
		while(run_test(f));
		if( fclose(f) ) {
			fprintf( stderr, "%s: couldn't close `%s': %s\n", progname, testsfile,
				 strerror( errno ) );
			return 1;
		}
		return 0;
	}
	#endif
	
	// State
	z80 _cpu, *cpu=&_cpu; // we want to work with a pointer
	bus_t _bus, *bus=&_bus;
	ula_t _ula, *ula=&_ula;
	
	SDL_Surface * screen=gf_init();
	SDL_WM_SetCaption("Spiffy - ZX Spectrum 48k", "Spiffy");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_Event event;
	SDL_Rect cls={0, 296, OSIZ_X, OSIZ_Y-296};
	SDL_FillRect(screen, &cls, SDL_MapRGB(screen->format, 0, 0, 0));
	SDL_Rect playbutton={164, 298, 16, 20};
	SDL_FillRect(screen, &playbutton, SDL_MapRGB(screen->format, 0x3f, 0xbf, 0x5f));
	SDL_Rect nextbutton={184, 298, 16, 20};
	SDL_FillRect(screen, &nextbutton, SDL_MapRGB(screen->format, 0x07, 0x07, 0x9f));
	SDL_Rect rewindbutton={204, 298, 16, 20};
	SDL_FillRect(screen, &rewindbutton, SDL_MapRGB(screen->format, 0x7f, 0x07, 0x6f));
	int errupt = 0;
	bus->portfe=0; // used by mixaudio (for the beeper), tape writing (MIC) and the screen update (for the BORDCR)
	bool ear=false; // tape reading EAR
	bool kstate[8][5]; // keyboard state
	unsigned char kenc[8]; // encoded keyboard state
	for(int i=0;i<8;i++)
	{
		for(int j=0;j<5;j++)
			kstate[i][j]=false;
		kenc[i]=0;
	}
	if(init_keyboard())
	{
		fprintf(stderr, "spiffy: failed to load keymap\n");
		return(1);
	}
#ifdef AUDIO
	SDL_AudioSpec fmt;
	fmt.freq = SAMPLE_RATE;
	fmt.format = AUDIO_U8;
	fmt.channels = 1;
	fmt.samples = AUDIOBUFLEN;
	fmt.callback = mixaudio;
	audiobuf abuf = {.rp=0, .wp=0};
	fmt.userdata = &abuf;

	/* Open the audio device */
	if ( SDL_OpenAudio(&fmt, NULL) < 0 ) {
		fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
		return(3);
	}
#endif	

	struct timeval frametime[100];
	gettimeofday(frametime, NULL);
	for(int i=1;i<100;i++) frametime[i]=frametime[0];
	
	// Mouse handling
	pos mouse;
	SDL_GetMouseState(&mouse.x, &mouse.y);
	//SDL_ShowCursor(SDL_DISABLE);
	char button;
	
	// Spectrum State
	FILE *fp = fopen(ROM_FILE, "rb");
	unsigned char RAM[65536];
	ramtop=65536;
	unsigned int i;
	for(i=0;i<ramtop;i++)
	{
		if(i<16384)
		{
			RAM[i]=fgetc(fp);
		}
		else
		{
			RAM[i]=0;
		}
	}
	fclose(fp);
	
	z80_init(); // initialise decoding tables
	int Fstate=0; // FLASH state
	z80_reset(cpu, bus);
	
	libspectrum_tape *deck=NULL;
	bool play=false;
	int oldtapeblock=-1;
	unsigned int tapeblocklen=0;
	
	if(ls&&fn)
	{
		FILE *fp=fopen(fn, "rb");
		string data=sslurp(fp);
		fclose(fp);
		libspectrum_id_t type;
		if(libspectrum_identify_file_raw(&type, fn, (unsigned char *)data.buf, data.i))
		{
			free_string(&data);
			fn=NULL;
		}
		else
		{
			libspectrum_class_t class;
			if(libspectrum_identify_class(&class, type))
			{
				free_string(&data);
				fn=NULL;
			}
			else
			{
				switch(class)
				{
					case LIBSPECTRUM_CLASS_TAPE:
						if((deck=libspectrum_tape_alloc()))
						{
							if(libspectrum_tape_read(deck, (unsigned char *)data.buf, data.i, type, fn))
							{
								libspectrum_tape_free(deck);
								free_string(&data);
								fn=NULL;
							}
							else
							{
								fprintf(stderr, "Mounted tape '%s'\n", fn);
							}
						}
					break;
					default:
						fprintf(stderr, "This class of file is not supported!\n");
						free_string(&data);
						fn=NULL;
					break;
				}
			}
		}
	}
	
	SDL_Flip(screen);
#ifdef AUDIO
	// Start sound
	SDL_PauseAudio(0);
#endif
	
	int frames=0;
	int Tstates=0;
	uint32_t T_to_tape_edge=0;
	int edgeflags=0;
	
	// Main program loop
	while(likely(!errupt))
	{
		if(unlikely((!debug)&&(*PC==breakpoint)&&bus->m1))
			debug=true;
		Tstates++;
		#ifdef AUDIO
		if(!(Tstates%(69888*50/(SAMPLE_RATE*SINC_RATE))))
		{
			abuf.play=play;
			unsigned int newwp=(abuf.wp+1)%SINCBUFLEN;
			if(delay&&!play)
				while(newwp==abuf.rp) usleep(5e3);
			abuf.bits[abuf.wp]=(bus->portfe&0x10);
			abuf.bits[abuf.wp]^=ear;
			abuf.wp=newwp;
		}
		#endif
		if(likely(play))
		{
			if(unlikely(!deck))
				play=false;
			else if(T_to_tape_edge)
				T_to_tape_edge--;
			else
			{
				int block;
				if(likely(!libspectrum_tape_position(&block, deck)))
				{
					if(unlikely(block!=oldtapeblock))
					{
						tapeblocklen=(libspectrum_tape_block_length(libspectrum_tape_current_block(deck))+69887)/69888;
						oldtapeblock=block;
					}
				}
				if(edgeflags&LIBSPECTRUM_TAPE_FLAGS_STOP)
					play=false;
				if(edgeflags&LIBSPECTRUM_TAPE_FLAGS_STOP48)
					play=false;
				if(!(edgeflags&LIBSPECTRUM_TAPE_FLAGS_NO_EDGE))
					ear=!ear;
				if(edgeflags&LIBSPECTRUM_TAPE_FLAGS_LEVEL_LOW)
					ear=false;
				if(edgeflags&LIBSPECTRUM_TAPE_FLAGS_LEVEL_HIGH)
					ear=true;
				if(edgeflags&LIBSPECTRUM_TAPE_FLAGS_TAPE)
				{
					play=false;
					T_to_tape_edge=0;
					edgeflags=0;
				}
				if(play)
					libspectrum_tape_get_next_edge(&T_to_tape_edge, &edgeflags, deck);
				SDL_FillRect(screen, &playbutton, play?SDL_MapRGB(screen->format, 0xbf, 0x1f, 0x3f):SDL_MapRGB(screen->format, 0x3f, 0xbf, 0x5f));
			}
		}
		if(bus->mreq)
			do_ram(RAM, bus, false);
		
		if(unlikely(bus->iorq&&(bus->tris==OUT)))
		{
			if(!(bus->addr&0x01))
				bus->portfe=bus->data;
		}
		
		if(unlikely(bus->iorq&&(bus->tris==IN)))
		{
			if(!(bus->addr&0x01))
			{
				unsigned char hi=bus->addr>>8;
				bus->data=(ear?0x40:0)|0x1f;
				for(int i=0;i<8;i++)
					if(!(hi&(1<<i)))
						bus->data&=~kenc[i];
			}
			else
				bus->data=0xff; // technically this is wrong, TODO floating bus
		}
		
		if(unlikely(debug&&(((cpu->M==0)&&(cpu->dT==0)&&(cpu->shiftstate==0))||debugcycle)))
		{
			show_state(RAM, cpu, Tstates, bus);
			if(bugstep)
			{
				if(getchar()=='c')
					debug=false;
			}
		}
		
		if(cpu->nothing)
		{
			cpu->nothing--;	
			if(cpu->steps)
				cpu->steps--;
			cpu->dT++;
		}
		else
			errupt=z80_tstep(cpu, bus, errupt);

		scrn_update(screen, Tstates, frames, play?7:0, Fstate, RAM, bus, ula);
		if(unlikely(Tstates==32))
			bus->irq=false;
		if(unlikely(Tstates>=69888))
		{
			bus->irq=true;
			SDL_Flip(screen);
			Tstates-=69888;
			Fstate=(Fstate+1)&0x1f; // flash alternates every 16 frames
			struct timeval tn;
			gettimeofday(&tn, NULL);
			double spd=min(200/(tn.tv_sec-frametime[frames%100].tv_sec+1e-6*(tn.tv_usec-frametime[frames%100].tv_usec)),999);
			frametime[frames++%100]=tn;
			if(!(frames%25))
			{
				char text[32];
				if(spd>=1)
					sprintf(text, "Speed: %0.3g%%", spd);
				else
					sprintf(text, "Speed: <1%%");
				dtext(screen, 8, 296, text, font, 255, 255, 0);
			}
			if(play)
				tapeblocklen=max(tapeblocklen, 1)-1;
			if(!(frames%25))
			{
				char text[32];
				sprintf(text, "T%03u", (tapeblocklen+49)/50);
				dtext(screen, 224, 298, text, font, 0xbf, 0xbf, 0xbf);
			}
			while(SDL_PollEvent(&event))
			{
				switch (event.type)
				{
					case SDL_QUIT:
						errupt++;
					break;
					case SDL_KEYDOWN:
						if(event.key.type==SDL_KEYDOWN)
						{
							SDL_keysym key=event.key.keysym;
							if(key.sym==SDLK_ESCAPE)
							{
								debug=true;
								bugstep=true;
							}
							#ifdef AUDIO
							else if(key.sym==SDLK_KP_ENTER)
							{
								abuf.wp=(abuf.wp+1)%AUDIOBUFLEN;
							}
							#endif
							else if(key.sym==SDLK_RETURN)
							{
								kstate[6][0]=true;
							}
							else if(key.sym==SDLK_BACKSPACE)
							{
								kstate[0][0]=true;
								kstate[4][0]=true;
							}
							else if(key.sym==SDLK_LEFT)
							{
								kstate[0][0]=true;
								kstate[3][4]=true;
							}
							else if(key.sym==SDLK_DOWN)
							{
								kstate[0][0]=true;
								kstate[4][4]=true;
							}
							else if(key.sym==SDLK_UP)
							{
								kstate[0][0]=true;
								kstate[4][3]=true;
							}
							else if(key.sym==SDLK_RIGHT)
							{
								kstate[0][0]=true;
								kstate[4][2]=true;
							}
							else if(key.sym==SDLK_CAPSLOCK)
							{
								kstate[0][0]=true;
								kstate[3][1]=true;
							}
							else if((key.sym==SDLK_LSHIFT)||(key.sym==SDLK_RSHIFT))
							{
								kstate[0][0]=true;
							}
							else if((key.sym==SDLK_LCTRL)||(key.sym==SDLK_RCTRL))
							{
								kstate[7][1]=true;
							}
							else if(key.sym==SDLK_LESS)
							{
								kstate[2][3]=true;
								kstate[7][1]=true;
							}
							else if(key.sym==SDLK_GREATER)
							{
								kstate[2][4]=true;
								kstate[7][1]=true;
							}
							else if(key.sym==SDLK_KP_PLUS)
							{
								kstate[6][2]=true;
								kstate[7][1]=true;
							}
							else if((key.sym==SDLK_MINUS)||(key.sym==SDLK_KP_MINUS))
							{
								kstate[6][3]=true;
								kstate[7][1]=true;
							}
							else if(key.sym==SDLK_EQUALS)
							{
								kstate[6][1]=true;
								kstate[7][1]=true;
							}
							else if(key.sym==SDLK_QUESTION)
							{
								kstate[0][3]=true;
								kstate[7][1]=true;
							}
							else if(key.sym==SDLK_KP_MULTIPLY)
							{
								kstate[7][4]=true;
								kstate[7][1]=true;
							}
							else if(key.sym==SDLK_KP_DIVIDE)
							{
								kstate[0][4]=true;
								kstate[7][1]=true;
							}
							else if((key.sym&0xFF80)==0)
							{
								char k=key.sym&0x7F;
								for(unsigned int i=0;i<nkmaps;i++)
								{
									if(kmap[i].key==k)
									{
										kstate[kmap[i].row[0]][kmap[i].col[0]]=true;
										if(kmap[i].twokey)
											kstate[kmap[i].row[1]][kmap[i].col[1]]=true;
									}
								}
							}
							for(unsigned int i=0;i<8;i++)
							{
								kenc[i]=0;
								for(unsigned int j=0;j<5;j++)
									if(kstate[i][j]) kenc[i]|=(1<<j);
							}
							// else it's not [low] ASCII
						}
					break;
					case SDL_KEYUP:
						if(event.key.type==SDL_KEYUP)
						{
							SDL_keysym key=event.key.keysym;
							if(key.sym==SDLK_ESCAPE)
							{
								debug=true;
								bugstep=true;
							}
							else if(key.sym==SDLK_RETURN)
							{
								kstate[6][0]=false;
							}
							else if(key.sym==SDLK_BACKSPACE)
							{
								kstate[0][0]=false;
								kstate[4][0]=false;
							}
							else if(key.sym==SDLK_LEFT)
							{
								kstate[0][0]=false;
								kstate[3][4]=false;
							}
							else if(key.sym==SDLK_DOWN)
							{
								kstate[0][0]=false;
								kstate[4][4]=false;
							}
							else if(key.sym==SDLK_UP)
							{
								kstate[0][0]=false;
								kstate[4][3]=false;
							}
							else if(key.sym==SDLK_RIGHT)
							{
								kstate[0][0]=false;
								kstate[4][2]=false;
							}
							else if(key.sym==SDLK_CAPSLOCK)
							{
								kstate[0][0]=false;
								kstate[3][1]=false;
							}
							else if((key.sym==SDLK_LSHIFT)||(key.sym==SDLK_RSHIFT))
							{
								kstate[0][0]=false;
							}
							else if((key.sym==SDLK_LCTRL)||(key.sym==SDLK_RCTRL))
							{
								kstate[7][1]=false;
							}
							else if(key.sym==SDLK_LESS)
							{
								kstate[2][3]=false;
								kstate[7][1]=false;
							}
							else if(key.sym==SDLK_GREATER)
							{
								kstate[2][4]=false;
								kstate[7][1]=false;
							}
							else if(key.sym==SDLK_KP_PLUS)
							{
								kstate[6][2]=false;
								kstate[7][1]=false;
							}
							else if((key.sym==SDLK_MINUS)||(key.sym==SDLK_KP_MINUS))
							{
								kstate[6][3]=false;
								kstate[7][1]=false;
							}
							else if(key.sym==SDLK_EQUALS)
							{
								kstate[6][1]=false;
								kstate[7][1]=false;
							}
							else if(key.sym==SDLK_QUESTION)
							{
								kstate[0][3]=false;
								kstate[7][1]=false;
							}
							else if(key.sym==SDLK_KP_MULTIPLY)
							{
								kstate[7][4]=false;
								kstate[7][1]=false;
							}
							else if(key.sym==SDLK_KP_DIVIDE)
							{
								kstate[0][4]=false;
								kstate[7][1]=false;
							}
							else if((key.sym&0xFF80)==0)
							{
								char k=key.sym&0x7F;
								for(unsigned int i=0;i<nkmaps;i++)
								{
									if(kmap[i].key==k)
									{
										kstate[kmap[i].row[0]][kmap[i].col[0]]=false;
										if(kmap[i].twokey)
											kstate[kmap[i].row[1]][kmap[i].col[1]]=false;
									}
								}
							}
							for(unsigned int i=0;i<8;i++)
							{
								kenc[i]=0;
								for(unsigned int j=0;j<5;j++)
									if(kstate[i][j]) kenc[i]|=(1<<j);
							}
							// else it's not [low] ASCII
						}
					break;
					case SDL_MOUSEMOTION:
						mouse.x=event.motion.x;
						mouse.y=event.motion.y;
					break;
					case SDL_MOUSEBUTTONDOWN:
						mouse.x=event.button.x;
						mouse.y=event.button.y;
						button=event.button.button;
						switch(button)
						{
							case SDL_BUTTON_LEFT:
								if(pos_rect(mouse, playbutton))
								{
									play=!play;
								}
								else if(pos_rect(mouse, nextbutton))
								{
									if(deck) libspectrum_tape_select_next_block(deck);
								}
								else if(pos_rect(mouse, rewindbutton))
								{
									if(deck) libspectrum_tape_nth_block(deck, 0);
								}
								SDL_FillRect(screen, &playbutton, play?SDL_MapRGB(screen->format, 0xbf, 0x1f, 0x3f):SDL_MapRGB(screen->format, 0x3f, 0xbf, 0x5f));
							break;
							case SDL_BUTTON_RIGHT:
							break;
							case SDL_BUTTON_WHEELUP:
							break;
							case SDL_BUTTON_WHEELDOWN:
							break;
						}
					break;
					case SDL_MOUSEBUTTONUP:
						mouse.x=event.button.x;
						mouse.y=event.button.y;
						button=event.button.button;
						switch(button)
						{
							case SDL_BUTTON_LEFT:
							break;
							case SDL_BUTTON_RIGHT:
							break;
							case SDL_BUTTON_WHEELUP:
							break;
							case SDL_BUTTON_WHEELDOWN:
							break;
						}
					break;
				}
			}
		}
	}
	
	show_state(RAM, cpu, Tstates, bus);
	
	#ifdef AUDIO
	// Stop sound
	SDL_PauseAudio(1);
	#endif
	
	// clean up
	if(SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
	raise(SIGKILL);
	return(0);
}

SDL_Surface * gf_init()
{
	SDL_Surface * screen;
	if(SDL_Init(SDL_INIT_VIDEO)<0)
	{
		perror("SDL_Init");
		return(NULL);
	}
	atexit(SDL_Quit);
	if((screen = SDL_SetVideoMode(OSIZ_X, OSIZ_Y, OBPP, SDL_HWSURFACE))==0)
	{
		perror("SDL_SetVideoMode");
		SDL_Quit();
		return(NULL);
	}
	if(SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0)
	{
		perror("SDL_LockSurface");
		return(NULL);
	}
	return(screen);
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

#ifdef AUDIO
void mixaudio(void *abuf, Uint8 *stream, int len)
{
	audiobuf *a=abuf;
	for(int i=0;i<len;i++)
	{
		for(unsigned int g=0;g<SINC_RATE;g++)
		{
			while(!a->play&&(a->rp==a->wp)) usleep(5e3);
			a->cbuf[a->rp]=a->bits[a->rp];
			a->rp=(a->rp+1)%SINCBUFLEN;
		}
		unsigned int l=a->play?SINCBUFLEN>>2:SINCBUFLEN;
		double v=0;
		for(unsigned int j=0;j<l;j++)
		{
			int d=a->cbuf[(a->rp+SINCBUFLEN-j)%SINCBUFLEN]-a->cbuf[(a->rp+SINCBUFLEN-j-1)%SINCBUFLEN];
			v+=d*sincgroups[SINC_RATE-(j%SINC_RATE)-1][j/SINC_RATE];
		}
		if(a->play) v*=0.2;
		stream[i]=floor(v*4.0+127.5);
	}
}
#endif

void show_state(const unsigned char * RAM, const z80 *cpu, int Tstates, const bus_t *bus)
{
	int i;
	printf("\nState: %c%c%c%c%c%c%c%c\n", cpu->regs[2]&0x80?'S':'-', cpu->regs[2]&0x40?'Z':'-', cpu->regs[2]&0x20?'5':'-', cpu->regs[2]&0x10?'H':'-', cpu->regs[2]&0x08?'3':'-', cpu->regs[2]&0x04?'P':'-', cpu->regs[2]&0x02?'N':'-', cpu->regs[2]&0x01?'C':'-');
	printf("P C  A F  B C  D E  H L  I x  I y  I R  S P  a f  b c  d e  h l  IFF IM\n");
	for(i=0;i<26;i+=2)
	{
		printf("%02x%02x ", cpu->regs[i+1], cpu->regs[i]);
	}
	printf("%c %c %1x\n", cpu->IFF[0]?'1':'0', cpu->IFF[1]?'1':'0', cpu->intmode);
	printf("\n");
	printf("Memory\t- near (PC), (HL) and (SP):\n");
	for(i=0;i<5;i++)
	{
		unsigned short int off = (*PC) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = (*HL) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = (*SP) + 2*(i);
		printf("%04x: %02x%02x\n", (unsigned short int)off, RAM[(off+1)%(1<<16)], RAM[off]);
	}
	printf("\t- near (BC), (DE) and (nn):\n");
	for(i=0;i<5;i++)
	{
		unsigned short int off = (*BC) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = (*DE) + 8*(i-2);
		off-=off%8;
		printf("%04x: %02x%02x %02x%02x %02x%02x %02x%02x\t", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3], RAM[off+4], RAM[off+5], RAM[off+6], RAM[off+7]);
		off = I16 + 4*(i-2);
		off-=off%4;
		printf("%04x: %02x%02x %02x%02x\n", (unsigned short int)off, RAM[off], RAM[off+1], RAM[off+2], RAM[off+3]);
	}
	printf("\n");
	printf("T-states: %u\tM-cycle: %u[%d]\tInternal regs: %02x-%02x-%02x\tShift state: %u", Tstates, cpu->M, cpu->dT, cpu->internal[0], cpu->internal[1], cpu->internal[2], cpu->shiftstate);
	if(cpu->nmiacc) printf(" NMI!");
	else if(cpu->intacc) printf(" INT!");
	printf("\n");
	printf("Bus: A=%04x\tD=%02x\t%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", bus->addr, bus->data, bus->tris==OUT?"WR":"wr", bus->tris==IN?"RD":"rd", bus->mreq?"MREQ":"mreq", bus->iorq?"IORQ":"iorq", bus->m1?"M1":"m1", bus->rfsh?"RFSH":"rfsh", bus->waitline?"WAIT":"wait", bus->irq?"INT":"int", bus->nmi?"NMI":"nmi", bus->halt?"HALT":"halt");
}

void scrn_update(SDL_Surface *screen, int Tstates, int frames, int frameskip, int Fstate, const unsigned char *RAM, bus_t *bus, ula_t *ula) // TODO: Maybe one day generate floating bus & ULA snow, but that will be hard!
{
	int line=((Tstates+12)/224)-16;
	int col=(((Tstates+12)%224)<<1);
	if(likely((line>=0) && (line<296)))
	{
		bool contend=false;
		if((col>=0) && (col<OSIZ_X))
		{
			unsigned char uladb=0xff, ulaab=bus->portfe&0x07;
			int ccol=(col>>3)-4;
			int crow=(line>>3)-6;
			if((ccol>=0) && (ccol<0x20) && (crow>=0) && (crow<0x18))
			{
				unsigned short int	dbh=0x40|(crow&0x18)|(line%8),
									dbl=((crow&0x7)<<5)|ccol,
									abh=0x58|(crow>>3),
									abl=dbl;
				uladb=RAM[(dbh<<8)+dbl];
				ulaab=RAM[(abh<<8)+abl];
				contend=!(((Tstates%8)==6)||((Tstates%8)==7));
			}
			if(ula->t1)
			{
				if(((bus->addr&0xC000)==0x4000))
					ula->memwait=true;
				if(bus->iorq&&!(bus->addr&1))
					ula->iowait=true;
				ula->t1=false;
			}
			else
			{
				if(!bus->mreq)
					ula->memwait=false;
				if(!bus->iorq)
					ula->iowait=false;
				ula->t1=!bus->mreq;
			}
			bus->clk_inhibit=(ula->memwait||ula->iowait)&&contend;
			if(!(frames&frameskip))
			{
				int ink=ulaab&0x07;
				int paper=(ulaab&0x38)>>3;
				bool flash=ulaab&0x80;
				bool bright=ulaab&0x40;
				int pr,pg,pb,ir,ig,ib; // blue1 red2 green4
				unsigned char t=bright?240:200;
				pr=(paper&2)?t:0;
				pg=(paper&4)?t:0;
				pb=(paper&1)?t:0;
				if(paper==1) pb+=15;
				ir=(ink&2)?t:0;
				ig=(ink&4)?t:0;
				ib=(ink&1)?t:0;
				if(ink==1) ib+=15;
				unsigned char s=0x80>>(((Tstates+12)%4)<<1);
				bool d=uladb&s;
				if(flash && (Fstate&0x10))
					d=!d;
				pset(screen, col, line, d?ir:pr, d?ig:pg, d?ib:pb);
				d=uladb&(s>>1);
				if(flash && (Fstate&0x10))
					d=!d;
				pset(screen, col+1, line, d?ir:pr, d?ig:pg, d?ib:pb);
			}
		}
	}
}

int dtext(SDL_Surface * scrn, int x, int y, const char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b)
{
	SDL_Color clrFg = {r, g, b,0};
	SDL_Rect rcDest = {x, y, 100, 12};
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

#ifdef CORETEST
static int
read_test( FILE *f, unsigned int *end_tstates, z80 *cpu, unsigned char *memory )
{
	const char *progname="spiffy";
	unsigned af, bc, de, hl, af_, bc_, de_, hl_, ix, iy, sp, pc;
	unsigned i, r, iff1, iff2, im;
	unsigned end_tstates2;
	unsigned address;
	char test_name[ 80 ];

	do {

		if( !fgets( test_name, sizeof( test_name ), f ) ) {

			if( feof( f ) ) return 1;

			fprintf( stderr, "%s: error reading test description from file: %s\n",
				 progname, strerror( errno ) );
			return 1;
		}

	} while( test_name[0] == '\n' );

	if( fscanf( f, "%x %x %x %x %x %x %x %x %x %x %x %x", &af, &bc,
				&de, &hl, &af_, &bc_, &de_, &hl_, &ix, &iy, &sp, &pc ) != 12 ) {
		fprintf( stderr, "%s: first registers line in file corrupt\n", progname);
		return 1;
	}

	*AF	= af;	*BC	= bc;	*DE	= de;	*HL	= hl;
	*AF_ = af_; *BC_ = bc_; *DE_ = de_; *HL_ = hl_;
	*Ix	= ix;	*Iy	= iy;	*SP	= sp;	*PC	= pc;

	int halted;
	if( fscanf( f, "%x %x %u %u %u %d %u", &i, &r, &iff1, &iff2, &im,
				&halted, &end_tstates2 ) != 7 ) {
		fprintf( stderr, "%s: second registers line in file corrupt\n", progname);
		return 1;
	}

	*Intvec = i; *Refresh = r; cpu->IFF[0] = iff1; cpu->IFF[1] = iff2; cpu->intmode = im; cpu->halt=halted;
	*end_tstates = end_tstates2;

	while( 1 ) {

		if( fscanf( f, "%x", &address ) != 1 ) {
			fprintf( stderr, "%s: no address found in file\n", progname);
			return 1;
		}

		if( address >= 0x10000 ) break;

		while( 1 ) {

			unsigned byte;

			if( fscanf( f, "%x", &byte ) != 1 ) {
	fprintf( stderr, "%s: no data byte found in file\n", progname);
	return 1;
			}
		
			if( byte >= 0x100 ) break;

			memory[ address++ ] = byte;

		}
	}

	printf( "%s", test_name );

	return 0;
}

static void
dump_z80_state( z80 *cpu, unsigned int tstates )
{
	printf( "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
		*AF, *BC, *DE, *HL, *AF_, *BC_, *DE_, *HL_, *Ix, *Iy, *SP, *PC );
	printf( "%02x %02x %d %d %d %d %d\n", *Intvec, *Refresh,
		cpu->IFF[0], cpu->IFF[1], cpu->intmode, cpu->halt, tstates );
}

static void
dump_memory_state( unsigned char *memory, unsigned char *initial_memory )
{
	size_t i;

	for( i = 0; i < 0x10000; i++ ) {

		if( memory[ i ] == initial_memory[ i ] ) continue;

		printf( "%04x ", (unsigned)i );

		while( i < 0x10000 && memory[ i ] != initial_memory[ i ] )
			printf( "%02x ", memory[ i++ ] );

		printf( "-1\n" );
	}
}

static int
run_test(FILE *f)
{
	size_t i;
	unsigned int tstates=0;
	unsigned char memory[0x10000], initial_memory[0x10000];
	ramtop=0x10000;
	for( i = 0; i < 0x10000; i += 4 ) {
		memory[ i     ] = 0xde; memory[ i + 1 ] = 0xad;
		memory[ i + 2 ] = 0xbe; memory[ i + 3 ] = 0xef;
	}

	z80 _cpu, *cpu=&_cpu;
	bus_t _bus, *bus=&_bus;
	z80_reset(cpu, bus);
	unsigned int end_tstates;
	if( read_test( f, &end_tstates, cpu, memory ) ) return 0;

	/* Grab a copy of the memory for comparison at the end */
	memcpy( initial_memory, memory, 0x10000 );

	int errupt=0;
	while(!errupt)
	{
		do_ram(memory, bus, true);
		fflush(stdout);
		if(cpu->nothing)
		{
			cpu->nothing--;
			if(cpu->steps)
				cpu->steps--;
			cpu->dT++;
		}
		else
			errupt=z80_tstep(cpu, bus, errupt);
		fflush(stderr);
		if(++tstates>=end_tstates)
		{
			if((cpu->M==0)&&(cpu->dT==0)&&!cpu->block_ints)
				errupt++;
		}
	}

	/* And dump our final state */
	dump_z80_state(cpu, tstates);
	dump_memory_state(memory, initial_memory);

	printf( "\n" );

	return 1;
}
#endif
