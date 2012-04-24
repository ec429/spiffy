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
#include "sysvars.h"
#include "basic.h"
#include "debug.h"
#include "ui.h"
#include "audio.h"
#include "coretest.h"

#define ROM_FILE "48.rom" // Spectrum ROM file (TODO: make configable)

#define GPL_MSG "spiffy Copyright (C) 2010-12 Edward Cree.\n\
 This program comes with ABSOLUTELY NO WARRANTY; for details see the GPL v3.\n\
 This is free software, and you are welcome to redistribute it\n\
 under certain conditions: GPL v3+\n"

typedef struct
{
	bool memwait;
	bool iowait;
	bool t1;
}
ula_t;

// helper fns
void scrn_update(SDL_Surface *screen, int Tstates, int frames, int frameskip, int Fstate, const unsigned char *RAM, bus_t *bus, ula_t *ula);
void getedge(libspectrum_tape *deck, bool *play, bool stopper, bool *ear, uint32_t *T_to_tape_edge, int *edgeflags, int *oldtapeblock, unsigned int *tapeblocklen);
void loadfile(const char *fn, libspectrum_tape **deck, libspectrum_snap **snap);
void loadsnap(libspectrum_snap *snap, z80 *cpu, bus_t *bus, unsigned char *RAM, int *Tstates);
void savesnap(libspectrum_snap **snap, z80 *cpu, bus_t *bus, unsigned char *RAM, int Tstates);

int main(int argc, char * argv[])
{
	TTF_Font *font=NULL;
	if(!TTF_Init())
	{
		font=TTF_OpenFont(PREFIX"/share/fonts/Vera.ttf", 12);
		if(!font) font=TTF_OpenFont("Vera.ttf", 12);
	}
	bool debug=false; // Generate debugging info?
	bool debugcycle=false; // Single-Tstate stepping?
	bool trace=false; // execution tracing in debugger?
	bool coretest=false; // run the core tests?
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
		else if((strcmp(argv[arg], "--pause") == 0) || (strcmp(argv[arg], "-p") == 0))
		{ // start with the emulation paused
			pause=true;
		}
		else if((strcmp(argv[arg], "--Tstate") == 0) || (strcmp(argv[arg], "-T") == 0))
		{ // activate single-Tstate stepping under debugger
			debugcycle=true;
		}
		else if((strcmp(argv[arg], "--no-Tstate") == 0) || (strcmp(argv[arg], "+T") == 0))
		{ // deactivate single-Tstate stepping under debugger
			debugcycle=false;
		}
		else if((strcmp(argv[arg], "--coretest") == 0) || (strcmp(argv[arg], "-c") == 0))
		{ // run the core tests
			coretest=true;
		}
		else
		{ // unrecognised option, assume it's a filename
			fn=argv[arg];
		}
	}
	
	printf(GPL_MSG);
	bool ls=!libspectrum_init();
	
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
	button *buttons;
	ui_init(screen, &buttons, edgeload, pause);
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
	unsigned char *sinc_rate=get_sinc_rate();
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
	libspectrum_snap *snap=NULL;
	
	SDL_Flip(screen);
#ifdef AUDIO
	// Start sound
	SDL_PauseAudio(0);
