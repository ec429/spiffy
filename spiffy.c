/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010
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
#include "ops.h"

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
#define VERSION_REV	0

#define VERSION_MSG "spiffy %hhu.%hhu.%hhu\n\
 Copyright (C) 2010 Edward Cree.\n\
 License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
 This is free software: you are free to change and redistribute it.\n\
 There is NO WARRANTY, to the extent permitted by law.\n",VERSION_MAJ, VERSION_MIN, VERSION_REV

#define GPL_MSG "spiffy Copyright (C) 2010 Edward Cree.\n\
 This program comes with ABSOLUTELY NO WARRANTY; for details see the GPL v3.\n\
 This is free software, and you are welcome to redistribute it\n\
 under certain conditions: GPL v3+\n"

#define max(a,b)	((a)>(b)?(a):(b))
#define min(a,b)	((a)>(b)?(b):(a))

// z80 core error messages
#define ZERR0	"spiffy: encountered bad shift state %u (x%hhu z%hhu y%hhu M%u) in z80 core\n", shiftstate, ods.x, ods.z, ods.y, M
#define ZERR1	"spiffy: encountered bad opcode s%u x%hhu (M%u) in z80 core\n", shiftstate, ods.x, M
#define ZERR2	"spiffy: encountered bad opcode s%u x%hhu z%hhu (M%u) in z80 core\n", shiftstate, ods.x, ods.z, M
#define ZERR2Y	"spiffy: encountered bad opcode s%u x%hhu y%hhu (M%u) in z80 core\n", shiftstate, ods.x, ods.y, M
#define ZERR3	"spiffy: encountered bad opcode s%u x%hhu z%hhu y%hhu (M%u) in z80 core\n", shiftstate, ods.x, ods.z, ods.y, M
#define ZERRM	"spiffy: encountered bad M-cycle %u in z80 core\n", M

typedef struct _pos
{
	int x;
	int y;
} pos;

SDL_Surface * gf_init();
int pset(SDL_Surface * screen, int x, int y, char r, char g, char b);
int line(SDL_Surface * screen, int x1, int y1, int x2, int y2, char r, char g, char b);
#ifdef AUDIO
void mixaudio(void *portfe, Uint8 *stream, int len);
#endif

// helper fns
void show_state(unsigned char * RAM, unsigned char * regs, int Tstates, int M, unsigned char internal[3], int shiftstate, bool * IFF, int intmode);
od od_bits(unsigned char opcode);
bool cc(unsigned char which, unsigned char flags);

unsigned long int ramtop;

