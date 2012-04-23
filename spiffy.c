/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
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
#include "pbm.h"

// SDL surface params
#define OSIZ_X	320
#define OSIZ_Y	360
#define OBPP	32

#define AUDIO		// Activates audio

#ifdef AUDIO
#define MAX_STRIS_INC_RATE	32
#define SAMPLE_RATE		8000 // Audio sample rate, Hz
#define AUDIOBUFLEN		(SAMPLE_RATE/100)
#define MAX_STRIS_INCBUFLEN	(AUDIOBUFLEN*MAX_STRIS_INC_RATE)
unsigned char sinc_rate=12;
#define STRIS_INCBUFLEN		(AUDIOBUFLEN*sinc_rate)
void update_sinc(unsigned char filterfactor);
#endif /* AUDIO */

#define ROM_FILE "48.rom" // Spectrum ROM file (TODO: make configable)

#define GPL_MSG "spiffy Copyright (C) 2010-12 Edward Cree.\n\
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
void uparrow(SDL_Surface * screen, SDL_Rect where, unsigned long col, unsigned long bcol);
void downarrow(SDL_Surface * screen, SDL_Rect where, unsigned long col, unsigned long bcol);
#ifdef AUDIO
void mixaudio(void *abuf, Uint8 *stream, int len);
typedef struct
{
	bool bits[MAX_STRIS_INCBUFLEN];
	bool cbuf[MAX_STRIS_INCBUFLEN];
	unsigned int rp, wp; // read & write pointers for 'bits' circular buffer
	bool play; // true if tape is playing (we mute and allow skipping)
	FILE *record;
}
audiobuf;

double sincgroups[MAX_STRIS_INC_RATE][AUDIOBUFLEN];
#endif /* AUDIO */

typedef struct
{
	bool memwait;
	bool iowait;
	bool t1;
}
ula_t;

typedef struct
{
	SDL_Rect posn;
	SDL_Surface *img;
	uint32_t col;
}
button;

// helper fns
void show_state(const unsigned char * RAM, const z80 *cpu, int Tstates, const bus_t *bus);
void scrn_update(SDL_Surface *screen, int Tstates, int frames, int frameskip, int Fstate, const unsigned char *RAM, bus_t *bus, ula_t *ula);
int dtext(SDL_Surface * scrn, int x, int y, int w, const char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b);
bool pos_rect(pos p, SDL_Rect r);
void getedge(libspectrum_tape *deck, bool *play, bool stopper, bool *ear, uint32_t *T_to_tape_edge, int *edgeflags, int *oldtapeblock, unsigned int *tapeblocklen);
void drawbutton(SDL_Surface *screen, button b);
void loadfile(const char *fn, libspectrum_tape **deck);
#define peek16(a)	(RAM[(a)]|(RAM[(a)+1]<<8))
#define poke16(a,v)	(RAM[(a)]=(v),RAM[(a)+1]=((v)>>8))
double float_decode(const unsigned char *RAM, unsigned int addr);
void float_encode(unsigned char *RAM, unsigned int addr, double val);

#ifdef CORETEST
static int read_test( FILE *f, unsigned int *end_tstates, z80 *cpu, unsigned char *memory);
static void dump_z80_state( z80 *cpu, unsigned int tstates );
static void dump_memory_state( unsigned char *memory, unsigned char *initial_memory );
static int run_test( FILE *f );
#endif /* CORETEST */

