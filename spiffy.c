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
#include <SDL.h>
#include <SDL/SDL_audio.h>
#include <SDL/SDL_ttf.h>
#include <time.h>
#include <errno.h>
#include "ops.h"
#include "z80.h"
#include "vchips.h"

// SDL surface params
#define OSIZ_X	320
#define OSIZ_Y	320
#define OBPP	32

//#define AUDIO		// Activates audio

#ifdef AUDIO
#define SAMPLE_RATE	8000 // Audio sample rate, Hz
#endif

#define ROM_FILE "48.rom" // Location of Spectrum ROM file (TODO: make configable)

#define VERSION_MAJ	0
#define VERSION_MIN	3
#define VERSION_REV	1

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
void pset(SDL_Surface * screen, int x, int y, char r, char g, char b);
int line(SDL_Surface * screen, int x1, int y1, int x2, int y2, char r, char g, char b);
#ifdef AUDIO
void mixaudio(void *portfe, Uint8 *stream, int len);
#endif

// helper fns
void show_state(unsigned char * RAM, z80 *cpu, int Tstates, bus_t *bus);
void scrn_update(SDL_Surface *screen, int Tstates, int Fstate, unsigned char RAM[65536], bus_t *bus);
int dtext(SDL_Surface * scrn, int x, int y, char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b);

#ifdef CORETEST
static int read_test( FILE *f, unsigned int *end_tstates, z80 *cpu, unsigned char *memory);
static void dump_z80_state( z80 *cpu, unsigned int tstates );
static void dump_memory_state( unsigned char *memory, unsigned char *initial_memory );
static int run_test( FILE *f );
#endif

unsigned char uladb,ulaab,uladbb,ulaabb;

int main(int argc, char * argv[])
{
	TTF_Font *font=NULL;
	if(!TTF_Init())
	{
		font=TTF_OpenFont("Vera.ttf", 12);
	}
	bool debug=false; // Generate debugging info?
	bool bugstep=false; // Single-step?
	#ifdef CORETEST
	bool coretest=false; // run the core tests?
	#endif
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
		#ifdef CORETEST
		else if((strcmp(argv[arg], "--coretest") == 0) || (strcmp(argv[arg], "-c") == 0))
		{ // run the core tests
			coretest=true;
		}
		#endif
		else
		{ // unrecognised option, assume it's a filename
			printf("Unrecognised option %s, exiting\n", argv[arg]);
			return(2);
		}
	}
	
	printf(GPL_MSG);
	
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
	z80 _cpu;
	z80 *cpu=&_cpu; // we want to work with a pointer
	bus_t _bus;
	bus_t *bus=&_bus;
	
	SDL_Surface * screen=gf_init();
	SDL_WM_SetCaption("Spiffy - ZX Spectrum 48k", "Spiffy");
	SDL_EnableUNICODE(1);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
	SDL_Event event;
	SDL_Rect cls;
	cls.x=0;
	cls.y=256;
	cls.w=OSIZ_X;
	cls.h=OSIZ_Y-256;
	int errupt = 0;
	bus->portfe=0; // used by mixaudio (for the EAR) and the screen update (for the BORDCR)
#ifdef AUDIO
	SDL_AudioSpec fmt;
	fmt.freq = SAMPLE_RATE;
	fmt.format = AUDIO_U8;
	fmt.channels = 1;
	fmt.samples = 64;
	fmt.callback = mixaudio;
	fmt.userdata = &bus->portfe;

	/* Open the audio device */
	if ( SDL_OpenAudio(&fmt, NULL) < 0 ) {
		fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
		return(3);
	}
#endif	

	bool delay=true; // attempt to maintain approximately a true Speccy speed, 50fps at 69888 T-states per frame, which is 3.4944MHz
	
	// Mouse handling
	pos mouse;
	SDL_GetMouseState(&mouse.x, &mouse.y);
	SDL_ShowCursor(SDL_DISABLE);
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
	bool reti=false; // was the last opcode RETI?	(some hardware detects this, eg. PIO)
	int Fstate=0; // FLASH state
	z80_reset(cpu, bus);
	
	SDL_Flip(screen);
	
#ifdef AUDIO
	// Start sound
	SDL_PauseAudio(0);