#endif /* AUDIO */
	
	int frames=0;
	int Tstates=0;
	uint32_t T_to_tape_edge=0;
	int edgeflags=0;
	
	if(ls&&fn)
	{
		loadfile(fn, &deck, &snap);
		if(snap)
		{
			loadsnap(snap, cpu, bus, RAM, &Tstates);
			fprintf(stderr, "Loaded snap '%s'\n", fn);
			libspectrum_snap_free(snap);
			snap=NULL;
		}
	}
	
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
		if(!(Tstates%(69888*50/(SAMPLE_RATE**sinc_rate))))
		{
			abuf.play=play;
			unsigned int newwp=(abuf.wp+1)%SINCBUFLEN;
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
					static char *oldline=NULL;
					char *thisline=strdup(line);
					char *cmd=strtok(line, " ");
					if(!cmd)
					{
						if(oldline) cmd=strtok(oldline, " ");
						free(thisline);
					}
					else
					{
						free(oldline);
						oldline=thisline;
					}
					if(!cmd)
					{
						fprintf(stderr, "This is the spiffy debugger.\nType `h' for a list of commands, or `help h' for a list of help sections\n");
					}
					else
					{
						if((strcmp(cmd, "c")==0)||(strcmp(cmd, "cont")==0))
						{
							debug=false;
							derrupt++;
						}
						else if((strcmp(cmd, "h")==0)||(strcmp(cmd, "help")==0))
						{
							const char *what=strtok(NULL, " ");
							if(!what) what="";
							if(strcmp(what, "h")==0)
								fprintf(stderr, h_h);
							else if(strcmp(what, "m")==0)
								fprintf(stderr, h_m);
							else if(strcmp(what, "v")==0)
								fprintf(stderr, h_v);
							else if(strcmp(what, "k")==0)
								fprintf(stderr, h_k);
							else if(strcmp(what, "=")==0)
								fprintf(stderr, h_eq);
							else
								fprintf(stderr, h_cmds);
						}
						else if((strcmp(cmd, "s")==0)||(strcmp(cmd, "state")==0))
							show_state(RAM, cpu, Tstates, bus);
						else if((strcmp(cmd, "t")==0)||(strcmp(cmd, "trace")==0))
							trace=true;
						else if((strcmp(cmd, "!t")==0)||(strcmp(cmd, "!trace")==0))
							trace=false;
						else if(strcmp(cmd, "1")==0)
							debugcycle=true;
						else if(strcmp(cmd, "!1")==0)
							debugcycle=false;
						else if((strcmp(cmd, "n")==0)||(strcmp(cmd, "next")==0))
							derrupt++;
						else if((strcmp(cmd, "=")==0)||(strcmp(cmd, "assign")==0))
						{
							char *what=strtok(NULL, " ");
							if(what)
							{
								char *rest=strtok(NULL, "");
								const char *reglist="AFBCDEHLXxYyIRSPafbcdehl";
								int reg=reg16(what);
								bool is16=(reg>=0);
								if(strlen(what)==1)
								{
									const char *p=strchr(reglist, *what);
									if(p)
										reg=(p+2-reglist)^1;
									is16=false;
								}
								if(reg>=0)
								{
									if(rest)
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
											fprintf(stderr, "set: bad value `%s'\n", rest);
										}
									}
									else
									{
										if(is16)
											fprintf(stderr, "%s = %04x\n", what, *(unsigned short int *)(cpu->regs+reg));
										else
											fprintf(stderr, "%c = %02x\n", reglist[(reg-2)^1], cpu->regs[reg]);
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
								char *a=strtok(NULL, " ");
								char *rest=strtok(NULL, "");
								unsigned int addr;
								if(a)
								{
									if(sscanf(a, "%x", &addr)==1) mdisplay(RAM, addr, what, rest);
									else if(*a=='%')
									{
										size_t plus=strcspn(a, "+-");
										signed int offset=0;
										if(a[plus]) sscanf(a+plus, "%d", &offset);
										a[plus]=0;
										const struct sysvar *sv=sysvarbyname(a+1);
										if(sv)
											mdisplay(RAM, sv->addr+offset, what, rest);
										else
											fprintf(stderr, "memory: no such sysvar `%s'\n", a+1);
									}
									else if(*a=='(')
									{
										char *brack=strchr(a, ')');
										if(brack)
										{
											*brack++=0;
											signed int offset=0;
											if(*brack) sscanf(brack, "%d", &offset);
											const struct sysvar *sv=sysvarbyname(a+2);
											if(sv)
											{
												if(sv->type==SVT_ADDR)
													mdisplay(RAM, peek16(sv->addr)+offset, what, rest);
												else
													fprintf(stderr, "memory: sysvar `%s' is not of type SVT_ADDR\n", sv->name);
											}
											else
												fprintf(stderr, "memory: no such sysvar `%s'\n", a+2);
										}
										else
											fprintf(stderr, "memory: unmatched `('\n");
									}
									else if(*a=='*')
									{
										size_t plus=strcspn(a, "+-");
										signed int offset=0;
										if(a[plus]) sscanf(a+plus, "%d", &offset);
										a[plus]=0;
										int reg=reg16(a+1);
										if(reg>=0)
											mdisplay(RAM, (*(unsigned short int *)(cpu->regs+reg))+offset, what, rest);
										else
											fprintf(stderr, "memory: `%s' is not a 16-bit register\n", a+1);
									}
									else
										fprintf(stderr, "memory: missing address\n");
								}
								else
									fprintf(stderr, "memory: missing address\n");
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
							unsigned short int sv_vars=peek16(sysvarbyname("VARS")->addr), i=sv_vars, l;
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
										if(what) match=!strcmp(fullname.buf, what);
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
						else if((strcmp(cmd, "k")==0)||(strcmp(cmd, "kn")==0))
						{
							unsigned short int sv_prog=peek16(sysvarbyname("PROG")->addr), sv_vars=peek16(sysvarbyname("VARS")->addr), i=sv_prog;
							bool shownums=!strcmp(cmd, "kn");
							char *what=strtok(NULL, "");
							signed int lnum=-1;
							if(what) sscanf(what, "%d", &lnum);
							unsigned int nlines=0;
							bas_line *lines=NULL;
							while(i<sv_vars)
							{
								bas_line new;
								new.number=RAM[i+1]|(RAM[i]<<8); // Big-endian!  Crazy Sinclair ROM!
								new.addr=i;
								i+=2;
								new.line.l=peek16(i);
								i+=2;
								new.line.i=0;
								new.line.buf=malloc(new.line.l);
								while(new.line.i+1<new.line.l)
								{
									append_char(&new.line, RAM[i+new.line.i]);
								}
								i+=new.line.l;
								unsigned int n=nlines++;
								bas_line *nl=realloc(lines, nlines*sizeof(bas_line));
								if(!nl)
								{
									nlines=n;
									perror("realloc");
								}
								else
								{
									(lines=nl)[n]=new;
								}
							}
							if(lnum<0)
								qsort(lines, nlines, sizeof(bas_line), compare_bas_line);
							for(unsigned int l=0;l<nlines;l++)
							{
								if((lnum<0)||(lnum==lines[l].number))
								{
									fprintf(stderr, "%04x %u ", lines[l].addr, lines[l].number);
									for(size_t p=0;p<lines[l].line.i;p++)
									{
										if((lines[l].line.buf[p]==0x0E)&&(p+5<lines[l].line.i))
										{
											if(shownums) fprintf(stderr, "[%g]", float_decode((const unsigned char *)lines[l].line.buf, p+1));
											p+=5;
										}
										else
											fputs(baschar(lines[l].line.buf[p]), stderr);
									}
									fputc('\n', stderr);
								}
							}
						}
						else if((strcmp(cmd, "y")==0)||(strcmp(cmd, "sysvars")==0))
						{
							char *what=strtok(NULL, "");
							bool match=!what;
							unsigned int i=0;
							const struct sysvar *sv=sysvars();
							while(sv[i].name)
							{
								if(what) match=!strcasecmp(what, sv[i].name);
								if(match)
								{
									fprintf(stderr, "%04x %6s", sv[i].addr, sv[i].name);
									unsigned char c=RAM[sv[i].addr];
									switch(sv[i].type)
									{
										case SVT_CHAR:
											if((c>=32)&&(c<127))
												fprintf(stderr, " = '%c'", c);
											fprintf(stderr, " = %u = 0x%02x = '\\%03o'\n", c, c, c);
										break;
										case SVT_FLAGS:
											fprintf(stderr, " = 0x%02x = ", c);
											for(unsigned int b=0;b<8;b++)
												fputc(((c<<b)&0x80)?'1':'0', stderr);
											fputc('\n', stderr);
										break;
										case SVT_ADDR:
											fprintf(stderr, " = 0x%04x\n", peek16(sv[i].addr));
										break;
										case SVT_BYTES:
											fprintf(stderr, " = {");
											for(unsigned int b=0;b<sv[i].len;b++)
											{
												if(b) fputc(' ', stderr);
												fprintf(stderr, "%02x", RAM[sv[i].addr+b]);
											}
											fprintf(stderr, "}\n");
										break;
										case SVT_U8:
											fprintf(stderr, " = %u\n", c);
										break;
										case SVT_U16:
											fprintf(stderr, " = %u\n", peek16(sv[i].addr));
										break;
										case SVT_U24:
											fprintf(stderr, " = %u\n", peek16(sv[i].addr)|(RAM[sv[i].addr+2]<<16));
										break;
										case SVT_XY:
											fprintf(stderr, " = (%u,%u)\n", c, RAM[sv[i].addr+1]);
										break;
										default:
											fprintf(stderr, " has bad type %d\n", sv[i].type);
										break;
									}
									if(what) break;
								}
								i++;
							}
							if(what&&!match) fprintf(stderr, "No such sysvar `%s'\n", what);
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
				}
				free(line);
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
				snprintf(text, 32, "SR:%03u", *sinc_rate);
				dtext(screen, 92, 320, 64, text, font, 0x9f, 0x9f, 0x9f);
				uparrow(screen, sr_up, 0xffdfff, 0x3f4f3f);
				downarrow(screen, sr_down, 0xdfffff, 0x4f3f3f);
				#endif /* AUDIO */
			}
			SDL_Event event;
			while(SDL_PollEvent(&event))
			{
				switch(event.type)
				{
					case SDL_QUIT:
						errupt++;
					break;
					case SDL_KEYDOWN:
						if(event.key.type==SDL_KEYDOWN)
						{
							SDL_keysym key=event.key.keysym;
							if(key.sym==SDLK_ESCAPE)
								debug=true;
							#ifdef AUDIO
							else if(key.sym==SDLK_KP_ENTER)
								abuf.wp=(abuf.wp+1)%AUDIOBUFLEN;
							#endif /* AUDIO */
							else if(key.sym==SDLK_RETURN)
								kstate[6][0]=true;
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
								kstate[0][0]=true;
							else if((key.sym==SDLK_LCTRL)||(key.sym==SDLK_RCTRL))
								kstate[7][1]=true;
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
								kstate[6][0]=false;
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
								kstate[0][0]=false;
							else if((key.sym==SDLK_LCTRL)||(key.sym==SDLK_RCTRL))
								kstate[7][1]=false;
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
									FILE *p=popen("spiffy-filechooser --load\"--title=Spiffy - Load Tape or Snapshot\"", "r");
									if(p)
									{
										char *fn=fgetl(p);
										fclose(p);
										if(fn&&(*fn!='-'))
										{
											loadfile(fn+1, &deck, &snap);
											if(snap)
											{
												loadsnap(snap, cpu, bus, RAM, &Tstates);
												fprintf(stderr, "Loaded snap '%s'\n", fn+1);
												libspectrum_snap_free(snap);
												snap=NULL;
											}
										}
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
									debug=true;
								else if(pos_rect(mouse, snapbutton.posn))
								{
									char *fn=NULL;
									FILE *p=popen("spiffy-filechooser --save \"--title=Spiffy - Save Snapshot\"", "r");
									if(p)
									{
										fn=fgetl(p);
										fclose(p);
									}
									if(fn&&(*fn!='-'))
									{
										FILE *sf=fopen(fn+1, "wb");
										if(sf)
										{
											savesnap(&snap, cpu, bus, RAM, Tstates);
											unsigned char *buffer=NULL; size_t l=0;
											int out_flags;
											libspectrum_snap_write(&buffer, &l, &out_flags, snap, LIBSPECTRUM_ID_SNAPSHOT_Z80, NULL, 0);
											fwrite(buffer, 1, l, sf);
											if(out_flags&LIBSPECTRUM_FLAG_SNAPSHOT_MAJOR_INFO_LOSS)
												fprintf(stderr, "Warning: libspectrum reports Major Info Loss, snap probably broken\n");
											else if(out_flags&LIBSPECTRUM_FLAG_SNAPSHOT_MINOR_INFO_LOSS)
												fprintf(stderr, "Warning: libspectrum reports Minor Info Loss, snap may be broken\n");
											else
												fprintf(stderr, "Snapshot saved\n");
											libspectrum_snap_set_pages(snap, 5, NULL);
											libspectrum_snap_set_pages(snap, 2, NULL);
											libspectrum_snap_set_pages(snap, 0, NULL);
											libspectrum_snap_free(snap);
											snap=NULL;
											fclose(sf);
										}
										else
											perror("fopen");
									}
									free(fn);
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
									*sinc_rate=min(*sinc_rate+1,MAX_SINC_RATE);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, sr_down))
								{
									*sinc_rate=max(*sinc_rate-1,1);
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
										char *fn=NULL;
										FILE *p=popen("spiffy-filechooser --save \"--title=Spiffy - Save Audio Capture\"", "r");
										if(p)
										{
											fn=fgetl(p);
											fclose(p);
										}
										if(!(fn&&(*fn=='-')))
										{
											FILE *a=fopen(fn?fn+1:"record.wav", "wb");
											if(a)
												wavheader(a);
											abuf.record=a;
										}
										free(fn);
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
									*sinc_rate=min(*sinc_rate<<1,MAX_SINC_RATE);
									update_sinc(filterfactor);
								}
								else if(pos_rect(mouse, sr_down))
								{
									*sinc_rate=max(*sinc_rate>>1,1);
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
#ifdef AUDIO
	abuf.play=true; // let the audio thread run free and finish
#endif
	return(0);
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

void loadfile(const char *fn, libspectrum_tape **deck, libspectrum_snap **snap)
{
	*snap=NULL;
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
			fn=NULL;
		}
		else
		{
			libspectrum_class_t class;
			if(libspectrum_identify_class(&class, type))
				fn=NULL;
			else
			{
				switch(class)
				{
					case LIBSPECTRUM_CLASS_TAPE:
						if(*deck) libspectrum_tape_free(*deck);
						if((*deck=libspectrum_tape_alloc()))
						{
							if(libspectrum_tape_read(*deck, (unsigned char *)data.buf, data.i, type, fn))
								libspectrum_tape_free(*deck);
							else
								fprintf(stderr, "Mounted tape '%s'\n", fn);
						}
					break;
					case LIBSPECTRUM_CLASS_SNAPSHOT:
						*snap=libspectrum_snap_alloc();
						if(libspectrum_snap_read(*snap, (unsigned char *)data.buf, data.i, type, fn))
						{
							fprintf(stderr, "Snap load failed\n");
							libspectrum_snap_free(*snap);
							*snap=NULL;
						}
					break;
					default:
						fprintf(stderr, "This class of file is not supported!\n");
					break;
				}
			}
		}
		free_string(&data);
	}
}

void loadsnap(libspectrum_snap *snap, z80 *cpu, bus_t *bus, unsigned char *RAM, int *Tstates)
{
	if(snap)
	{
		if(libspectrum_snap_machine(snap)!=LIBSPECTRUM_MACHINE_48) fprintf(stderr, "loadsnap: warning: machine is not 48, snap will probably fail\n");
		z80_reset(cpu, bus);
		AREG=libspectrum_snap_a(snap);
		FREG=libspectrum_snap_f(snap);
		*BC=libspectrum_snap_bc(snap);
		*DE=libspectrum_snap_de(snap);
		*HL=libspectrum_snap_hl(snap);
		aREG=libspectrum_snap_a_(snap);
		fREG=libspectrum_snap_f_(snap);
		*BC_=libspectrum_snap_bc_(snap);
		*DE_=libspectrum_snap_de_(snap);
		*HL_=libspectrum_snap_hl_(snap);
		*Ix=libspectrum_snap_ix(snap);
		*Iy=libspectrum_snap_iy(snap);
		*Intvec=libspectrum_snap_i(snap);
		*Refresh=libspectrum_snap_r(snap);
		*SP=libspectrum_snap_sp(snap);
		*PC=libspectrum_snap_pc(snap);
		cpu->IFF[0]=libspectrum_snap_iff1(snap);
		cpu->IFF[1]=libspectrum_snap_iff2(snap);
		cpu->intmode=libspectrum_snap_im(snap);
		*Tstates=libspectrum_snap_tstates(snap);
		cpu->halt=libspectrum_snap_halted(snap);
		cpu->block_ints=libspectrum_snap_last_instruction_ei(snap);
		bus->portfe=libspectrum_snap_out_ula(snap);
		memcpy(RAM+0x4000, libspectrum_snap_pages(snap, 5), 0x4000);
		memcpy(RAM+0x8000, libspectrum_snap_pages(snap, 2), 0x4000);
		memcpy(RAM+0xC000, libspectrum_snap_pages(snap, 0), 0x4000);
		// At present we ignore SLT data
	}
}

void savesnap(libspectrum_snap **snap, z80 *cpu, bus_t *bus, unsigned char *RAM, int Tstates)
{
	if((*snap=libspectrum_snap_alloc()))
	{
		libspectrum_snap_set_machine(*snap, LIBSPECTRUM_MACHINE_48);
		libspectrum_snap_set_a(*snap, AREG);
		libspectrum_snap_set_f(*snap, FREG);
		libspectrum_snap_set_bc(*snap, *BC);
		libspectrum_snap_set_de(*snap, *DE);
		libspectrum_snap_set_hl(*snap, *HL);
		libspectrum_snap_set_a_(*snap, aREG);
		libspectrum_snap_set_f_(*snap, fREG);
		libspectrum_snap_set_bc_(*snap, *BC_);
		libspectrum_snap_set_de_(*snap, *DE_);
		libspectrum_snap_set_hl_(*snap, *HL_);
		libspectrum_snap_set_ix(*snap, *Ix);
		libspectrum_snap_set_iy(*snap, *Iy);
		libspectrum_snap_set_i(*snap, *Intvec);
		libspectrum_snap_set_r(*snap, *Refresh);
		libspectrum_snap_set_sp(*snap, *SP);
		libspectrum_snap_set_pc(*snap, *PC);
		libspectrum_snap_set_iff1(*snap, cpu->IFF[0]);
		libspectrum_snap_set_iff2(*snap, cpu->IFF[1]);
		libspectrum_snap_set_im(*snap, cpu->intmode);
		libspectrum_snap_set_tstates(*snap, Tstates);
		libspectrum_snap_set_halted(*snap, cpu->halt);
		libspectrum_snap_set_last_instruction_ei(*snap, cpu->block_ints);
		libspectrum_snap_set_out_ula(*snap, bus->portfe);
		libspectrum_snap_set_pages(*snap, 5, RAM+0x4000);
		libspectrum_snap_set_pages(*snap, 2, RAM+0x8000);
		libspectrum_snap_set_pages(*snap, 0, RAM+0xC000);
	}
}