int main(int argc, char * argv[])
{
	TTF_Font *font=NULL;
	if(!TTF_Init())
	{
		font=TTF_OpenFont(PREFIX"/share/fonts/Vera.ttf", 12);
		if(!font) font=TTF_OpenFont("Vera.ttf", 12);
	}
	bool debug=false; // Generate debugging info?
	bool bugstep=false; // Single-step?
	bool debugcycle=false; // Single-Tstate stepping?
	bool trace=false; // execution tracing in debugger?
	#ifdef CORETEST
	bool coretest=false; // run the core tests?
	#endif /* CORETEST */
	bool pause=false;
	bool stopper=false; // stop tape at end of this block?
	bool edgeload=true; // edge loader enabled
	#ifdef AUDIO
	bool delay=true; // attempt to maintain approximately a true Speccy speed, 50fps at 69888 T-states per frame, which is 3.4944MHz
	unsigned char filterfactor=51; // this value minimises noise with various beeper engines (dunno why).  Other good values are 38, 76
	update_sinc(filterfactor);
	#endif /* AUDIO */
	const char *fn=NULL;
	unsigned int nbreaks=0;
	unsigned int *breakpoints=NULL;
	int arg;
	for (arg=1; arg<argc; arg++)
	{
		if((strcmp(argv[arg], "--debug") == 0) || (strcmp(argv[arg], "-d") == 0))
		{ // activate debugging mode
			debug=true;
		}
		else if(strncmp(argv[arg], "-b=", 3) == 0)
		{ // activate debugging mode at a breakpoint
			unsigned int breakpoint;
			if(sscanf(argv[arg]+3, "%04x", &breakpoint)==1)
			{
				unsigned int n=nbreaks++;
				unsigned int *nbp=realloc(breakpoints, nbreaks*sizeof(unsigned int));
				if(!nbp)
				{
					perror("malloc");
					return(1);
				}
				(breakpoints=nbp)[n]=breakpoint;
			}
			else
			{
				fprintf(stderr, "Ignoring bad argument '%s'\n", argv[arg]);
			}
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
		#endif /* CORETEST */
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
	#endif /* CORETEST */
	
	// State
	z80 _cpu, *cpu=&_cpu; // we want to work with a pointer
	bus_t _bus, *bus=&_bus;
	ula_t _ula, *ula=&_ula;
	
	SDL_Surface * screen=gf_init();
	if(!screen)
	{
		fprintf(stderr, "Failed to set up video\n");
		return(2);
	}
	SDL_WM_SetCaption("Spiffy - ZX Spectrum 48k", "Spiffy");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_Event event;
	line(screen, 0, 296, OSIZ_X-1, 296, 255, 255, 255);
	SDL_Rect cls={0, 297, OSIZ_X, OSIZ_Y-297};
	SDL_FillRect(screen, &cls, SDL_MapRGB(screen->format, 0, 0, 0));
	FILE *fimg;
	string img;
	fimg=configopen("buttons/load.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button loadbutton={.img=pbm_string(img), .posn={124, 298, 17, 17}, .col=0x8f8f1f};
	drawbutton(screen, loadbutton);
	free_string(&img);
	fimg=configopen("buttons/flash.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button edgebutton={.img=pbm_string(img), .posn={144, 298, 17, 17}, .col=edgeload?0xffffff:0x1f1f1f};
	drawbutton(screen, edgebutton);
	free_string(&img);
	fimg=configopen("buttons/play.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button playbutton={.img=pbm_string(img), .posn={164, 298, 17, 17}, .col=0x3fbf5f};
	drawbutton(screen, playbutton);
	free_string(&img);
	fimg=configopen("buttons/next.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button nextbutton={.img=pbm_string(img), .posn={184, 298, 17, 17}, .col=0x07079f};
	drawbutton(screen, nextbutton);
	free_string(&img);
	fimg=configopen("buttons/stop.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button stopbutton={.img=pbm_string(img), .posn={204, 298, 17, 17}, .col=0x3f0707};
	drawbutton(screen, stopbutton);
	free_string(&img);
	fimg=configopen("buttons/rewind.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button rewindbutton={.img=pbm_string(img), .posn={224, 298, 17, 17}, .col=0x7f076f};
	drawbutton(screen, rewindbutton);
	free_string(&img);
	fimg=configopen("buttons/pause.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button pausebutton={.img=pbm_string(img), .posn={8, 340, 17, 17}, .col=pause?0xbf6f07:0x7f6f07};
	drawbutton(screen, pausebutton);
	free_string(&img);
	fimg=configopen("buttons/reset.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button resetbutton={.img=pbm_string(img), .posn={28, 340, 17, 17}, .col=0xffaf07};
	drawbutton(screen, resetbutton);
	free_string(&img);
	fimg=configopen("buttons/bug.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button bugbutton={.img=pbm_string(img), .posn={48, 340, 17, 17}, .col=0xbfff3f};
	drawbutton(screen, bugbutton);
	free_string(&img);
#ifdef AUDIO
	SDL_Rect aw_up={76, 321, 7, 6}, aw_down={76, 328, 7, 6};
	SDL_Rect sr_up={140, 321, 7, 6}, sr_down={140, 328, 7, 6};
	fimg=configopen("buttons/record.pbm", "rb");
	img=sslurp(fimg);
	if(fimg) fclose(fimg);
	button recordbutton={.img=pbm_string(img), .posn={8, 320, 17, 17}, .col=0x7f0707};
	drawbutton(screen, recordbutton);
	free_string(&img);
#endif /* AUDIO */
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
	if(SDL_InitSubSystem(SDL_INIT_AUDIO))
	{
		fprintf(stderr, "spiffy: failed to initialise audio subsystem:\tSDL_InitSubSystem:%s\n", SDL_GetError());
		return(3);
	}
	SDL_AudioSpec fmt;
	fmt.freq = SAMPLE_RATE;
	fmt.format = AUDIO_S16;
	fmt.channels = 1;
	fmt.samples = AUDIOBUFLEN;
	fmt.callback = mixaudio;
	audiobuf abuf = {.rp=0, .wp=0, .record=NULL};
	fmt.userdata = &abuf;

	/* Open the audio device */
	if ( SDL_OpenAudio(&fmt, NULL) < 0 ) {
		fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
		return(3);
	}
#endif /* AUDIO */

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
	bus_reset(bus);
	
	libspectrum_tape *deck=NULL;
	bool play=false;
	int oldtapeblock=-1;
	unsigned int tapeblocklen=0;
	
	if(ls&&fn)
	{
		loadfile(fn, &deck);
	}
	
	SDL_Flip(screen);
#ifdef AUDIO
	// Start sound
	SDL_PauseAudio(0);
#endif /* AUDIO */
	
	int frames=0;
	int Tstates=0;
	uint32_t T_to_tape_edge=0;
	int edgeflags=0;
	
	// Main program loop
	while(likely(!errupt))
	{
		if(unlikely(nbreaks))
		{
			for(unsigned int bp=0;bp<nbreaks;bp++)
			{
				if((!debug)&&(*PC==breakpoints[bp])&&(cpu->M==0)&&(cpu->dT==0)&&(cpu->shiftstate==0))
				{
					debug=true;
				}
			}
		}
		Tstates++;
		#ifdef AUDIO
		if(!(Tstates%(69888*50/(SAMPLE_RATE*sinc_rate))))
		{
			abuf.play=play;
			unsigned int newwp=(abuf.wp+1)%STRIS_INCBUFLEN;
			if(delay&&!play)
				while(newwp==abuf.rp) usleep(5e3);
			abuf.bits[abuf.wp]=(bus->portfe&0x10);
			abuf.bits[abuf.wp]^=ear;
			abuf.wp=newwp;
		}
		#endif /* AUDIO */
		if(likely(play&&!pause))
		{
			if(unlikely(!deck))
				play=false;
			else if(T_to_tape_edge)
				T_to_tape_edge--;
			else
			{
				getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
			}
		}
		if(bus->mreq)
			do_ram(RAM, bus, false);
		
		if(unlikely(bus->iorq&&(bus->tris==TRIS_OUT)))
		{
			if(!(bus->addr&0x01))
				bus->portfe=bus->data;
		}
		
		if(unlikely(bus->iorq&&(bus->tris==TRIS_IN)))
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
			if(bugstep)
			{
				if(trace)
					show_state(RAM, cpu, Tstates, bus);
				int derrupt=0;
				while(!derrupt)
				{
					fprintf(stderr, ">");
					fflush(stderr);
					char *line=finpl(stdin);
					if(line)
					{
						char *cmd=strtok(line, " ");
						if(!cmd)
						{
							debug=false;
							derrupt++;
						}
						else if((strcmp(cmd, "c")==0)||(strcmp(cmd, "cont")==0))
						{
							debug=false;
							derrupt++;
						}
						else if((strcmp(cmd, "h")==0)||(strcmp(cmd, "help")==0))
						{
							const char *what=strtok(NULL, " ");
							if(!what) what="";
							if(strcmp(what, "m")==0)
								fprintf(stderr, "spiffy debugger: memory commands\n\
m r xxxx       read memory at address xxxx (hex)\n\
m w xxxx [yy]  write value yy (hex) or 0 to address xxxx (hex)\n\
m fr xxxx      read 5-byte float from address xxxx (hex)\n\
m fw xxxx d    write 5-byte float d (decimal) to address xxxx (hex)\n\
");
							else if(strcmp(what, "v")==0)
								fprintf(stderr, "spiffy debugger: BASIC variables\n\
v              list all variables\n\
v foo          examine (numeric) foo\n\
v a$           examine (string) a$\n\
v a(1,2,3)     examine numeric array a\n\
v a$(4)        examine character array a$\n\
    If the variable is numeric, the listed address is that of the 5-byte float\n\
    value; if string, the listed address is the start of the variable's\n\
    metadata (the string data starts 3 bytes later).  If it's an array, the\n\
    address of the first element of the array is given (that is, the address\n\
    when all the remaining subscripts are filled out with 1s); this is the\n\
    case regardless of whether the array is numeric or character.\n\
");
							else if(strcmp(what, "=")==0)
								fprintf(stderr, "spiffy debugger: register assignments\n\
= reg val      assigns val (hex) to register reg\n\
\n\
The registers are: PC AF BC DE HL IX IY SP AF' BC' DE' HL'\n\
8-bit assignments can be made as follows:\n\
 MSB LSB register\n\
  A   F     AF\n\
  B   C     BC\n\
  D   E     DE\n\
  H   L     HL\n\
  X   x     IX\n\
  Y   y     IY\n\
  I   R     IR (can't assign as 16-bit)\n\
  S   P     SP\n\
  a   f     AF'\n\
  b   c     BC'\n\
  d   e     DE'\n\
  h   l     HL'\n\
");
							else
								fprintf(stderr, "spiffy debugger:\n\
n[ext]         single-step the Z80\n\
c[ont]         continue emulation\n\
h[elp]         this help here\n\
s[tate]        show Z80 state\n\
t[race]        trace Z80 state\n\
b[reak] xxxx   set a breakpoint\n\
!b[reak] xxxx  delete a breakpoint\n\
l[ist]         list breakpoints\n\
= reg val      assign a value to a register (see 'h =')\n\
m[emory]       read/write memory (see 'h m')\n\
ei             enable interrupts\n\
di             disable interrupts\n\
r[eset]        reset the Z80\n\
[!]i[nt]       set/clear TRIS_INT line\n\
[!]nmi         set/clear NMI line\n\
v[ars]         examine BASIC variables (see 'h v')\n\
q[uit]         quit Spiffy\n");
						}
						else if((strcmp(cmd, "s")==0)||(strcmp(cmd, "state")==0))
							show_state(RAM, cpu, Tstates, bus);
						else if((strcmp(cmd, "t")==0)||(strcmp(cmd, "trace")==0))
							trace=true;
						else if((strcmp(cmd, "!t")==0)||(strcmp(cmd, "!trace")==0))
							trace=false;
						else if((strcmp(cmd, "n")==0)||(strcmp(cmd, "next")==0))
							derrupt++;
						else if((strcmp(cmd, "=")==0)||(strcmp(cmd, "assign")==0))
						{
							char *what=strtok(NULL, " ");
							if(what)
							{
								char *rest=strtok(NULL, "");
								int reg=-1;
								bool is16=false;
								if(strcasecmp(what, "PC")==0)
								{
									reg=0;
									is16=true;
								}
								else if(strlen(what)==1)
								{
									const char *reglist="AFBCDEHLXxYyIRSPafbcdehl";
									const char *p=strchr(reglist, *what);
									if(p)
										reg=(p+2-reglist)^1;
									is16=false;
								}
								else if(strcasecmp(what, "AF")==0)
								{
									reg=2;
									is16=true;
								}
								else if(strcasecmp(what, "BC")==0)
								{
									reg=4;
									is16=true;
								}
								else if(strcasecmp(what, "DE")==0)
								{
									reg=6;
									is16=true;
								}
								else if(strcasecmp(what, "HL")==0)
								{
									reg=8;
									is16=true;
								}
								else if(strcasecmp(what, "IX")==0)
								{
									reg=10;
									is16=true;
								}
								else if(strcasecmp(what, "IY")==0)
								{
									reg=12;
									is16=true;
								}
								else if(strcasecmp(what, "SP")==0)
								{
									reg=16;
									is16=true;
								}
								else if(strcasecmp(what, "AF'")==0)
								{
									reg=18;
									is16=true;
								}
								else if(strcasecmp(what, "BC'")==0)
								{
									reg=20;
									is16=true;
								}
								else if(strcasecmp(what, "DE'")==0)
								{
									reg=22;
									is16=true;
								}
								else if(strcasecmp(what, "HL'")==0)
								{
									reg=24;
									is16=true;
								}
								if(reg>=0)
								{
									unsigned int val;
									if(sscanf(rest, "%x", &val)==1)
									{
										cpu->regs[reg]=val;
										if(is16)
											cpu->regs[reg+1]=val>>8;
									}
									else
									{
										fprintf(stderr, "set: missing value\n");
									}
								}
								else
								{
									fprintf(stderr, "No such register %s\n", what);
								}
							}
						}
						else if((strcmp(cmd, "m")==0)||(strcmp(cmd, "memory")==0))
						{
							char *what=strtok(NULL, " ");
							if(what)
							{
								if(*what=='r')
								{
									char *rest=strtok(NULL, "");
									if(rest)
									{
										unsigned int addr;
										if(sscanf(rest, "%x", &addr)==1)
										{
											fprintf(stderr, "[%04x]=%02x\n", addr, RAM[addr]);
										}
										else
											fprintf(stderr, "memory: missing address\n");
									}
									else
										fprintf(stderr, "memory: missing address\n");
								}
								else if(*what=='w')
								{
									char *a=strtok(NULL, " ");
									if(a)
									{
										char *rest=strtok(NULL, "");
										unsigned int addr;
										if(sscanf(a, "%x", &addr)==1)
										{
											unsigned int val;
											if(!(rest&&(sscanf(rest, "%x", &val)==1)))
												val=0;
											RAM[addr]=val;
										}
										else
											fprintf(stderr, "memory: missing address\n");
									}
								}
								else if(strcmp(what, "fr")==0)
								{
									char *rest=strtok(NULL, "");
									if(rest)
									{
										unsigned int addr;
										if(sscanf(rest, "%x", &addr)==1)
										{
											fprintf(stderr, "[%04x.f]=%g\n", addr, float_decode(RAM, addr));
										}
										else
											fprintf(stderr, "memory: missing address\n");
									}
									else
										fprintf(stderr, "memory: missing address\n");
								}
								else if(strcmp(what, "fw")==0)
								{
									char *a=strtok(NULL, " ");
									if(a)
									{
										char *rest=strtok(NULL, "");
										unsigned int addr;
										if(sscanf(a, "%x", &addr)==1)
										{
											double val;
											if(!(rest&&(sscanf(rest, "%lg", &val)==1)))
												fprintf(stderr, "memory: missing value\n");
											else
												float_encode(RAM, addr, val);
										}
										else
											fprintf(stderr, "memory: missing address\n");
									}
								}
								else
									fprintf(stderr, "memory: bad mode (see 'h m')\n");
							}
							else
								fprintf(stderr, "memory: missing mode (see 'h m')\n");
						}
						else if((strcmp(cmd, "b")==0)||(strcmp(cmd, "break")==0))
						{
							char *rest=strtok(NULL, "");
							unsigned int bp=0;
							if(rest&&(sscanf(rest, "%x", &bp)==1))
							{
								unsigned int n=nbreaks++;
								unsigned int *nbp=realloc(breakpoints, nbreaks*sizeof(unsigned int));
								if(!nbp)
								{
									perror("malloc");
								}
								else
								{
									(breakpoints=nbp)[n]=bp;
									fprintf(stderr, "breakpoint at %04x\n", bp);
								}
							}
							else
							{
								fprintf(stderr, "break: missing argument\n");
							}
						}
						else if((strcmp(cmd, "!b")==0)||(strcmp(cmd, "!break")==0))
						{
							char *rest=strtok(NULL, "");
							unsigned int bp=0;
							if(rest&&(sscanf(rest, "%x", &bp)==1))
							{
								for(unsigned int i=0;i<nbreaks;i++)
								{
									while(breakpoints[i]==bp)
									{
										fprintf(stderr, "deleted breakpoint at %04x\n", breakpoints[i]);
										if(i<--nbreaks)
											breakpoints[i]=breakpoints[i+1];
										else break;
									}
								}
							}
							else
							{
								for(unsigned int i=0;i<nbreaks;i++)
									fprintf(stderr, "deleted breakpoint at %04x\n", breakpoints[i]);
								free(breakpoints);
								breakpoints=NULL;
								nbreaks=0;
							}
						}
						else if((strcmp(cmd, "l")==0)||(strcmp(cmd, "list")==0))
						{
							for(unsigned int i=0;i<nbreaks;i++)
							{
								fprintf(stderr, "breakpoint at %04x\n", breakpoints[i]);
							}
						}
						else if(strcmp(cmd, "ei")==0)
							cpu->IFF[0]=cpu->IFF[1]=1;
						else if(strcmp(cmd, "di")==0)
							cpu->IFF[0]=cpu->IFF[1]=0;
						else if((strcmp(cmd, "r")==0)||(strcmp(cmd, "reset")==0))
							bus_reset(bus);
						else if((strcmp(cmd, "i")==0)||(strcmp(cmd, "int")==0))
							bus->irq=true;
						else if((strcmp(cmd, "!i")==0)||(strcmp(cmd, "!int")==0))
							bus->irq=false;
						else if(strcmp(cmd, "nmi")==0)
							bus->nmi=true;
						else if(strcmp(cmd, "!nmi")==0)
							bus->nmi=false;
						else if((strcmp(cmd, "v")==0)||(strcmp(cmd, "vars")==0))
						{
							unsigned short int sv_vars=peek16(23627), i=sv_vars, l;
							char *what=strtok(NULL, ""), *rest=NULL;
							unsigned int wlen=0;
							if(what)
							{
								rest=strchr(what, '(');
								if(rest)
								{
									char *nrest=strdup(rest);
									*rest=0;
									rest=nrest;
								}
								wlen=strlen(what);
								while(wlen&&isspace(what[wlen-1]))
									what[--wlen]=0;
							}
							double num;
							bool match=!what;
							while(i&&(RAM[i]!=0x80))
							{
								unsigned char name=(RAM[i]&0x1f)|0x60;
								unsigned short int addr=i;
								switch(RAM[i]>>5)
								{
									case 2: // String
										// 010aaaaa Length[2] Text[]
										if(what) match=((wlen==2)&&(what[0]==name)&&(what[1]=='$'));
										i++;
										l=peek16(i);
										i+=2;
										if(match)
										{
											if(rest)
											{
												unsigned int j;
												if(sscanf(rest, "(%u)", &j)!=1)
													fprintf(stderr, "3 Subscript wrong, 0:1\n");
												else if((j<1)||(j>l))
													fprintf(stderr, "3 Subscript wrong, 0:1\n");
												else
												{
													unsigned char c=RAM[i+j-1];
													fprintf(stderr, "%04x $ %c$(%u) = ", i+j-1, name, j);
													if((c>=32)&&(c<127))
														fprintf(stderr, "'%c' = ", c);
													fprintf(stderr, "%u = 0x%02x = '\\%03o'\n", c, c, c);
												}
											}
											else
											{
												fprintf(stderr, "%04x $ %c$ = \"", addr, name);
												unsigned int k=0;
												bool overlong=false;
												for(unsigned int j=0;j<l;j++)
												{
													if(k>=64)
													{
														overlong=true;
														break;
													}
													unsigned char c=RAM[i+j];
													if((c>=32)&&(c<127))
													{
														fputc(c, stderr);
														k++;
													}
													else
													{
														int len;
														fprintf(stderr, "\\%03o%n", c, &len);
														k+=len;
													}
												}
												fprintf(stderr, overlong?"\"...\n":"\"\n");
											}
										}
										i+=l;
									break;
									case 3: // Number whose name is one letter
										// 011aaaaa Value[5]
										if(what) match=((wlen==1)&&(*what==name));
										if(rest) match=false;
										if(match)
										{
											fprintf(stderr, "%04x # ", addr+1);
											fputc(name, stderr);
										}
										num=float_decode(RAM, ++i);
										if(match) fprintf(stderr, " = %g\n", num);
										i+=5;
									break;
									case 4: // Array of numbers
										// 100aaaaa TotalLength[2] DimensionsAndValues[]
										if(what) match=((wlen==1)&&(*what==name)&&rest);
										if(!what) fprintf(stderr, "%04x # %c (array)\n", addr, name);
										i++;
										l=peek16(i);
										i+=2;
										if(what&&match)
										{
											unsigned char ndim=RAM[i], sub=0;
											unsigned short int dims[ndim];
											for(unsigned char dim=0;dim<ndim;dim++)
												dims[dim]=peek16(i+1+dim*2);
											unsigned int subs[ndim];
											const char *p=rest;
											while(p&&*p&&(sub<ndim))
											{
												if(strchr("(,) \t", *p)) { p++; continue; }
												int l;
												if(sscanf(p, "%u%n", subs+sub, &l)!=1)
													break;
												p+=l;
												if(subs[sub]<1)
												{
													fprintf(stderr, "3 Subscript wrong, 0:%u\n", sub+1);
													break;
												}
												if(subs[sub]>dims[sub])
												{
													fprintf(stderr, "3 Subscript wrong, 0:%u\n", sub+1);
													break;
												}
												sub++;
											}
											addr=i+1+ndim*2;
											if(sub<ndim)
											{
												for(unsigned char dim=0;dim<sub;dim++)
												{
													unsigned short int offset=(subs[dim]-1)*5;
													for(unsigned char d2=dim+1;d2<ndim;d2++)
														offset*=dims[d2];
													addr+=offset;
												}
												fprintf(stderr, "%04x # %c", addr, name);
												for(unsigned char dim=0;dim<sub;dim++)
												{
													fprintf(stderr, "(%u)", subs[dim]);
												}
												fprintf(stderr, " (array");
												for(unsigned char dim=sub;dim<ndim;dim++)
													fprintf(stderr, "[%u]", dims[dim]);
												fprintf(stderr, ")\n");
											}
											else
											{
												for(unsigned char dim=0;dim<ndim;dim++)
												{
													unsigned short int offset=(subs[dim]-1)*5;
													for(unsigned char d2=dim+1;d2<ndim;d2++)
														offset*=dims[d2];
													addr+=offset;
												}
												fprintf(stderr, "%04x # %c", addr, name);
												for(unsigned char dim=0;dim<sub;dim++)
												{
													fprintf(stderr, "(%u)", subs[dim]);
												}
												fprintf(stderr, " = %g\n", float_decode(RAM, addr));
											}
										}
										i+=l;
									break;
									case 5:; // Number whose name is longer than one letter
										// 101aaaaa 0bbbbbbb ... 1zzzzzzz Value[5]
										string fullname=init_string();
										append_char(&fullname, name);
										i++;
										do append_char(&fullname, RAM[i]&0x7F);
										while(!(RAM[i++]&0x80));
										if(what) match=strcmp(fullname.buf, what);
										num=float_decode(RAM, i);
										if(match) fprintf(stderr, "%04x # %s = %g\n", i, fullname.buf, num);
										free_string(&fullname);
										i+=5;
									break;
									case 6: // Array of characters
										// 110aaaaa TotalLength[2] DimensionsAndValues[]
										if(what) match=((wlen==2)&&(what[0]==name)&&(what[1]=='$'));
										if(!what) fprintf(stderr, "%04x $ %c$ (array)\n", addr, name);
										i++;
										l=peek16(i);
										i+=2;
										if(what&&match)
										{
											unsigned char ndim=RAM[i], sub=0;
											unsigned short int dims[ndim];
											for(unsigned char dim=0;dim<ndim;dim++)
												dims[dim]=peek16(i+1+dim*2);
											unsigned int subs[ndim];
											const char *p=rest;
											while(p&&*p&&(sub<ndim))
											{
												if(strchr("(,) \t", *p)) { p++; continue; }
												int l;
												if(sscanf(p, "%u%n", subs+sub, &l)!=1)
													break;
												p+=l;
												if(subs[sub]<1)
												{
													fprintf(stderr, "3 Subscript wrong, 0:%u\n", sub+1);
													break;
												}
												if(subs[sub]>dims[sub])
												{
													fprintf(stderr, "3 Subscript wrong, 0:%u\n", sub+1);
													break;
												}
												sub++;
											}
											addr=i+1+ndim*2;
											if(sub<ndim)
											{
												for(unsigned char dim=0;dim<sub;dim++)
												{
													unsigned short int offset=subs[dim]-1;
													for(unsigned char d2=dim+1;d2<ndim;d2++)
														offset*=dims[d2];
													addr+=offset;
												}
												fprintf(stderr, "%04x $ %c$", addr, name);
												for(unsigned char dim=0;dim<sub;dim++)
												{
													fprintf(stderr, "(%u)", subs[dim]);
												}
												fprintf(stderr, " (array");
												for(unsigned char dim=sub;dim<ndim;dim++)
													fprintf(stderr, "[%u]", dims[dim]);
												fputc(')', stderr);
												if(sub+1==ndim)
												{
													fprintf(stderr, " = \"");
													unsigned int k=0;
													bool overlong=false;
													for(unsigned int j=0;j<dims[ndim-1];j++)
													{
														if(k>=64)
														{
															overlong=true;
															break;
														}
														unsigned char c=RAM[addr+j];
														if((c>=32)&&(c<127))
														{
															fputc(c, stderr);
															k++;
														}
														else
														{
															int len;
															fprintf(stderr, "\\%03o%n", c, &len);
															k+=len;
														}
													}
													fputc('"', stderr);
													if(overlong) fprintf(stderr, "...");
												}
												fputc('\n', stderr);
											}
											else
											{
												for(unsigned char dim=0;dim<ndim;dim++)
												{
													unsigned short int offset=(subs[dim]-1);
													for(unsigned char d2=dim+1;d2<ndim;d2++)
														offset*=dims[d2];
													addr+=offset;
												}
												fprintf(stderr, "%04x $ %c$", addr, name);
												for(unsigned char dim=0;dim<sub;dim++)
													fprintf(stderr, "(%u)", subs[dim]);
												unsigned char c=RAM[addr];
												if((c>=32)&&(c<127))
													fprintf(stderr, " = '%c'", c);
												fprintf(stderr, " = %u = 0x%02x = '\\%03o'\n", c, c, c);
											}
										}
										i+=l;
									break;
									case 7: // Control variable of a FOR-NEXT loop
										// 111aaaaa Value[5] Limit[5] Step[5] LoopingLine[2] StmtNumber[1]
										if(what) match=((wlen==1)&&(*what==name));
										num=float_decode(RAM, ++i);
										i+=5;
										double limit=float_decode(RAM, i);
										i+=5;
										double step=float_decode(RAM, i);
										i+=5;
										unsigned short int loop=peek16(i);
										i+=2;
										unsigned char stmt=RAM[i++];
										if(match) fprintf(stderr, "%04x # %c = %g (<%g %+g @%u:%u)\n", addr+1, name, num, limit, step, loop, stmt);
									break;
									default:
										fprintf(stderr, "Error - unrecognised var type %d\n", RAM[i]>>5);
										i=0;
									break;
								}
								if(what&&match) break;
							}
							free(rest);
							if(what&&!match) fprintf(stderr, "No such variable '%s'\n", what);
						}
						else if((strcmp(cmd, "q")==0)||(strcmp(cmd, "quit")==0))
						{
							errupt++;
							derrupt++;
						}
						else
						{
							fprintf(stderr, "Unrecognised command '%s'.  Type 'h' for help\n", cmd);
						}
					}
					free(line);
				}
			}
			else
			{
				show_state(RAM, cpu, Tstates, bus);
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
		else if(!pause)
		{
			errupt=z80_tstep(cpu, bus, errupt);
			if(unlikely(play&&(*PC==0x05e7)&&(edgeload))) // Magic edge-loader (hard-coded implementation of LD-EDGE-1)
			{
				unsigned int wait=358;
				while((T_to_tape_edge<wait)&&play)
				{
					Tstates+=T_to_tape_edge;
					wait-=T_to_tape_edge;
					getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
				}
				if(play) T_to_tape_edge-=wait;
				while(1)
				{
					cpu->regs[5]++;
					wait=4;
					while((T_to_tape_edge<wait)&&play)
					{
						Tstates+=T_to_tape_edge;
						wait-=T_to_tape_edge;
						getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
					}
					if(play) T_to_tape_edge-=wait;
					cpu->regs[2]&=~FC;
					if(!cpu->regs[5])
					{
						cpu->regs[2]|=FZ;
						wait=11;
						while((T_to_tape_edge<wait)&&play)
						{
							Tstates+=T_to_tape_edge;
							wait-=T_to_tape_edge;
							getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
						}
						if(play) T_to_tape_edge-=wait;
						*PC=RAM[(*SP)++];
						*PC|=RAM[(*SP)++]<<8;
						break;
					}
					else
					{
						cpu->regs[2]&=~FZ;
						wait=22; // 5 + 7 + 10
						while((T_to_tape_edge<wait)&&play)
						{
							Tstates+=T_to_tape_edge;
							wait-=T_to_tape_edge;
							getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
						}
						if(play) T_to_tape_edge-=wait;
						unsigned char hi=0x7f;
						unsigned char data=(ear?0x40:0)|0x1f;
						for(int i=0;i<8;i++)
							if(!(hi&(1<<i)))
								data&=~kenc[i];
						cpu->regs[3]=(data>>1)|((cpu->regs[2]&FC)?0x80:0);
						cpu->regs[2]&=~FC;
						if(data&1) cpu->regs[2]|=FC;
						wait=10; // 1 + 4 + 5
						while((T_to_tape_edge<wait)&&play)
						{
							Tstates+=T_to_tape_edge;
							wait-=T_to_tape_edge;
							getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
						}
						if(play) T_to_tape_edge-=wait;
						if(!(cpu->regs[2]&FC))
						{
							wait=6;
							while((T_to_tape_edge<wait)&&play)
							{
								Tstates+=T_to_tape_edge;
								wait-=T_to_tape_edge;
								getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
							}
							if(play) T_to_tape_edge-=wait;
							*PC=RAM[(*SP)++];
							*PC|=RAM[(*SP)++]<<8;
							break;
						}
						else
						{
							cpu->regs[3]^=cpu->regs[4];
							cpu->regs[3]&=0x20;
							cpu->regs[2]&=~FZ;
							wait=18; // 4 + 7 + 7
							while((T_to_tape_edge<wait)&&play)
							{
								Tstates+=T_to_tape_edge;
								wait-=T_to_tape_edge;
								getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
							}
							if(play) T_to_tape_edge-=wait;if(!cpu->regs[3])
							{
								wait=5;
								while((T_to_tape_edge<wait)&&play)
								{
									Tstates+=T_to_tape_edge;
									wait-=T_to_tape_edge;
									getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
								}
								if(play) T_to_tape_edge-=wait;
								continue;
							}
							cpu->regs[3]=cpu->regs[4];
							cpu->regs[3]^=0xFF;
							cpu->regs[4]=cpu->regs[3];
							cpu->regs[3]&=7;
							cpu->regs[3]|=8;
							wait=36; // 4+4+4+7+7+10
							while((T_to_tape_edge<wait)&&play)
							{
								Tstates+=T_to_tape_edge;
								wait-=T_to_tape_edge;
								getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
							}
							if(play) T_to_tape_edge-=wait;
							bus->portfe=cpu->regs[3];
							cpu->regs[2]|=FC;
							*PC=RAM[(*SP)++];
							*PC|=RAM[(*SP)++]<<8;
							wait=15; // 1+4+10
							while((T_to_tape_edge<wait)&&play)
							{
								Tstates+=T_to_tape_edge;
								wait-=T_to_tape_edge;
								getedge(deck, &play, stopper, &ear, &T_to_tape_edge, &edgeflags, &oldtapeblock, &tapeblocklen);
							}
							if(play) T_to_tape_edge-=wait;
							break;
						}
					}
				}
			}
		}
		scrn_update(screen, Tstates, frames, play?7:0, Fstate, RAM, bus, ula);
		if(unlikely(Tstates==32))
			bus->irq=false;
		if(unlikely(Tstates>=69888))
		{
			bus->irq=true;
			bus->reset=false;
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
				dtext(screen, 8, 298, 92, text, font, 255, 255, 0);
				playbutton.col=play?0xbf1f3f:0x3fbf5f;
				drawbutton(screen, playbutton);
			}
			if(play&&!pause)
				tapeblocklen=max(tapeblocklen, 1)-1;
			if(!(frames%25))
			{
				char text[32];
				int tapen=0;
				if(deck)
				{
					libspectrum_tape_position(&tapen, deck);
					snprintf(text, 32, "T%03u [%u]", (tapeblocklen+49)/50, tapen);
				}
				else
				{
					snprintf(text, 32, "T--- [-]");
				}
				dtext(screen, 244, 298, 76, text, font, 0xbf, 0xbf, 0xbf);
				#ifdef AUDIO
				snprintf(text, 32, "BW:%03u", filterfactor);
				dtext(screen, 28, 320, 64, text, font, 0x9f, 0x9f, 0x9f);
				uparrow(screen, aw_up, 0xffdfff, 0x3f4f3f);
				downarrow(screen, aw_down, 0xdfffff, 0x4f3f3f);
				snprintf(text, 32, "SR:%03u", sinc_rate);
				dtext(screen, 92, 320, 64, text, font, 0x9f, 0x9f, 0x9f);
				uparrow(screen, sr_up, 0xffdfff, 0x3f4f3f);
				downarrow(screen, sr_down, 0xdfffff, 0x4f3f3f);
				#endif /* AUDIO */
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
							#endif /* AUDIO */
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
							if(key.sym==SDLK_RETURN)
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
								if(pos_rect(mouse, loadbutton.posn))
								{
									FILE *p=popen("spiffy-filechooser", "r");
									if(p)
									{
										char *fn=fgetl(p);
										fclose(p);
										loadfile(fn, &deck);
									}
								}
								else if(pos_rect(mouse, edgebutton.posn))
									edgeload=!edgeload;
								else if(pos_rect(mouse, playbutton.posn))
									play=!play;
								else if(pos_rect(mouse, nextbutton.posn))
								{
									if(deck) libspectrum_tape_select_next_block(deck);
								}
								else if(pos_rect(mouse, stopbutton.posn))
									stopper=!stopper;
								else if(pos_rect(mouse, rewindbutton.posn))
								{
									if(deck) libspectrum_tape_nth_block(deck, 0);
								}
								else if(pos_rect(mouse, pausebutton.posn))
									pause=!pause;
								else if(pos_rect(mouse, resetbutton.posn))
									bus_reset(bus);
								else if(pos_rect(mouse, bugbutton.posn))
								{
									debug=true;
									bugstep=true;
								}
								#ifdef AUDIO
								else if(pos_rect(mouse, aw_up))
								{
									filterfactor=min(filterfactor+1,0x80);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, aw_down))
								{
									filterfactor=max(filterfactor-1,1);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, sr_up))
								{
									sinc_rate=min(sinc_rate+1,MAX_STRIS_INC_RATE);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, sr_down))
								{
									sinc_rate=max(sinc_rate-1,1);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, recordbutton.posn))
								{
									if(abuf.record)
									{
										FILE *a=abuf.record;
										abuf.record=NULL;
										fclose(a);
									}
									else
									{
										FILE *a=fopen("record.wav", "wb");
										if(a)
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
										abuf.record=a;
									}
								}
								recordbutton.col=abuf.record?0xff0707:0x7f0707;
								drawbutton(screen, recordbutton);
								#endif /* AUDIO */
								edgebutton.col=edgeload?0xffffff:0x1f1f1f;
								playbutton.col=play?0xbf1f3f:0x3fbf5f;
								stopbutton.col=stopper?0x3f07f7:0x3f0707;
								pausebutton.col=pause?0xbf6f07:0x7f6f07;
								drawbutton(screen, edgebutton);
								drawbutton(screen, playbutton);
								drawbutton(screen, stopbutton);
								drawbutton(screen, pausebutton);
							break;
							case SDL_BUTTON_RIGHT:
								#ifdef AUDIO
								if(pos_rect(mouse, aw_up))
								{
									filterfactor=min(filterfactor<<1,0x80);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, aw_down))
								{
									filterfactor=max(filterfactor>>1,1);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, sr_up))
								{
									sinc_rate=min(sinc_rate<<1,MAX_STRIS_INC_RATE);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, sr_down))
								{
									sinc_rate=max(sinc_rate>>1,1);
									update_sinc(filterfactor);
								}
								#endif /* AUDIO */
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
	
	//show_state(RAM, cpu, Tstates, bus);
	SDL_PauseAudio(1);
	return(0);
}

SDL_Surface * gf_init()
{
	SDL_Surface * screen;
	if(SDL_Init(SDL_INIT_VIDEO)<0)
	{
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return(NULL);
	}
	if((screen = SDL_SetVideoMode(OSIZ_X, OSIZ_Y, OBPP, SDL_HWSURFACE))==0)
	{
		fprintf(stderr, "SDL_SetVideoMode: %s\n", SDL_GetError());
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

#ifdef AUDIO
void mixaudio(void *abuf, Uint8 *stream, int len)
{
	audiobuf *a=abuf;
	for(int i=0;i<len;i+=2)
	{
		for(unsigned int g=0;g<sinc_rate;g++)
		{
			while(!a->play&&(a->rp==a->wp)) usleep(5e3);
			a->cbuf[a->rp]=a->bits[a->rp];
			a->rp=(a->rp+1)%STRIS_INCBUFLEN;
		}
		unsigned int l=a->play?STRIS_INCBUFLEN>>2:STRIS_INCBUFLEN;
		double v=0;
		for(unsigned int j=0;j<l;j++)
		{
			signed char d=a->cbuf[(a->rp+STRIS_INCBUFLEN-j)%STRIS_INCBUFLEN]-a->cbuf[(a->rp+STRIS_INCBUFLEN-j-1)%STRIS_INCBUFLEN];
			if(d)
				v+=d*sincgroups[sinc_rate-(j%sinc_rate)-1][j/sinc_rate];
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
	double sinc[STRIS_INCBUFLEN];
	for(unsigned int i=0;i<STRIS_INCBUFLEN;i++)
	{
		double v=filterfactor*(i/(double)STRIS_INCBUFLEN-0.5);
		sinc[i]=(v?sin(v)/v:1)*16.0/(double)sinc_rate;
	}
	for(unsigned int g=0;g<sinc_rate;g++)
	{
		for(unsigned int j=0;j<AUDIOBUFLEN;j++)
			sincgroups[g][j]=0;
		for(unsigned int i=0;i<STRIS_INCBUFLEN;i++)
		{
			unsigned int j=(i+g)/sinc_rate;
			if(j<AUDIOBUFLEN)
				sincgroups[g][j]+=sinc[i];
		}
	}
}
#endif /* AUDIO */

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
	else if(cpu->intacc) printf(" TRIS_INT!");
	printf("\n");
	printf("Bus: A=%04x\tD=%02x\t%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%s\n", bus->addr, bus->data, bus->tris==TRIS_OUT?"WR":"wr", bus->tris==TRIS_IN?"RD":"rd", bus->mreq?"MREQ":"mreq", bus->iorq?"IORQ":"iorq", bus->m1?"M1":"m1", bus->rfsh?"RFSH":"rfsh", bus->waitline?"WAIT":"wait", bus->irq?"TRIS_INT":"int", bus->nmi?"NMI":"nmi", bus->reset?"RESET":"reset", bus->halt?"HALT":"halt");
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
#endif /* CORETEST */

void getedge(libspectrum_tape *deck, bool *play, bool stopper, bool *ear, uint32_t *T_to_tape_edge, int *edgeflags, int *oldtapeblock, unsigned int *tapeblocklen)
{
	int block;
	if(likely(!libspectrum_tape_position(&block, deck)))
	{
		if(unlikely(block!=*oldtapeblock))
		{
			*tapeblocklen=(libspectrum_tape_block_length(libspectrum_tape_current_block(deck))+69887)/69888;
			*oldtapeblock=block;
			if(stopper)
				*play=false;
		}
	}
	if(*edgeflags&LIBSPECTRUM_TAPE_FLAGS_STOP)
		*play=false;
	if(*edgeflags&LIBSPECTRUM_TAPE_FLAGS_STOP48)
		*play=false;
	if(!(*edgeflags&LIBSPECTRUM_TAPE_FLAGS_NO_EDGE))
		*ear=!*ear;
	if(*edgeflags&LIBSPECTRUM_TAPE_FLAGS_LEVEL_LOW)
		*ear=false;
	if(*edgeflags&LIBSPECTRUM_TAPE_FLAGS_LEVEL_HIGH)
		*ear=true;
	if(*edgeflags&LIBSPECTRUM_TAPE_FLAGS_TAPE)
	{
		*play=false;
		*T_to_tape_edge=0;
		*edgeflags=0;
	}
	if(*play)
		libspectrum_tape_get_next_edge(T_to_tape_edge, edgeflags, deck);
}

void drawbutton(SDL_Surface *screen, button b)
{
	SDL_FillRect(screen, &b.posn, SDL_MapRGB(screen->format, b.col>>16, b.col>>8, b.col));
	if(b.img) SDL_BlitSurface(b.img, NULL, screen, &b.posn);
}

void loadfile(const char *fn, libspectrum_tape **deck)
{
	FILE *fp=fopen(fn, "rb");
	if(!fp)
	{
		fprintf(stderr, "Failed to open '%s': %s\n", fn, strerror(errno));
	}
	else
	{
		string data=sslurp(fp);
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
						if(*deck) libspectrum_tape_free(*deck);
						if((*deck=libspectrum_tape_alloc()))
						{
							if(libspectrum_tape_read(*deck, (unsigned char *)data.buf, data.i, type, fn))
							{
								libspectrum_tape_free(*deck);
								free_string(&data);
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
					break;
				}
			}
		}
	}
}

double float_decode(const unsigned char *RAM, unsigned int addr)
{
	if(!RAM[addr])
	{
		if((!RAM[addr+1])||(RAM[addr+1]==0xFF))
		{
			if(!RAM[addr+4])
			{
				unsigned short int val=peek16(addr+2);
				if(RAM[addr+1]) return(val-131072);
				return(val);
			}
		}
	}
	signed char exponent=RAM[addr]-128;
	unsigned long mantissa=((RAM[addr+1]|0x80)<<24)|(RAM[addr+2]<<16)|(RAM[addr+3]<<8)|RAM[addr+4];
	bool minus=RAM[addr+1]&0x80;
	return((minus?-1.0:1.0)*mantissa*exp2(exponent-32));
}

void float_encode(unsigned char *RAM, unsigned int addr, double val)
{
	if((fabs(val)<65536)&&(ceil(val)==val))
	{
		RAM[addr]=RAM[addr+4]=0;
		signed int ival=ceil(val);
		if(signbit(val))
		{
			RAM[addr+1]=0xFF;
			ival+=131072;
		}
		else
			RAM[addr+1]=0;
		poke16(addr+2, (unsigned int)ival);
	}
	else if(isfinite(val))
	{
		signed char exponent=1+floor(log2(fabs(val)));
		double mantissa=rint(fabs(val)*exp2(32-exponent));
		if(mantissa>0xffffffff)
		{
			fprintf(stderr, "float_encode: 6 Number too big, %g\n", val);
			return;
		}
		unsigned long mi=mantissa;
		RAM[addr]=exponent+128;
		RAM[addr+1]=((mi>>24)&0x7F)|(signbit(val)?0x80:0);
		RAM[addr+2]=mi>>16;
		RAM[addr+3]=mi>>8;
		RAM[addr+4]=mi;
	}
	else
	{
		fprintf(stderr, "float_encode: cannot encode non-finite number %g\n", val);
	}
}