#endif
	
	time_t start_time=time(NULL);
	int frames=0;
	int Tstates=0;
	
	// Main program loop
	while(!errupt)
	{
		if((!debug)&&(*PC==breakpoint)&&bus->m1)
		{
			debug=true;
		}
		Tstates++;
		do_ram(RAM, bus, false);
		
		if(bus->iorq&&(bus->tris==OUT))
		{
			if(!(bus->addr&0x01))
				bus->portfe=bus->data;
		}
		
		if(debug&&(cpu->M==0)&&(cpu->dT==0)&&(cpu->shiftstate==0))
		{
			show_state(RAM, cpu, Tstates, bus);
			if(bugstep) getchar();
		}
		
		errupt=z80_tstep(cpu, bus, errupt);

		scrn_update(screen, Tstates, Fstate, RAM, bus);
		if(Tstates>=69888)
		{
			SDL_Flip(screen);
			Tstates-=69888;
			Fstate=(Fstate+1)%32; // flash alternates every 16 frames
			frames++;
			char text[32];
			double spd=(frames*2.0)/(double)(time(NULL)-start_time);
			sprintf(text, "Speed: %0.3g%%", spd);
			dtext(screen, 8, 296, text, font, 255, 255, 0);
			// TODO generate an interrupt
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
							if(key.sym==SDLK_q)
							{
								errupt++;
							}
							/*
							the ascii character is:
							if ((key.unicode & 0xFF80) == 0)
							{
								// it's (char)keysym.unicode & 0x7F;
							}
							else
							{
								// it's not [low] ASCII
							}
							*/
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
	
	// clean up
	if(SDL_MUSTLOCK(screen))
		SDL_UnlockSurface(screen);
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

void pset(SDL_Surface * screen, int x, int y, char r, char g, char b)
{
	long int s_off = (y*screen->pitch) + x*screen->format->BytesPerPixel;
	unsigned long int pixval = SDL_MapRGB(screen->format, r, g, b),
		* pixloc = (unsigned long int *)(((unsigned char *)screen->pixels)+s_off);
	*pixloc = pixval;
}

int line(SDL_Surface * screen, int x1, int y1, int x2, int y2, char r, char g, char b)
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
void mixaudio(void *portfe, Uint8 *stream, int len)
{
	int i;
	for(i=0;i<len;i++)
	{
		stream[i]=(*((char *)portfe)&0x18)?255:0;
	}
}
#endif

void show_state(unsigned char * RAM, z80 *cpu, int Tstates, bus_t *bus)
{
	int i;
	printf("\nState:\n");
	printf("P C	A F	B C	D E	H L	I x	I y	I R	S P	a f	b c	d e	h l	IFF IM\n");
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
	printf("T-states: %u\tM-cycle: %u[%d]\tInternal regs: %02x-%02x-%02x\tShift state: %u\n", Tstates, cpu->M, cpu->dT, cpu->internal[0], cpu->internal[1], cpu->internal[2], cpu->shiftstate);
	printf("Bus: A=%04x\tD=%02x\t%s|%s|%s|%s|%s|%s|%s\n", bus->addr, bus->data, bus->tris==OUT?"WR":"wr", bus->tris==IN?"RD":"rd", bus->mreq?"MREQ":"mreq", bus->iorq?"IORQ":"iorq", bus->m1?"M1":"m1", bus->rfsh?"RFSH":"rfsh", bus->waitline?"WAIT":"wait");
}

void scrn_update(SDL_Surface *screen, int Tstates, int Fstate, unsigned char RAM[65536], bus_t *bus) // TODO: Maybe one day generate floating bus & ULA snow, but that will be hard!
{
	bool contend=false;
	int line=(Tstates/224)-16;
	int col=((Tstates%224)<<1)-16;
	if((line>=0) && (line<296))
	{
		if((col>=0) && (col<OSIZ_X))
		{
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
				contend=!(((Tstates%8)==5)||((Tstates%8)==6));
			}
			else
			{
				contend=false;
				uladb=0xff;
				ulaab=bus->portfe&0x07;
			}
			bus->waitline=contend&&((((bus->addr)&0xC000)==0x4000)||(bus->iorq&&(bus->tris)&&!((bus->addr)%2)));
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
			unsigned char s=0x80>>((Tstates%4)<<1);
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

int dtext(SDL_Surface * scrn, int x, int y, char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b)
{
	SDL_Color clrFg = {r, g, b,0};
	SDL_Rect rcDest = {x, y, OSIZ_X, 12};
	SDL_FillRect(scrn, &rcDest, SDL_MapRGB(scrn->format, 0, 0, 0));
	SDL_Surface *sText = TTF_RenderText_Solid(font, text, clrFg);
	SDL_BlitSurface(sText, NULL, scrn, &rcDest);
	SDL_FreeSurface(sText);
	return(0);
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
		errupt=z80_tstep(cpu, bus, errupt);
		fflush(stderr);
		if(++tstates>end_tstates)
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
