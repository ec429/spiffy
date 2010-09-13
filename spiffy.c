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
#include <SDL/SDL_ttf.h>
#include <math.h>
#include <time.h>
#include "ops.h"
#include "z80.h"

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
#define ZERR0	"spiffy: encountered bad shift state %u (x%hhu z%hhu y%hhu M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->ods.y, cpu->M
#define ZERR1	"spiffy: encountered bad opcode s%u x%hhu (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->M
#define ZERR2	"spiffy: encountered bad opcode s%u x%hhu z%hhu (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->M
#define ZERR2Y	"spiffy: encountered bad opcode s%u x%hhu y%hhu (p%hhu q%hhu) (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.y, cpu->ods.p, cpu->ods.q, cpu->M
#define ZERR3	"spiffy: encountered bad opcode s%u x%hhu z%hhu y%hhu (p%hhu q%hhu) (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->ods.y, cpu->ods.p, cpu->ods.q, cpu->M
#define ZERRM	"spiffy: encountered bad M-cycle %u in z80 core (s%hhu x%hhu z%hhu y%hhu)\n", cpu->M, cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->ods.y

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
int dtext(SDL_Surface * scrn, int x, int y, char * text, TTF_Font * font, char r, char g, char b);

unsigned long int ramtop;

unsigned char uladb,ulaab,uladbb,ulaabb;

int main(int argc, char * argv[])
{
	TTF_Font *font=NULL;
	if(!TTF_Init())
	{
		font=TTF_OpenFont("Vera.ttf", 12);
	}
	bool debug=false; // Generate debugging info?
	int breakpoint=-1;
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
		else
		{ // unrecognised option, assume it's a filename
			printf("Unrecognised option %s, exiting\n", argv[arg]);
			return(2);
		}
	}
	
	printf(GPL_MSG);
	
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
	int i;
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
	
	memset(cpu->regs, 0, sizeof(unsigned char[26]));
	
	// Fill in register decoding tables
	// tbl_r: B C D E H L (HL) A
	tbl_r[0]=5;
	tbl_r[1]=4;
	tbl_r[2]=7;
	tbl_r[3]=6;
	tbl_r[4]=9;
	tbl_r[5]=8;
	tbl_r[6]=26; // regs[26] does not exist and should not be used
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
	// Fill in other decoding tables
	// tbl_im: 0 1 1 2
	tbl_im[0]=0;
	tbl_im[1]=tbl_im[2]=1;
	tbl_im[3]=2;
	
	cpu->block_ints=false; // was the last opcode an EI or other INT-blocking opcode?
	cpu->IFF[0]=cpu->IFF[1]=false;
	cpu->intmode=0; // Interrupt Mode
	bool reti=false; // was the last opcode RETI?  (some hardware detects this, eg. PIO)
	cpu->waitlim=1; // internal; max dT to allow while WAIT is active
	cpu->disp=false; // have we had the displacement byte? (DD/FD CB)
	
	bus->tris=OFF;
	bus->iorq=false;
	bus->mreq=false;
	bus->m1=false;
	bus->rfsh=false;
	bus->addr=0;
	bus->data=0;
	bus->waitline=false;
	
	int Fstate=0; // FLASH state
	
	SDL_Flip(screen);
	
#ifdef AUDIO
	// Start sound
	SDL_PauseAudio(0);