int main(int argc, char * argv[])
{
	int arg;
	for (arg=1; arg<argc; arg++)
	{
		if((strcasecmp(argv[arg], "--version") == 0) || (strcasecmp(argv[arg], "-v") == 0))
		{ // print version info and exit
			printf(VERSION_MSG);
			return(0);
		}
		else
		{ // unrecognised option, assume it's a filename
			printf("Unrecognised option %s, exiting\n", argv[arg]);
			return(2);
		}
	}
	
	printf(GPL_MSG);
	
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
	unsigned char portfe=0xFF; // used by mixaudio (for the EAR) and the screen update (for the BORDCR)
#ifdef AUDIO
	SDL_AudioSpec fmt;
	fmt.freq = SAMPLE_RATE;
	fmt.format = AUDIO_U8;
	fmt.channels = 1;
	fmt.samples = 64;
	fmt.callback = mixaudio;
	fmt.userdata = &portfe;

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
	int i;
	for(i=0;i<ramtop;i++)
	{
		if(i<16384)
		{
			RAM[i]=fgetc(fp);
		}
		else
		{
//			RAM[i]=0;
		}
	}
	fclose(fp);
	
	// Registers:
	// PCAFBCDEHLIxIyIRSPafbcdehl
	// 1032547698badcfe1032547698
	// PC = Program Counter
	// A = Accumulator
	// F = Flags register [7]SZ5H3PNC[0]
	// BC, DE, HL: user registers
	// Ix, Iy: Index registers
	// I: Interrupt Vector
	// R: Memory Refresh
	// SP: Stack Pointer
	// afbcdehl: Alternate register set (EX/EXX)
	unsigned char regs[27]; // the [26] is for internal use, but is deprecated (as we now have the internal[] unnamed registers)
	memset(regs, 0, sizeof(regs));
	
	// Fill in register decoding tables
	// tbl_r: B C D E H L (HL) A
	tbl_r[0]=5;
	tbl_r[1]=4;
	tbl_r[2]=7;
	tbl_r[3]=6;
	tbl_r[4]=9;
	tbl_r[5]=8;
	tbl_r[6]=26; // regs[26] needs to be set up first
	tbl_r[7]=3;
	// tbl_rp: BC DE HL SP
	tbl_rp[0]=4;
	tbl_rp[1]=6;
	tbl_rp[2]=8;
	tbl_rp[3]=16;
	// tbl_rp2: BC DE HL AF
	tbl_rp2[0]=4;
	tbl_rp2[1]=6;
	tbl_rp2[2]=8;
	tbl_rp2[3]=2;
	
	bool IFF[2]; // Interrupts Flip Flops
	bool block_ints=false; // was the last opcode an EI or other INT-blocking opcode?
	IFF[0]=IFF[1]=false;
	int intmode=0; // Interrupt Mode
	bool reti=false; // was the last opcode RETI?  (some hardware detects this, eg. PIO)
	
	enum {OFF,IN,OUT} tris=OFF;
	unsigned short int portno=0; // Address lines for IN/OUT
	unsigned char ioval=0; // Value written by Z80 (OUT) / peripheral (IN)
	unsigned char ioreg=0; // which (regs[]) reg to place the IN result in?
	
	int Fstate=0; // FLASH state
	
	bool trace=true, debug=true; // trace I/O? Generate debugging info?
	
	SDL_Flip(screen);
	
#ifdef AUDIO
	// Start sound
	SDL_PauseAudio(0);
#endif
	
	int Tstates=0;
	int dT=0;
	
	int M=0; // note: my M-cycles do not correspond to official labelling
	unsigned char internal[3]={0,0,0}; // Internal Z80 registers
	od ods;
	
	int shiftstate=0;	// The 'shift state' resulting from prefixes
						// bits as follows: 1=CB 2=ED 4=DD 8=FD
						// Valid states: CBh/1, EDh/2. DDh/4. FDh/8. DDCBh/5 and FDCBh/9.
	
	if(debug)
		show_state(RAM, regs, Tstates, M, internal, shiftstate, IFF, intmode);
	
	// Main program loop
	while(!errupt)
	{
		block_ints=false;
		Tstates++;
		dT++;
		switch(M)
		{
			case 0: // M0 = OCF(4)
				if(dT>=4)
				{
					internal[0]=RAM[(*PC)++];
					if((internal[0]==0xCB)&&(!shiftstate&0x02)) // ED CB is an instruction, not a shift
					{
						shiftstate|=0x01;
						block_ints=true;
					}
					else if((internal[0]==0xED)&&!(shiftstate&0x01)) // CB ED is an instruction, not a shift
					{
						shiftstate=0x02; // ED may not combine
						block_ints=true;
					}
					else if(internal[0]==0xDD)
					{
						shiftstate&=~(0x0A); // ED,FD may not combine with DD
						shiftstate|=0x04;
						block_ints=true;
					}
					else if(internal[0]==0xFD)
					{
						shiftstate&=~(0x06); // DD,ED may not combine with FD
						shiftstate|=0x08;
						block_ints=true;
					}
					else
					{
						ods=od_bits(internal[0]);
						M++;
					}
					dT=0;
				}
			break;
			case 1: // M1
				if(shiftstate&0x01)
				{
					fprintf(stderr, ZERR1);
					errupt++;
				}
				else if(shiftstate&0x02)
				{
					fprintf(stderr, ZERR1);
					errupt++;
				}
				else
				{
					switch(ods.x)
					{
						case 0: // x0
							switch(ods.z)
							{
								case 1: // x0 z1
									if(!ods.q) // x0 z1 q0 == LD rp[p],nn: M1=ODL(3)
									{
										if(dT>=3)
										{
											internal[1]=RAM[(*PC)++];
											M++;
											dT=0;
										}
									}
									else // x0 z1 q1 == ADD HL,rp[p]: M1=IO(4)
									{
										if(dT>=4)
										{
											M++;
											dT=0;
										}
									}
								break;
								default: // x0 z?
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 1: // x1
							if((ods.y==6)&&(ods.z==6)) // x1 z6 y6 == HALT
							{
								M=0;
							}
							else // x1 !(z6 y6) == LD r[y],r[z]
							{
								regs[tbl_r[ods.y]]=regs[tbl_r[ods.z]];
								M=0;
							}
						break;
						case 2: // x2 == alu[y] A,r[z]
							if(ods.z==6) // r[z]=(HL), M1=MR(3)
							{
								if(dT>=3)
								{
									internal[1]=RAM[*HL];
									M++;
									dT=0;
								}
							}
							else // M1=IO(0)
							{
								internal[1]=regs[tbl_r[ods.z]];
								M++;
							}
						break;
						case 3: // x3
							switch(ods.z)
							{
								case 3: // x3 z3
									switch(ods.y)
									{
										case 0: // x3 z3 y0 == JP nn: M1=ODL(3)
											if(dT>=3)
											{
												internal[1]=RAM[(*PC)++];
												M++;
												dT=0;
											}
										break;
										case 6: // x3 z3 y6 == DI
											IFF[0]=IFF[1]=false;
											M=0;
										break;
										default: // x3 z3 y?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								default: // x3 z?
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						default: // x?
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
			break;
			case 2: // M2
				if(shiftstate&0x01)
				{
					fprintf(stderr, ZERR1);
					errupt++;
				}
				else if(shiftstate&0x02)
				{
					fprintf(stderr, ZERR1);
					errupt++;
				}
				else
				{
					switch(ods.x)
					{
						case 0: // x0
							switch(ods.z)
							{
								case 1:
									if(!ods.q) // x0 z1 q0 == LD rp[p],nn: M2=ODH(3)
									{
										if(dT>=3)
										{
											internal[2]=RAM[(*PC)++];
											regs[tbl_rp[ods.p]]=I16;
											M=0;
											dT=0;
										}
									}
									else // x0 z1 q1 == ADD HL,rp[p]: M2=IO(3)
									{
										if(dT>=3)
										{
											op_add16(ods, regs, shiftstate);
											M=0;
											dT=0;
										}
									}
								break;
								default:
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 2: // x2
							// x2 s0/4/8 == alu[y] A,r[z]
							// M2=IO(0)
							op_alu(ods, regs, internal[1]);
							M=0;
						break;
						case 3: // x3
							switch(ods.z)
							{
								case 3: // x3 z3
									switch(ods.y)
									{
										case 0: // x3 z3 y0 == JP nn: M2=ODH(3)
											if(dT>=3)
											{
												internal[2]=RAM[(*PC)++];
												*PC=I16;
												M=0;
												dT=0;
											}
										break;
										default: // x3 z3 y?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								default: // x3 z?
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						default: // x?
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
			break;
			default: // M?
				fprintf(stderr, ZERRM);
				errupt++;
			break;
		}
		if(debug)
			show_state(RAM, regs, Tstates, M, internal, shiftstate, IFF, intmode);
		if(Tstates>=69888)
		{
			SDL_Flip(screen);
			Tstates-=69888;
			// TODO generate an interrupt
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

int pset(SDL_Surface * screen, int x, int y, char r, char g, char b)
{
	long int s_off = ((y * OSIZ_X) + x) * 4;
	unsigned long int pixval = SDL_MapRGB(screen->format, r, g, b),
		* pixloc = screen->pixels + s_off;
	*pixloc = pixval;
	return(0);
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

od od_bits(unsigned char opcode)
{
	od rv;
	rv.x=opcode>>6;
	rv.y=(opcode>>3)%8;
	rv.z=opcode%8;
	rv.p=rv.y>>1;
	rv.q=rv.y%2;
	return(rv);
}

bool cc(unsigned char which, unsigned char flags)
{
	bool rv;
	switch((which%8)>>1) // if we get a bad which, we'll just assume that only the low three bits matter (bad, I know, but might come in handy)
	{
		case 0: // Z
			rv=flags&FZ;
		break;
		case 1: // C
			rv=flags&FC;
		break;
		case 2: // PE (1)
			rv=flags&FP;
		break;
		case 3: // M (S)
			rv=flags&FS;
		break;
	}
	return((rv==0)^(which%2));
}

void show_state(unsigned char * RAM, unsigned char * regs, int Tstates, int M, unsigned char internal[3], int shiftstate, bool * IFF, int intmode)
{
	int i;
	printf("\nState:\n");
	printf("P C  A F  B C  D E  H L  I x  I y  I R  S P  a f  b c  d e  h l  IFF IM\n");
	for(i=0;i<26;i+=2)
	{
		printf("%02x%02x ", regs[i+1], regs[i]);
	}
	printf("%c %c %1x", IFF[0]?'1':'0', IFF[1]?'1':'0', intmode);
	printf("\n\n");
	printf("Memory - near (PC), (HL) and (SP):\n");
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
	printf("T-states: %u\tM-cycle: %u\tInternal regs: %02x-%02x-%02x\tShift state: %u\n", Tstates, M, internal[0], internal[1], internal[2], shiftstate);
}