#endif
	
	time_t start_time=time(NULL);
	int frames=0;
	int Tstates=0;
	cpu->dT=0;
	
	cpu->M=0; // note: my M-cycles do not correspond to official labelling
	cpu->internal[0]=cpu->internal[1]=cpu->internal[2]=0;
	cpu->shiftstate=0;
	
	// Main program loop
	while(!errupt)
	{
		if((!debug)&&(*PC==breakpoint)&&bus->m1)
		{
			debug=true;
		}
		cpu->block_ints=false;
		Tstates++;
		cpu->dT++;
		if(bus->waitline)
			cpu->dT=min(cpu->dT, cpu->waitlim);
		if(bus->mreq&&(bus->tris==IN))
		{
			if(bus->addr<ramtop)
			{
				bus->data=RAM[bus->addr];
			}
			else
			{
				bus->data=floor(rand()*256.0/RAND_MAX);
			}
		}
		else if(bus->mreq&&(bus->tris==OUT))
		{
			if((bus->addr&0xC000)&&(bus->addr<ramtop))
				RAM[bus->addr]=bus->data;
		}
		
		if(bus->iorq&&(bus->tris==OUT))
		{
			if(!(bus->addr&0x01))
				bus->portfe=bus->data;
		}
		
		if(debug)
			show_state(RAM, cpu, Tstates, bus);
		
		if((cpu->dT==0)&&bus->rfsh)
		{
			bus->rfsh=false;
			bus->addr=0;
			bus->mreq=false;
		}
		int oM=cpu->M;
		switch(cpu->M)
		{
			case 0: // M0 = OCF(4)
				switch(cpu->dT)
				{
					case 0:
						bus->tris=OFF;
						bus->addr=*PC;
						bus->iorq=false;
						bus->mreq=false;
						bus->m1=true; // M1 line may be incorrect in prefixed series (Should it remain active for each byte of the opcode?  Or just for the first prefix?  I implement the former)
					break;
					case 1:
						bus->tris=IN;
						bus->addr=*PC;
						bus->iorq=false;
						bus->m1=true;
						bus->mreq=true;
					break;
					case 2:
						(*PC)++;
						cpu->internal[0]=bus->data;
						if((cpu->shiftstate&0x01)&&(cpu->shiftstate&0x0C)&&!cpu->disp) // DD/FD CB d XX; d is displacement byte (this is an OD(3), not an OCF(4))
						{ // Possible further M1 line incorrectness, as I have M1 active for d
							cpu->internal[1]=bus->data;
							cpu->block_ints=true;
							cpu->dT=-1;
							cpu->disp=true;
						}
						else
						{
							if((cpu->internal[0]==0xCB)&&!(cpu->shiftstate&0x02)) // ED CB is an instruction, not a shift
							{
								cpu->shiftstate|=0x01;
								cpu->block_ints=true;
							}
							else if((cpu->internal[0]==0xED)&&!(cpu->shiftstate&0x01)) // CB ED is an instruction, not a shift
							{
								cpu->shiftstate=0x02; // ED may not combine
								cpu->block_ints=true;
							}
							else if((cpu->internal[0]==0xDD)&&(!cpu->shiftstate&0x01)) // CB DD is an instruction, not a shift
							{
								cpu->shiftstate&=~(0x0A); // ED,FD may not combine with DD
								cpu->shiftstate|=0x04;
								cpu->block_ints=true;
								cpu->disp=false;
							}
							else if((cpu->internal[0]==0xFD)&&(!cpu->shiftstate&0x01)) // CB FD is an instruction, not a shift
							{
								cpu->shiftstate&=~(0x06); // DD,ED may not combine with FD
								cpu->shiftstate|=0x08;
								cpu->block_ints=true;
								cpu->disp=false;
							}
							else
							{
								cpu->ods=od_bits(cpu->internal[0]);
								cpu->M++;
							}
							cpu->dT=-2;
							if(cpu->disp) // the XX in DD/FD CB b XX is an IO(5), not an OCF(4)
								cpu->dT--;
							cpu->disp=false;
						}
						bus->mreq=true;
						bus->m1=false;
						bus->rfsh=true;
						bus->tris=OFF;
						bus->addr=((*Intvec)<<8)+*Refresh;
						(*Refresh)++;
						if(!((*Refresh)&0x7f)) // preserve the high bit of R
							(*Refresh)^=0x80;
						cpu->waitlim=1;
					break;
				}
			break;
			case 1: // M1
				if(cpu->shiftstate&0x01)
				{
					switch(cpu->ods.x)
					{
						case 1: // CB x1 == BIT y,r[z]
							if(cpu->shiftstate&0x0C) // FD/DD CB d x2 == BIT y,(IXY+d): M1=MR(4)
							{
								STEP_MR((*IHL)+((signed char)cpu->internal[1]), &cpu->internal[2]);
								if(cpu->M>1)
								{
									cpu->internal[2]&=(1<<cpu->ods.y); // internal[2] is now BIT
									// flags
									// SZ5H3PNC
									// *Z*1**0-
									// ZP: BIT == 0
									// S: BIT && y=7
									// 53: from (IXY+d)h
									cpu->regs[2]&=FC;
									cpu->regs[2]|=FH;
									cpu->regs[2]|=cpu->internal[2]?0:(FZ|FP);
									cpu->regs[2]|=cpu->internal[2]&FS;
									cpu->regs[2]|=(((*IHL)+((signed char)cpu->internal[1]))>>8)&(F5|F3);
									cpu->dT=-2;
									cpu->M=0;
								}
							}
							else // CB x1 == BIT y,r[z]
							{
								fprintf(stderr, ZERR0);
								errupt++;
							}
						break;
						case 2: // CB x2 == RES y,r[z]
							if(cpu->shiftstate&0x0C) // FD/DD CB d x2 == LD r[z],RES y,(IXY+d): M1=MR(4)
							{
								STEP_MR((*IHL)+((signed char)cpu->internal[1]), &cpu->internal[2]);
								if(cpu->M>1)
								{
									cpu->internal[2]&=~(1<<cpu->ods.y);
									if(cpu->ods.z!=6)
									{
										cpu->regs[tbl_r[cpu->ods.z]]=cpu->internal[2]; // H and L are /not/ IXYfied
									}
									cpu->dT=-2;
								}
							}
							else // CB x2 == RES y,r[z]
							{
								fprintf(stderr, ZERR0);
								errupt++;
							}
						break;
						case 3: // CB x3 == SET y,r[z]
							if(cpu->shiftstate&0x0C) // FD/DD CB d x3 == LD r[z],SET y,(IXY+d): M1=MR(4)
							{
								STEP_MR((*IHL)+((signed char)cpu->internal[1]), &cpu->internal[2]);
								if(cpu->M>1)
								{
									cpu->internal[2]|=(1<<cpu->ods.y);
									if(cpu->ods.z!=6)
									{
										cpu->regs[tbl_r[cpu->ods.z]]=cpu->internal[2]; // H and L are /not/ IXYfied
									}
									cpu->dT=-2;
								}
							}
							else // CB x3 == SET y,r[z]
							{
								fprintf(stderr, ZERR0);
								errupt++;
							}
						break;
						default:
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
				else if(cpu->shiftstate&0x02)
				{
					switch(cpu->ods.x)
					{
						case 0: // s2 x0x3 == LNOP
						case 3:
							cpu->M=0;
						break;
						case 1: // s2 x1
							switch(cpu->ods.z)
							{
								case 2: // s2 x1 z2
									if(!cpu->ods.q) // s2 x1 z2 q0 == SBC HL,rp[p]: M1=IO(4)
									{
										if(cpu->dT>=4)
										{
											cpu->M++;
											cpu->dT=0;
										}
									}
									else // s2 x1 z2 q1 == ADC HL,rp[p]: M1=IO(4)
									{
										if(cpu->dT>=4)
										{
											cpu->M++;
											cpu->dT=0;
										}
									}
								break;
								case 3: // s2 x1 z3 == LD rp[p]<=>(nn): M1=ODL(3)
									STEP_OD(1);
								break;
								case 6: // s2 x1 z6 == IM im[y]: M1=IO(0)
									cpu->intmode=tbl_im[cpu->ods.y&3];
									cpu->M=0;
								break;
								case 7: // s2 x1 z7
									switch(cpu->ods.y)
									{
										case 0: // s2 x1 z7 y0 == LD I,A: M1=IO(1)
											if(cpu->dT>=0)
											{
												cpu->regs[15]=cpu->regs[3];
												cpu->regs[2]&=~FP;
												if(cpu->IFF[1])
													cpu->regs[2]|=FP;
												cpu->M=0;
												cpu->dT=-1;
											}
										break;
										default:
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								default:
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 2: // s2 x2
							if((cpu->ods.z<4)&&(cpu->ods.y>3)) // bli[y,z]
							{
								op_bli(cpu, bus);
							}
							else // LNOP
							{
								cpu->M=0;
							}
						break;
						default: // s2 x?
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
				else
				{
					switch(cpu->ods.x)
					{
						case 0: // x0
							switch(cpu->ods.z)
							{
								case 0: // x0 z0
									switch(cpu->ods.y)
									{
										case 0: // x0 z0 y0 == NOP: M1=IO(0)
											cpu->M=0;
										break;
										case 2: // x0 z0 y2 == DJNZ d: M1=IO(1)
											cpu->M++;
											cpu->dT--;
										break;
										case 4: // x0 z0 y4-7 == JR cc[y-4],d: M1=OD(3);
										case 5:
										case 6:
										case 7:
											STEP_OD(1);
											if(cpu->M>1)
											{
												if(!cc(cpu->ods.y-4, cpu->regs[2]))
													cpu->M=0;
											}
										break;
										default:
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 1: // x0 z1
									if(!cpu->ods.q) // x0 z1 q0 == LD rp[p],nn: M1=ODL(3)
									{
										STEP_OD(1);
									}
									else // x0 z1 q1 == ADD HL,rp[p]: M1=IO(4)
									{
										if(cpu->dT>=4)
										{
											cpu->M++;
											cpu->dT=0;
										}
									}
								break;
								case 2: // x0 z2
									switch(cpu->ods.p)
									{
										case 2: // x0 z2 p2 == LD (nn)<=>HL: M1=ODL(3)
										case 3: // x0 z2 p3 == LD (nn)<=>A: M1=ODL(3)
											STEP_OD(1);
										break;
										default:
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 3: // x0 z3
									if(cpu->dT>=0)
									{
										if(!cpu->ods.q) // x0 z3 q0 == INC rp[p]: M1=IO(2)
										{
											(*(unsigned short *)(cpu->regs+IRP(tbl_rp[cpu->ods.p])))++;
										}
										else // x0 z3 q1 == DEC rp[p]: M1=IO(2)
										{
											(*(unsigned short *)(cpu->regs+IRP(tbl_rp[cpu->ods.p])))--;
										}
										cpu->M=0;
										cpu->dT=-2;
									}
								break;
								case 4: // x0 z4 == INC r[y]: M1=IO(0)
									if(cpu->ods.y==6) // x0 z4 y6 == INC (HL)
									{
										if(cpu->shiftstate&0xC) // DD/FD x0 z4 y6 == INC (IXY+d): M1=OD(3)
										{
											STEP_OD(1); // get displacement byte
										}
										else // x0 z4 y6 == INC (HL): M1=MR(4)
										{
											STEP_MR(*HL, &cpu->internal[1]);
											if(cpu->M>1)
											{
												cpu->dT=-2;
											}
										}
									}
									else
									{
										cpu->regs[IR(tbl_r[cpu->ods.y])]=op_inc8(cpu, cpu->regs[IR(tbl_r[cpu->ods.y])]);
										cpu->M=0;
									}
								break;
								case 5: // x0 z5 == DEC r[y]: M1=IO(0)
									if(cpu->ods.y==6) // x0 z5 y6 == DEC (HL)
									{
										if(cpu->shiftstate&0xC) // DD/FD x0 z5 y6 == DEC (IXY+d): M1=OD(3)
										{
											STEP_OD(1); // get displacement byte
										}
										else // x0 z5 y6 == DEC (HL): M1=MR(4)
										{
											STEP_MR(*HL, &cpu->internal[1]);
											if(cpu->M>1)
											{
												cpu->dT=-2;
											}
										}
									}
									else
									{
										cpu->regs[IR(tbl_r[cpu->ods.y])]=op_dec8(cpu, cpu->regs[IR(tbl_r[cpu->ods.y])]);
										cpu->M=0;
									}
								break;
								case 6: // x0 z6 == LD r[y],n: M1=OD(3)
									STEP_OD(1);
									if(cpu->ods.y!=6) // ie, *not* (HL)
									{
										if(cpu->M>1)
										{
											cpu->regs[IR(tbl_r[cpu->ods.y])]=cpu->internal[1];
											cpu->M=0;
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
							if((cpu->ods.y==6)&&(cpu->ods.z==6)) // x1 z6 y6 == HALT
							{
								cpu->M=0;
							}
							else // x1 !(z6 y6) == LD r[y],r[z]
							{
								if(cpu->ods.y==6) // LD (HL),r[z]: M1=MW(3)
								{
									if(cpu->shiftstate&0xC) // LD (IXY+d),r[z]: M1=OD(3)
									{
										STEP_OD(1);
									}
									else
									{
										STEP_MW(*HL, cpu->regs[tbl_r[cpu->ods.z]]);
										if(cpu->M>1)
											cpu->M=0;
									}
								}
								else if(cpu->ods.z==6) // LD r[y],(HL): M1=MR(3)
								{
									if(cpu->shiftstate&0xC) // LD r[y],(IXY+d): M1=OD(3)
									{
										STEP_OD(1);
									}
									else
									{
										STEP_MR(*HL, &cpu->regs[tbl_r[cpu->ods.y]]);
										if(cpu->M>1)
											cpu->M=0;
									}
								}
								else // LD r,r: M1=IO(0)
								{
									cpu->regs[IR(tbl_r[cpu->ods.y])]=cpu->regs[IR(tbl_r[cpu->ods.z])];
									cpu->M=0;
								}
							}
						break;
						case 2: // x2 == alu[y] A,r[z]
							if(cpu->ods.z==6) // r[z]=(HL)
							{
								if(cpu->shiftstate&0xC) // alu[y] A,(IXY+d): M1=OD(3)
								{
									STEP_OD(1); // get displacement byte
								}
								else // alu[y] A,(HL): M1=MR(3)
								{
									STEP_MR(*HL, &cpu->internal[1]);
									if(cpu->M>1)
									{
										op_alu(cpu, cpu->internal[1]);
										cpu->M=0;
									}
								}
							}
							else // M1=IO(0)
							{
								cpu->internal[1]=cpu->regs[IR(tbl_r[cpu->ods.z])];
								op_alu(cpu, cpu->internal[1]);
								cpu->M=0;
							}
						break;
						case 3: // x3
							switch(cpu->ods.z)
							{
								case 1: // x3 z1
									switch(cpu->ods.q)
									{
										case 0: // x3 z1 q0 == POP rp2[p]: M1=SRH(3)
											STEP_SR(2);
										break;
										case 1: // x3 z1 q1
											switch(cpu->ods.p)
											{
												case 0: // x3 z1 q1 p0 == RET: M1=SRH(3)
													STEP_SR(2);
												break;
												case 1: // x3 z1 q1 p1 == EXX: M1=IO(0)
													for(i=4;i<10;i++) // BCDEHL
													{
														unsigned char tmp=cpu->regs[i];
														cpu->regs[i]=cpu->regs[i+0x10];
														cpu->regs[i+0x10]=tmp;
													}
													cpu->M=0;
												break;
												case 3: // x3 z1 q1 p3 == LD SP, HL: M1=IO(2)
													if(cpu->dT==0)
													{
														*SP=*IHL;
														cpu->dT=-2;
														cpu->M=0;
													}
												break;
												default: // x3 z1 q1 p?
													fprintf(stderr, ZERR3);
													errupt++;
												break;
											}
										break;
									}
								break;
								case 3: // x3 z3
									switch(cpu->ods.y)
									{
										case 0: // x3 z3 y0 == JP nn: M1=ODL(3)
											STEP_OD(1);
										break;
										case 2: // x3 z3 y2 == OUT (n),A: M1=OD(3)
											STEP_OD(1);
										break;
										case 5: // x3 z3 y5 == EX DE,HL: M1=IO(0)
											cpu->M=0;
											unsigned short int tmp=*DE;
											*DE=*HL;
											*HL=tmp;
										break;
										case 6: // x3 z3 y6 == DI: M1=IO(0)
											cpu->IFF[0]=cpu->IFF[1]=false;
											cpu->M=0;
										break;
										case 7: // x3 z3 y7 == EI: M1=IO(0)
											cpu->IFF[0]=cpu->IFF[1]=true;
											cpu->M=0;
										break;
										default: // x3 z3 y?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 5: // x3 z5
									switch(cpu->ods.q)
									{
										case 1: // x3 z5 q1
											switch(cpu->ods.p)
											{
												case 0: // x3 z5 q1 p0 == CALL nn: M1=ODL(3)
													STEP_OD(1);
												break;
												default: // x3 z5 q1 p?
													fprintf(stderr, ZERR3);
													errupt++;
												break;
											}
										break;
										default: // x3 z5 q?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 6: // x3 z6 == alu[y] n: M1=OD(3)
									STEP_OD(1);
									if(cpu->M>1)
									{
										op_alu(cpu, cpu->internal[1]);
										cpu->M=0;
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
				if(cpu->shiftstate&0x01)
				{
					switch(cpu->ods.x)
					{
						case 2: // CB x2 == RES y,r[z]
						case 3: // CB x3 == SET y,r[z]
							if(cpu->shiftstate&0x0C) // FD/DD CB d x2/3 == LD r[z],RES/SET y,(IXY+d): M2=MW(3)
							{
								STEP_MW((*IHL)+((signed char)cpu->internal[1]), cpu->internal[2]);
								if(cpu->M>2)
									cpu->M=0;
							}
							else
							{
								fprintf(stderr, ZERR0);
								errupt++;
							}
						break;
						default:
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
				else if(cpu->shiftstate&0x02)
				{
					switch(cpu->ods.x)
					{
						case 1: // s2 x1
							switch(cpu->ods.z)
							{
								case 2: // s2 x1 z2
									if(!cpu->ods.q) // s2 x1 z2 q0 == SBC HL,rp[p]: M2=IO(3)
									{
										if(cpu->dT>=2)
										{
											op_sbc16(cpu);
											cpu->M=0;
											cpu->dT=-1;
										}
									}
									else // s2 x1 z2 q1 == ADC HL,rp[p]: M2=IO(3)
									{
										if(cpu->dT>=2)
										{
											op_adc16(cpu);
											cpu->M=0;
											cpu->dT=-1;
										}
									}
								break;
								case 3: // s2 x1 z3 == LD rp[p]<=>(nn): M2=ODH(3)
									STEP_OD(2);
								break;
								default:
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 2: // s2 x2
							if((cpu->ods.z<4)&&(cpu->ods.y>3)) // bli[y,z]
							{
								op_bli(cpu, bus);
							}
							else // LNOP
							{
								cpu->M=0;
							}
						break;
						default:
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
				else
				{
					switch(cpu->ods.x)
					{
						case 0: // x0
							switch(cpu->ods.z)
							{
								case 0: // x0 z0
									switch(cpu->ods.y)
									{
										case 2: // x0 z0 y2 == DJNZ d: M2=OD(3)
											STEP_OD(1);
											if(cpu->M>2)
											{
												cpu->regs[5]--; // decrement B
												if(cpu->regs[5]==0) // and jump if not zero
													cpu->M=0;
											}
										break;
										case 4: // x0 z0 y4-7 == JR cc[y-4],d: M2=IO(5)
										case 5:
										case 6:
										case 7:
											if(cpu->dT>=4)
											{
												(*PC)+=(signed char)cpu->internal[1];
												cpu->dT=-1;
												cpu->M=0;
											}
										break;
										default:
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 1: // x0 z1
									if(!cpu->ods.q) // x0 z1 q0 == LD rp[p],nn: M2=ODH(3)
									{
										STEP_OD(2);
										if(cpu->M>2)
										{
											cpu->regs[IRP(tbl_rp[cpu->ods.p])]=cpu->internal[1];
											cpu->regs[IRP(tbl_rp[cpu->ods.p])+1]=cpu->internal[2];
											cpu->M=0;
											cpu->dT=-1;
										}
									}
									else // x0 z1 q1 == ADD HL,rp[p]: M2=IO(3)
									{
										if(cpu->dT>=2)
										{
											op_add16(cpu);
											cpu->M=0;
											cpu->dT=-1;
										}
									}
								break;
								case 2: // x0 z2
									switch(cpu->ods.p)
									{
										case 2: // x0 z2 p2 == LD (nn)<=>HL: M2=ODH(3)
										case 3: // x0 z2 p3 == LD (nn)<=>A: M2=ODH(3)
											STEP_OD(2);
										break;
										default:
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 4: // x0 z54== INC r[y]
									if(cpu->ods.y==6) // x0 z4 y6 == INC (HL)
									{
										if(cpu->shiftstate&0xC) // DD/FD x0 z4 y6 = INC (IXY+d): M2=IO(5)
										{
											if(cpu->dT==0)
											{
												cpu->dT=-5;
												cpu->M++;
											}
										}
										else // x0 z4 y6 == INC (HL): M2=MW(3)
										{
											if(cpu->dT==0)
											{
												cpu->internal[1]=op_inc8(cpu, cpu->internal[1]);
											}
											STEP_MW(*IHL, cpu->internal[1]);
											if(cpu->M>2)
											{
												cpu->M=0;
												cpu->dT=-1;
											}
										}
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								break;
								case 5: // x0 z5 == DEC r[y]
									if(cpu->ods.y==6) // x0 z5 y6 == DEC (HL)
									{
										if(cpu->shiftstate&0xC) // DD/FD x0 z5 y6 = DEC (IXY+d): M2=IO(5)
										{
											if(cpu->dT==0)
											{
												cpu->dT=-5;
												cpu->M++;
											}
										}
										else // x0 z5 y6 == DEC (HL): M2=MW(3)
										{
											if(cpu->dT==0)
											{
												cpu->internal[1]=op_dec8(cpu, cpu->internal[1]);
											}
											STEP_MW(*IHL, cpu->internal[1]);
											if(cpu->M>2)
											{
												cpu->M=0;
												cpu->dT=-1;
											}
										}
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								break;
								case 6: // x0 z6 == LD r[y],n: M2(HL):=MW(3)
									if(cpu->ods.y==6)
									{
										if(cpu->shiftstate&0xC) // DD/FD x0 z6 y6 = LD (IXY+d): M2=OD(3)
										{
											STEP_OD(2); // get operand byte
										}
										else // x0 z6 y6 == LD (HL),n: M2=MW(3)
										{
											STEP_MW(*IHL, cpu->internal[1]);
											if(cpu->M>2)
											{
												cpu->M=0;
												cpu->dT=-1;
											}
										}
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								break;
								default:
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 1: // x1
							if((cpu->ods.y==6)&&(cpu->ods.z==6)) // x1 z6 y6 == HALT: no M2
							{
								fprintf(stderr, ZERR3);
								errupt++;
							}
							else // x1 !(z6 y6) == LD r[y],r[z]
							{
								if(cpu->ods.y==6)
								{
									if(cpu->shiftstate&0xC) // LD (IXY+d),r[z]: M2=IO(5)
									{
										if(cpu->dT>=0)
										{
											cpu->dT-=5;
											cpu->M++;
										}
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								}
								else if(cpu->ods.z==6)
								{
									if(cpu->shiftstate&0xC) // LD r[y],(IXY+d): M2=IO(5)
									{
										if(cpu->dT>=0)
										{
											cpu->dT-=5;
											cpu->M++;
										}
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								}
								else // LD r,r: no M2
								{
									fprintf(stderr, ZERR3);
									errupt++;
								}
							}
						break;
						case 3: // x3
							switch(cpu->ods.z)
							{
								case 1: // x3 z1
									switch(cpu->ods.q)
									{
										case 0: // x3 z1 q0 = POP rp2[p]: M2=SRL(3)
											STEP_SR(1);
											cpu->regs[IRP(tbl_rp2[cpu->ods.p])]=I16;
										break;
										default:
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 3: // x3 z3
									switch(cpu->ods.y)
									{
										case 0: // x3 z3 y0 == JP nn: M2=ODH(3)
											STEP_OD(2);
											if(cpu->M>2)
											{
												*PC=I16;
												cpu->M=0;
												cpu->dT=-1;
											}
										break;
										case 2: // x3 z3 y2 == OUT (n),A: M2=PW(4)
											STEP_PW((cpu->regs[3]<<8)+cpu->internal[1], cpu->regs[3]);
											if(cpu->M>2)
											{
												cpu->M=0;
												cpu->dT=-1;
											}
										break;
										default: // x3 z3 y?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 5: // x3 z5
									switch(cpu->ods.q)
									{
										case 1: // x3 z5 q1
											switch(cpu->ods.p)
											{
												case 0: // x3 z5 q1 p0 == CALL nn: M2=ODH(4)
													STEP_OD(2);
													if(cpu->M>2)
														cpu->dT--;
												break;
												default: // x3 z5 q1 p?
													fprintf(stderr, ZERR3);
													errupt++;
												break;
											}
										break;
										default: // x3 z5 q?
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
			case 3: // M3
				if(cpu->shiftstate&0x01)
				{
					fprintf(stderr, ZERR1);
					errupt++;
				}
				else if(cpu->shiftstate&0x02)
				{
					switch(cpu->ods.x)
					{
						case 1: // s2 x1
							switch(cpu->ods.z)
							{
								case 3: // s2 x1 z3
									switch(cpu->ods.q)
									{
										case 0: // s2 x1 z3 q0 == LD (nn), rp[p] : M3=MWL(3)
											STEP_MW(I16, cpu->regs[tbl_rp[cpu->ods.p]]);
										break;
										case 1: // s2 x1 z3 q1 == LD rp[p], (nn) : M3=MRL(3)
											STEP_MR(I16, &cpu->regs[tbl_rp[cpu->ods.p]]);
										break;
									}
								break;
								default: // s2 x1 z?
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 2: // s2 x2
							if((cpu->ods.z<4)&&(cpu->ods.y>3)) // bli[y,z]
							{
								op_bli(cpu, bus);
							}
							else // LNOP
							{
								cpu->M=0;
							}
						break;
						default: // s2 x?
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
				else
				{
					switch(cpu->ods.x)
					{
						case 0: // x0
							switch(cpu->ods.z)
							{
								case 0: // x0 z0
									switch(cpu->ods.y)
									{
										case 2: // x0 z0 y2 == DJNZ d: M3=IO(5)
											if(cpu->dT==0)
											{
												(*PC)+=(signed char)cpu->internal[1];
												cpu->dT=-5;
												cpu->M=0;
											}
										break;
										default: // x0 z0 y?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 2: // x0 z2
									switch(cpu->ods.p)
									{
										case 2: // x0 z2 p2 == LD (nn)<=>HL
											switch(cpu->ods.q)
											{
												case 0: // x0 z2 p2 q0 == LD (nn),HL: M3=MWL(3)
													STEP_MW(I16, cpu->regs[IL]);
												break;
												case 1: // x0 z2 p2 q1 == LD HL,(nn): M3=MRL(3)
													STEP_MR(I16, &cpu->regs[IL]);
												break;
											}
										break;
										case 3: // x0 z2 p3 == LD (nn)<=>A
											switch(cpu->ods.q)
											{
												case 0: // x0 z2 p3 q0 == LD (nn),A: M3=MW(3)
													STEP_MW(I16, cpu->regs[3]);
													if(cpu->M>3)
														cpu->M=0;
												break;
												case 1: // x0 z2 p3 q1 == LD A,(nn): M3=MR(3)
													STEP_MR(I16, &cpu->regs[3]);
												break;
											}
										break;
										default: // x0 z2 p?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 4: // x0 z4 == INC r[y]; M3 should only happen if DD/FD and r[y] is (HL)
								case 5: // x0 z5 == DEC r[y]; M3 should only happen if DD/FD and r[y] is (HL)
									if(cpu->ods.y==6)
									{
										if(cpu->shiftstate&0xC) // DD/FD x0 z4/5 y6 == INC/DEC (IXY+d): M3=MR(3)
										{
											STEP_MR((*IHL)+((signed char)cpu->internal[1]), &cpu->internal[2]);
										}
										else
										{
											fprintf(stderr, ZERR3);
											errupt++;
										}
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								break;
								default: // x0 z?
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 1: // x1
							if((cpu->ods.y==6)&&(cpu->ods.z==6)) // x1 z6 y6 == HALT: no M3
							{
								fprintf(stderr, ZERR3);
								errupt++;
							}
							else // x1 !(z6 y6) == LD r[y],r[z]
							{
								if(cpu->ods.y==6)
								{
									if(cpu->shiftstate&0xC) // LD (IXY+d),r[z]: M3=MW(3)
									{
										STEP_MW(*IHL+(signed char)cpu->internal[1], cpu->regs[tbl_r[cpu->ods.z]]); // H and L unchanged eg. LD (IX+d),H (not IXH)
										if(cpu->M>3)
											cpu->M=0;
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								}
								else if(cpu->ods.z==6)
								{
									if(cpu->shiftstate&0xC) // LD r[y],(IXY+d): M3=MR(3)
									{
										STEP_MR(*IHL+(signed char)cpu->internal[1], &cpu->regs[tbl_r[cpu->ods.y]]); // H and L unchanged eg. LD H,(IX+d) (not IXH)
										if(cpu->M>3)
											cpu->M=0;
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								}
								else // LD r,r: no M3
								{
									fprintf(stderr, ZERR3);
									errupt++;
								}
							}
						break;
						case 3: // x3
							switch(cpu->ods.z)
							{
								case 5: // x3 z5
									switch(cpu->ods.q)
									{
										case 1: // x3 z5 q1
											switch(cpu->ods.p)
											{
												case 0: // x3 z5 q1 p0 == CALL nn: M3=SWH(3)
													STEP_SW((*PC)>>8);
												break;
												default: // x3 z5 q1 p?
													fprintf(stderr, ZERR3);
													errupt++;
												break;
											}
										break;
										default: // x3 z5 q?
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
			case 4: // M4
				if(cpu->shiftstate&0x01)
				{
					fprintf(stderr, ZERR1);
					errupt++;
				}
				else if(cpu->shiftstate&0x02)
				{
					switch(cpu->ods.x)
					{
						case 1: // s2 x1
							switch(cpu->ods.z)
							{
								case 3: // s2 x1 z3
									switch(cpu->ods.q)
									{
										case 0: // s2 x1 z3 q0 == LD (nn), rp[p] : M4=MWH(3)
											STEP_MW(I16+1, cpu->regs[tbl_rp[cpu->ods.p]+1]);
											if(cpu->M>4)
												cpu->M=0;
										break;
										case 1: // s2 x1 z3 q1 == LD rp[p], (nn) : M4=MRH(3)
											STEP_MR(I16+1, &cpu->regs[tbl_rp[cpu->ods.p]+1]);
										break;
									}
								break;
								default: // s2 x1 z?
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						default: // s2 x?
							fprintf(stderr, ZERR1);
							errupt++;
						break;
					}
				}
				else
				{
					switch(cpu->ods.x)
					{
						case 0: // x0
							switch(cpu->ods.z)
							{
								case 2: // x0 z2
									switch(cpu->ods.p)
									{
										case 2: // x0 z2 p2 == LD (nn)<=>HL
											switch(cpu->ods.q)
											{
												case 0: // x0 z2 p2 q0 == LD (nn),HL: M4=MWH(3)
													STEP_MW(I16+1, cpu->regs[IH]);
													if(cpu->M>4)
														cpu->M=0;
												break;
												case 1: // x0 z2 p2 q1 == LD HL,(nn): M4=MRH(3)
													STEP_MR(I16+1, &cpu->regs[IH]);
													if(cpu->M>4)
														cpu->M=0;
												break;
											}
										break;
										default: // x0 z2 p?
											fprintf(stderr, ZERR3);
											errupt++;
										break;
									}
								break;
								case 4: // x0 z4 == INC r[y]; M4 should only happen if DD/FD and r[y] is (HL)
								case 5: // x0 z5 == DEC r[y]; M4 should only happen if DD/FD and r[y] is (HL)
									if(cpu->ods.y==6)
									{
										if(cpu->shiftstate&0xC) // DD/FD x0 z4/5 y6 == INC/DEC (IXY+d): M4=MW(4)
										{
											STEP_MW((*IHL)+((signed char)cpu->internal[1]), cpu->internal[2]+((cpu->ods.z&1)?-1:1));
											if(cpu->M>4)
											{
												cpu->dT--;
												cpu->M=0;
											}
										}
										else
										{
											fprintf(stderr, ZERR3);
											errupt++;
										}
									}
									else
									{
										fprintf(stderr, ZERR3);
										errupt++;
									}
								break;
								default: // x0 z?
									fprintf(stderr, ZERR2);
									errupt++;
								break;
							}
						break;
						case 3: // x3
							switch(cpu->ods.z)
							{
								case 5: // x3 z5
									switch(cpu->ods.q)
									{
										case 1: // x3 z5 q1
											switch(cpu->ods.p)
											{
												case 0: // x3 z5 q1 p0 == CALL nn: M4=SWL(3)
													STEP_SW(*PC);
													if(cpu->M>4)
													{
														cpu->M=0;
														(*PC)=I16;
													}
												break;
												default: // x3 z5 q1 p?
													fprintf(stderr, ZERR3);
													errupt++;
												break;
											}
										break;
										default: // x3 z5 q?
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
		if(oM&&!cpu->M) // when M is set to 0, shift is reset
		{
			cpu->shiftstate=0;
			cpu->disp=false;
		}
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
	long int s_off = ((y * OSIZ_X) + x)<<2;
	unsigned long int pixval = SDL_MapRGB(screen->format, r, g, b),
		* pixloc = screen->pixels + s_off;
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
			unsigned char s=1<<((Tstates%4)<<1);
			bool d=uladb&s;
			if(flash && (Fstate&0x10))
				d=!d;
			pset(screen, col, line, d?ir:pr, d?ig:pg, d?ib:pb);
			d=uladb&(s<<1);
			if(flash && (Fstate&0x10))
				d=!d;
			pset(screen, col+1, line, d?ir:pr, d?ig:pg, d?ib:pb);
		}
	}
}

int dtext(SDL_Surface * scrn, int x, int y, char * text, TTF_Font * font, char r, char g, char b)
{
	SDL_Color clrFg = {r, g, b,0};
	SDL_Rect rcDest = {x, y, OSIZ_X, 12};
	SDL_FillRect(scrn, &rcDest, SDL_MapRGB(scrn->format, 0, 0, 0));
	SDL_Surface *sText = TTF_RenderText_Solid(font, text, clrFg);
	SDL_BlitSurface(sText, NULL, scrn, &rcDest);
	SDL_FreeSurface(sText);
	return(0);
}
