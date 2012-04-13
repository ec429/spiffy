/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	z80.c - Z80 core
*/

#include <stdio.h>
#include "string.h"
#include "z80.h"
#include "ops.h"

#define max(a,b)	((a)>(b)?(a):(b))
#define min(a,b)	((a)>(b)?(b):(a))

// z80 core error messages
#define ZERR0	"spiffy: encountered bad shift state %u (x%u z%u y%u M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->ods.y, cpu->M
#define ZERR1	"spiffy: encountered bad opcode s%u x%u (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->M
#define ZERR2	"spiffy: encountered bad opcode s%u x%u z%u (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->M
#define ZERR2Y	"spiffy: encountered bad opcode s%u x%u y%u (p%u q%u) (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.y, cpu->ods.p, cpu->ods.q, cpu->M
#define ZERR3	"spiffy: encountered bad opcode s%u x%u z%u y%u (p%u q%u) (M%u) in z80 core\n", cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->ods.y, cpu->ods.p, cpu->ods.q, cpu->M
#define ZERRM	"spiffy: encountered bad M-cycle %u in z80 core (s%u x%u z%u y%u)\n", cpu->M, cpu->shiftstate, cpu->ods.x, cpu->ods.z, cpu->ods.y

void z80_init(void)
{
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
	// tbl_im: 0 0 1 2
	tbl_im[0]=tbl_im[1]=0;
	tbl_im[2]=1;
	tbl_im[3]=2;
}

void z80_reset(z80 *cpu, bus_t *bus)
{
	memset(cpu->regs, 0, sizeof(unsigned char[26]));
	cpu->block_ints=false; // was the last opcode an EI or other INT-blocking opcode?
	cpu->IFF[0]=cpu->IFF[1]=false;
	cpu->intmode=0; // Interrupt Mode
	cpu->disp=false; // have we had the displacement byte? (DD/FD CB)
	cpu->halt=false;
	cpu->M=0; // note: my M-cycles do not correspond to official labelling
	cpu->dT=0;
	cpu->internal[0]=cpu->internal[1]=cpu->internal[2]=0;
	cpu->shiftstate=0;
	cpu->intacc=false;
	cpu->nmiacc=false;
	cpu->nothing=0;
	cpu->steps=0;
	
	bus->tris=OFF;
	bus->iorq=false;
	bus->mreq=false;
	bus->m1=false;
	bus->rfsh=false;
	bus->addr=0;
	bus->data=0;
	bus->reti=false;
	bus->halt=false;
}

void bus_reset(bus_t *bus)
{
	bus->irq=false;
	bus->nmi=false;
	bus->waitline=false;
	bus->clk_inhibit=false;
	bus->reset=true;
}

int z80_tstep(z80 *cpu, bus_t *bus, int errupt)
{
	if(bus->clk_inhibit) return(errupt);
	cpu->dT++;
	if(unlikely(cpu->nothing))
	{
		cpu->nothing--;
		if(cpu->steps)
			cpu->steps--;
		return(errupt);
	}
	if(cpu->steps)
	{
		cpu->steps--;
		switch(cpu->ste)
		{
			case MW:
				STEP_MW(cpu->sta, cpu->stv);
			break;
			case PR:
				STEP_PR(cpu->sta, cpu->stp);
			break;
			case PW:
				STEP_PW(cpu->sta, cpu->stv);
			break;
			case SW:
				STEP_SW(cpu->stv);
			break;
		}
		return(errupt);
	}
	if(unlikely(bus->reset))
	{
		z80_reset(cpu, bus);
	}
	if((cpu->dT==0)&&bus->rfsh)
	{
		bus->rfsh=false;
		bus->addr=0;
		bus->mreq=false;
	}
	if(unlikely(cpu->nmiacc)) // XXX This should take a non-zero amount of time!  Also, it should stack the return value!
	{
		*PC=0x0066;
		cpu->halt=false;
		cpu->nmiacc=false;
		if(cpu->halt)
			return(errupt);
	}
	else if(unlikely(cpu->intacc))
	{
		switch(cpu->intmode)
		{
			case 0:
				cpu->halt=true;
				if(cpu->dT>5) // XXX this will break horribly for opcodes longer than a single byte, but I have no idea how those work anyway
				{
					cpu->internal[0]=bus->data;
					cpu->ods=od_bits(cpu->internal[0]);
					cpu->M=1;
					cpu->dT=0;
					cpu->halt=false;
					cpu->intacc=false;
				}
			break;
			case 1:
				cpu->halt=true;
				if(cpu->dT>6)
				{
					cpu->internal[0]=0xff;
					cpu->ods=od_bits(cpu->internal[0]);
					cpu->M=1;
					cpu->dT=0;
					cpu->halt=false;
					cpu->intacc=false;
				}
			break;
			case 2:
				switch(cpu->M)
				{
					case 0:
						cpu->halt=true;
						switch(cpu->dT)
						{
							case 0:
								bus->addr=*PC;
								bus->m1=true;
							break;
							case 2:
								bus->iorq=true;
							break;
							case 4:
								cpu->internal[1]=bus->data;
								cpu->internal[2]=*Intvec;
								bus->m1=false;
								bus->iorq=false;
								bus->mreq=true;
								bus->rfsh=true;
								bus->addr=((*Intvec)<<8)|*Refresh;
							break;
							case 6:
								bus->rfsh=false;
								cpu->M=1;
								cpu->dT=-1;
							break;
						}
					break;
					case 1: // M1=SWH(3)
						STEP_SW((*PC)>>8);
					break;
					case 2: // M2=SWL(3)
						STEP_SW(*PC);
					break;
					case 3: // M3=MRL(3)
						STEP_MR(I16, &cpu->regs[0]);
					break;
					case 4: // M4=MRH(3)
						STEP_MR(I16+1, &cpu->regs[1]);
						if(cpu->M>4)
						{
							cpu->M=0;
							cpu->intacc=false;
							cpu->halt=false;
						}
					break;
				}
			break;
		}
		if(cpu->halt)
			return(errupt);
	}
	if(unlikely(cpu->halt))
	{
		cpu->M=0;
		switch(cpu->dT)
		{
			case 0:
			case 1:
				bus->tris=OFF;
				bus->addr=*PC;
				bus->iorq=false;
				bus->mreq=false;
				bus->m1=true;
				bus->rfsh=false;
			break;
			case 2:
				bus->tris=OFF;
				bus->addr=*PC;
				bus->iorq=false;
				bus->mreq=false;
				bus->m1=false;
				bus->rfsh=true;
				bus->addr=((*Intvec)<<8)+*Refresh;
				(*Refresh)++;
				if(!((*Refresh)&0x7f)) // preserve the high bit of R
					(*Refresh)^=0x80;
			break;
			case 3:
				bus->tris=OFF;
				bus->addr=*PC;
				bus->iorq=false;
				bus->mreq=false;
				bus->m1=true;
				bus->rfsh=false;
				cpu->dT=-1;
				if(bus->nmi&&!cpu->block_ints)
				{
					cpu->IFF[0]=false;
					cpu->nmiacc=true;
				}
				else if(bus->irq&&cpu->IFF[0]&&!cpu->block_ints)
				{
					cpu->IFF[0]=cpu->IFF[1]=false;
					cpu->intacc=true;
				}
			break;
		}
		return(errupt);
	}
	int oM=cpu->M;
	switch(cpu->M)
	{
		case 0: // M0 = OCF(4)
			if(likely(!(cpu->intacc||cpu->nmiacc)))
			{
				switch(cpu->dT)
				{
					case 0:
						bus->tris=IN;
						bus->addr=*PC;
						bus->iorq=false;
						bus->mreq=false;
						bus->m1=!((cpu->shiftstate&0x01)&&(cpu->shiftstate&0x0C)); // M1 line may be incorrect in prefixed series (Should it remain active for each byte of the opcode?	Or just for the first prefix?	I implement the former.	However, after DD/FD CB, the next two fetches (d and XX) are not M1)
						//cpu->nothing=1;
					break;
					case 1:
						bus->tris=IN;
						bus->addr=*PC;
						bus->iorq=false;
						bus->mreq=true;
						bus->m1=!((cpu->shiftstate&0x01)&&(cpu->shiftstate&0x0C));
					break;
					case 2:
						if(unlikely(bus->waitline))
							cpu->dT--;
						else
						{
							(*PC)++;
							bool rfix=false, rblock=false;
							cpu->internal[0]=bus->data;
							if((cpu->shiftstate&0x01)&&(cpu->shiftstate&0x0C)&&!cpu->disp) // DD/FD CB d XX; d is displacement byte (this is an OD(3), not an OCF(4))
							{
								cpu->internal[1]=bus->data;
								cpu->block_ints=true;
								cpu->dT=-1;
								cpu->disp=true;
								rblock=true;
							}
							else
							{
								if((cpu->internal[0]==0xCB)&&!(cpu->shiftstate&0x03)) // CB/ED CB is an instruction, not a shift
								{
									cpu->shiftstate|=0x01;
									cpu->block_ints=true;
									rfix=true;
								}
								else if((cpu->internal[0]==0xED)&&!(cpu->shiftstate&0x03)) // CB/ED ED is an instruction, not a shift
								{
									cpu->shiftstate=0x02; // ED may not combine
									cpu->block_ints=true;
								}
								else if((cpu->internal[0]==0xDD)&&!(cpu->shiftstate&0x03)) // CB/ED DD is an instruction, not a shift
								{
									cpu->shiftstate&=~(0x08); // FD may not combine with DD
									cpu->shiftstate|=0x04;
									cpu->block_ints=true;
									cpu->disp=false;
								}
								else if((cpu->internal[0]==0xFD)&&!(cpu->shiftstate&0x03)) // CB/ED FD is an instruction, not a shift
								{
									cpu->shiftstate&=~(0x04); // DD may not combine with FD
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
								{
									cpu->dT--;
									cpu->disp=false;
									rblock=true;
								}
							}
							if(unlikely(cpu->M&&(cpu->shiftstate&0x02)&&(cpu->ods.x==2)&&(cpu->ods.y&4)&&(cpu->ods.z&2)&&!(cpu->ods.z&4)))
							{ // IN-- and OT-- functions take an extra Tstate
								cpu->dT--;
							}
							bus->mreq=false;
							bus->m1=false;
							bus->tris=OFF;
							if((rfix||!((cpu->shiftstate&0x01)&&(cpu->shiftstate&0x0C)&&!((cpu->internal[0]==0xFD)||(cpu->internal[0]==0xDD))))&&!rblock)
							{
								bus->rfsh=true;
								bus->mreq=true;
								bus->addr=((*Intvec)<<8)+*Refresh;
								(*Refresh)++;
								if(!((*Refresh)&0x7f)) // preserve the high bit of R
									(*Refresh)^=0x80;
							}
						}
					break;
				}
			}
		break;
		case 1: // M1
			if(cpu->shiftstate&0x01) // CB
			{
				switch(cpu->ods.x)
				{
					case 0: // CB x0 == rot[y] r[z]
						if(cpu->shiftstate&0x0C) // FD/DD CB x0 == LD r[z], rot[y] (IX+d): M1=MR(4)
						{
							STEP_MR((*IHL)+((signed char)cpu->internal[1]), &cpu->internal[2]);
							if(cpu->M>1)
							{
								if(cpu->ods.y&4)
									cpu->internal[2]=op_s(cpu, cpu->internal[2]);
								else
									cpu->internal[2]=op_r(cpu, cpu->internal[2]);
								if(cpu->ods.z!=6)
									cpu->regs[tbl_r[cpu->ods.z]]=cpu->internal[2]; // H and L are /not/ IXYfied
								cpu->dT=-2;
							}
						}
						else
						{
							if(cpu->ods.z==6) // CB x0 z6 == rot[y] (HL): M1=MR(4)
							{
								STEP_MR(*HL, &cpu->internal[1]);
								if(cpu->M>1)
								{
									if(cpu->ods.y&4)
										cpu->internal[1]=op_s(cpu, cpu->internal[1]);
									else
										cpu->internal[1]=op_r(cpu, cpu->internal[1]);
									cpu->dT=-2;
								}
							}
							else // CB x0 !z6 == rot[y] r[z]: M1=IO(0)
							{
								if(cpu->ods.y&4)
									cpu->regs[tbl_r[cpu->ods.z]]=op_s(cpu, cpu->regs[tbl_r[cpu->ods.z]]);
								else
									cpu->regs[tbl_r[cpu->ods.z]]=op_r(cpu, cpu->regs[tbl_r[cpu->ods.z]]);
								cpu->M=0;
							}
						}
					break;
					case 1: // CB x1 == BIT y,r[z]
						if(cpu->shiftstate&0x0C) // FD/DD CB d x1 == BIT y,(IXY+d): M1=MR(4)
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
							if(cpu->ods.z==6) // CB x1 z6 == BIT y,(HL): M1=MR(4)
							{
								STEP_MR(*HL, &cpu->internal[1]);
								if(cpu->M>1)
								{
									bool nz=cpu->internal[1]&(1<<cpu->ods.y);
									// flags
									// SZ5H3PNC
									// *Zm1mZ0-
									// ZP: BIT == 0
									// S: BIT && y=7
									// 53: from MEMPTR (TODO: give a rat's ass about MEMPTR)	current impl: always 0
									cpu->regs[2]&=FC;
									cpu->regs[2]|=FH;
									if(!nz) cpu->regs[2]|=FZ|FP;
									else if(cpu->ods.y==7) cpu->regs[2]|=FS;
									cpu->dT=-2;
									cpu->M=0;
								}
							}
							else // CB x1 !z6 == BIT y,r[z]: M1=IO(0)
							{
								bool nz=cpu->regs[tbl_r[cpu->ods.z]]&(1<<cpu->ods.y);
								// flags
								// SZ5H3PNC
								// *Z*1*Z0-
								// ZP: BIT == 0
								// S: BIT && y=7
								// 53: from r
								cpu->regs[2]&=FC;
								cpu->regs[2]|=FH;
								cpu->regs[2]|=cpu->regs[tbl_r[cpu->ods.z]]&(F5|F3); // this is how the FUSE testsuite and some other docs say 53 work
								if(!nz) cpu->regs[2]|=FZ|FP;
								else
								{
									if(cpu->ods.y==7) cpu->regs[2]|=FS;
									// this is how The Undocumented Z80 Documented says 53 work
									/*if(cpu->ods.y==5) cpu->regs[2]|=F5;
									if(cpu->ods.y==3) cpu->regs[2]|=F3;*/
								}
								cpu->M=0;
							}
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
							if(cpu->ods.z==6) // CB x2 z6 == RES y,(HL): M1=MR(4)
							{
								STEP_MR(*HL, &cpu->internal[1]);
								if(cpu->M>1)
								{
									cpu->internal[1]&=~(1<<cpu->ods.y);
									cpu->dT=-2;
								}
							}
							else // CB x2 !z6 == RES y,r[z]: M1=IO(0)
							{
								cpu->regs[tbl_r[cpu->ods.z]]&=~(1<<cpu->ods.y);
								cpu->M=0;
							}
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
							if(cpu->ods.z==6) // CB x2 z6 == SET y,(HL): M1=MR(4)
							{
								STEP_MR(*HL, &cpu->internal[1]);
								if(cpu->M>1)
								{
									cpu->internal[1]|=(1<<cpu->ods.y);
									cpu->dT=-2;
								}
							}
							else // CB x3 !z6 == SET y,r[z]: M1=IO(0)
							{
								cpu->regs[tbl_r[cpu->ods.z]]|=(1<<cpu->ods.y);
								cpu->M=0;
							}
						}
					break;
					default:
						fprintf(stderr, ZERR1);
						errupt++;
					break;
				}
			}
			else if(cpu->shiftstate&0x02) // ED
			{
				switch(cpu->ods.x)
				{
					case 0: // ED x0x3 == LNOP
					case 3:
						cpu->M=0;
					break;
					case 1: // ED x1
						switch(cpu->ods.z)
						{
							case 0: // ED x1 z0 == IN r[y],(C): M1=PR(4)
								STEP_PR(*BC,&cpu->internal[1]);
								if(cpu->M>1)
								{
									if(cpu->ods.y!=6)
										cpu->regs[tbl_r[cpu->ods.y]]=cpu->internal[1];
									// Flags: SZ503P0-
									cpu->regs[2]&=FC;
									cpu->regs[2]|=(cpu->internal[1]&(FS|F5|F3));
									if(!cpu->internal[1]) cpu->regs[2]|=FZ;
									if(parity(cpu->internal[1])) cpu->regs[2]|=FP;
									cpu->M=0;
								}
							break;
							case 1: // ED x1 z1 == OUT (C),r[y]: M1=PW(4)
								STEP_PW(*BC,(cpu->ods.y==6)?0:cpu->regs[tbl_r[cpu->ods.y]]);
								if(cpu->M>1)
									cpu->M=0;
							break;
							case 2: // ED x1 z2
								if(!cpu->ods.q) // ED x1 z2 q0 == SBC HL,rp[p]: M1=IO(4)
								{
									if(cpu->dT>=4)
									{
										cpu->M++;
										cpu->dT=0;
									}
								}
								else // ED x1 z2 q1 == ADC HL,rp[p]: M1=IO(4)
								{
									if(cpu->dT>=4)
									{
										cpu->M++;
										cpu->dT=0;
									}
								}
							break;
							case 3: // ED x1 z3 == LD rp[p]<=>(nn): M1=ODL(3)
								STEP_OD(1);
							break;
							case 4: // ED x1 z4 == NEG: M1=IO(0)
							{
								bool h=cpu->regs[3]&0x0f;
								cpu->regs[3]=-cpu->regs[3];
								// Flags: SZ5H3V1C
								cpu->regs[2]=FN|(cpu->regs[3]&(FS|F5|F3));
								if(cpu->regs[3]==0x80) cpu->regs[2]|=FV;
								if(!cpu->regs[3]) cpu->regs[2]|=FZ;
								else cpu->regs[2]|=FC;
								if(h) cpu->regs[2]|=FH;
								cpu->M=0;
							}
							break;
							case 5: // ED x1 z5 == RETI/N: M1=SRL(3)
								STEP_SR(1);
								bus->reti=true;
							break;
							case 6: // ED x1 z6 == IM im[y]: M1=IO(0)
								cpu->intmode=tbl_im[cpu->ods.y&3];
								cpu->M=0;
							break;
							case 7: // ED x1 z7
								switch(cpu->ods.y)
								{
									case 0: // ED x1 z7 y0 == LD I,A: M1=IO(1)
										if(cpu->dT>=0)
										{
											*Intvec=cpu->regs[3];
											cpu->M=0;
											cpu->dT--;
										}
									break;
									case 1: // ED x1 z7 y1 == LD R,A: M1=IO(1)
										if(cpu->dT>=0)
										{
											*Refresh=cpu->regs[3];
											cpu->M=0;
											cpu->dT--;
										}
									break;
									case 2: // ED x1 z7 y2 == LD A,I: M1=IO(1)
										if(cpu->dT>=0)
										{
											// Flags: SZ503i0- (i = IFF2)
											cpu->regs[3]=*Intvec;
											cpu->regs[2]&=FC;
											cpu->regs[2]|=cpu->regs[3]&(FS|F5|F3);
											if(!cpu->regs[3]) cpu->regs[2]|=FZ;
											if(cpu->IFF[1]) cpu->regs[2]|=FP;
											cpu->M=0;
											cpu->dT--;
										}
									break;
									case 3: // ED x1 z7 y3 == LD A,R: M1=IO(1)
										if(cpu->dT>=0)
										{
											// Flags: SZ503i0- (i = IFF2)
											cpu->regs[3]=*Refresh;
											cpu->regs[2]&=FC;
											cpu->regs[2]|=cpu->regs[3]&(FS|F5|F3);
											if(!cpu->regs[3]) cpu->regs[2]|=FZ;
											if(cpu->IFF[1]) cpu->regs[2]|=FP;
											cpu->M=0;
											cpu->dT--;
										}
									break;
									case 4: // ED x1 z7 y4 == RRD: M1=MR(3)
										STEP_MR(*HL, &cpu->internal[1]);
										if(cpu->M>1)
										{
											unsigned char A=cpu->regs[3];
											cpu->regs[3]=(cpu->regs[3]&0xf0)|(cpu->internal[1]&0x0f);
											cpu->internal[1]=((A&0x0f)<<4)|(cpu->internal[1]>>4);
										}
									break;
									case 5: // ED x1 z7 y5 == RLD: M1=MR(3)
										STEP_MR(*HL, &cpu->internal[1]);
										if(cpu->M>1)
										{
											unsigned char A=cpu->regs[3];
											cpu->regs[3]=(cpu->regs[3]&0xf0)|(cpu->internal[1]>>4);
											cpu->internal[1]=(A&0x0f)|(cpu->internal[1]<<4);
										}
									break;
									case 6: // ED x1 z7 y6 == NOP
									case 7: // ED x1 z7 y7 == NOP
										cpu->M=0;
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
					case 2: // ED x2
						if((cpu->ods.z<4)&&(cpu->ods.y>3)) // bli[y,z]
						{
							op_bli(cpu, bus);
						}
						else // LNOP
						{
							cpu->M=0;
						}
					break;
					default: // ED x?
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
									case 1: // x0 z0 y1 == EX AF,AF': M1=IO(0)
									{
										unsigned short tmp=*AF;
										*AF=*AF_;
										*AF_=tmp;
										cpu->M=0;
									}
									break;
									case 2: // x0 z0 y2 == DJNZ d: M1=IO(1)
										cpu->M++;
										cpu->dT--;
									break;
									case 3: // x0 z0 y3 == JR d: M1=OD(3)
										STEP_OD(1);
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
									case 0: // x0 z2 p0 == LD (BC)<=>A
										if(cpu->ods.q) // x0 z2 p0 q1 == LD A,(BC): M1=MR(3)
										{
											STEP_MR(*BC, &cpu->regs[3]);
											if(cpu->M>1)
												cpu->M=0;
										}
										else // x0 z2 p0 q0 == LD (BC),A: M1=MW(3)
										{
											STEP_MW(*BC, cpu->regs[3]);
											if(cpu->M>1)
												cpu->M=0;
										}
									break;
									case 1: // x0 z2 p1 == LD (DE)<=>A
										if(cpu->ods.q) // x0 z2 p1 q1 == LD A,(DE): M1=MR(3)
										{
											STEP_MR(*DE, &cpu->regs[3]);
											if(cpu->M>1)
												cpu->M=0;
										}
										else // x0 z2 p1 q0 == LD (DE),A: M1=MW(3)
										{
											STEP_MW(*DE, cpu->regs[3]);
											if(cpu->M>1)
												cpu->M=0;
										}
									break;
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
										(*IRPP(cpu->ods.p))++;
									}
									else // x0 z3 q1 == DEC rp[p]: M1=IO(2)
									{
										(*IRPP(cpu->ods.p))--;
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
											cpu->dT=-2;
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
											cpu->dT=-2;
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
								else // LD (IXY+d),n: M1=OD(5)
								{
									if((cpu->M>1)&&(cpu->shiftstate&0x0C))
										cpu->dT-=2;
								}
							break;
							case 7: // x0 z7
								switch(cpu->ods.y)
								{
									case 0: // rotates on Accumulator: M1=IO(0)
									case 1:
									case 2:
									case 3:
										op_ra(cpu);
										cpu->M=0;
									break;
									case 4: // x0 z7 y4 == DAA: M1=IO(0)
									{
										unsigned char diff=0, l=cpu->regs[3]&0x0F, h=(cpu->regs[3]&0xF0)>>4;
										bool c,hc;
										if(cpu->regs[2]&FC)
										{
											if(l<10)
											{
												if(cpu->regs[2]&FH)
													diff=0x66;
												else
													diff=0x60;
											}
											else
												diff=0x66;
											c=true;
										}
										else
										{
											if(l<10)
											{
												if(h<10)
												{
													if(cpu->regs[2]&FH)
														diff=0x06;
													else
														diff=0;
													c=false;
												}
												else
												{
													if(cpu->regs[2]&FH)
														diff=0x66;
													else
														diff=0x60;
													c=true;
												}
											}
											else
											{
												if(h<9)
												{
													diff=0x06;
													c=false;
												}
												else
												{
													diff=0x66;
													c=true;
												}
											}
										}
										if(cpu->regs[2]&FN)
											hc=(cpu->regs[2]&FH)&&(l<6);
										else
											hc=(l>9);
										if(cpu->regs[2]&FN)
											cpu->regs[3]-=diff;
										else
											cpu->regs[3]+=diff;
										// Flags: SZ5H3P-C
										cpu->regs[2]&=FN;
										cpu->regs[2]|=cpu->regs[3]&(FS|F5|F3);
										if(c) cpu->regs[2]|=FC;
										if(hc) cpu->regs[2]|=FH;
										if(!cpu->regs[3]) cpu->regs[2]|=FZ;
										if(parity(cpu->regs[3])) cpu->regs[2]|=FP;
										cpu->M=0;
									}
									break;
									case 5: // x0 z7 y5 == CPL: M1=IO(0)
										cpu->regs[3]=~cpu->regs[3];
										cpu->regs[2]&=FS|FZ|FP|FC;
										cpu->regs[2]|=FH|FN;
										cpu->regs[2]|=cpu->regs[3]&(F5|F3);
										cpu->M=0;
									break;
									case 6: // x0 z7 y6 == SCF: M1=IO(0)
										// flags:
										// --*0*-01
										// 5,3 from A
										cpu->regs[2]&=FS|FZ|FP;
										cpu->regs[2]|=FC;
										cpu->regs[2]|=(cpu->regs[3]&(F5|F3));
										cpu->M=0;
									break;
									case 7: // x0 z7 y7 == CCF: M1=IO(0)
										// flags:
										// --***-0*
										// H as old C
										// Complement C
										// 5,3 from A
										cpu->regs[2]&=FS|FZ|FP|FC;
										cpu->regs[2]|=(cpu->regs[2]&FC)?(FC|FH):0;
										cpu->regs[2]^=FC;
										cpu->regs[2]|=(cpu->regs[3]&(F5|F3));
										cpu->M=0;
									break;
									default: // x0 z7 y?
										fprintf(stderr, ZERR3);
										errupt++;
									break;
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
							cpu->halt=true;
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
							case 0: // x3 z0 == RET cc[y]: M1=IO(1)
								cpu->dT--;
								cpu->M++;
								if(!cc(cpu->ods.y, cpu->regs[2]))
									cpu->M=0;
							break;
							case 1: // x3 z1
								switch(cpu->ods.q)
								{
									case 0: // x3 z1 q0 == POP rp2[p]: M1=SRL(3)
										STEP_SR(1);
									break;
									case 1: // x3 z1 q1
										switch(cpu->ods.p)
										{
											case 0: // x3 z1 q1 p0 == RET: M1=SRL(3)
												STEP_SR(1);
											break;
											case 1: // x3 z1 q1 p1 == EXX: M1=IO(0)
												for(int i=4;i<10;i++) // BCDEHL
												{
													unsigned char tmp=cpu->regs[i];
													cpu->regs[i]=cpu->regs[i+0x10];
													cpu->regs[i+0x10]=tmp;
												}
												cpu->M=0;
											break;
											case 2: // x3 z1 q1 p2 == JP HL(IxIy): M1=IO(0)
												*PC=*IHL; // No displacement byte; it's HL, not (HL) - in spite of mnemonics to the contrary
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
							case 2: // x3 z2 == JP cc[y],nn: M1=ODL(3)
								STEP_OD(1);
							break;
							case 3: // x3 z3
								switch(cpu->ods.y)
								{
									case 0: // x3 z3 y0 == JP nn: M1=ODL(3)
										STEP_OD(1);
									break;
									// x3 z3 y1 == CB prefix
									case 2: // x3 z3 y2 == OUT (n),A: M1=OD(3)
									case 3: // x3 z3 y3 == IN A,(n): M1=OD(3)
										STEP_OD(1);
									break;
									case 4: // x3 z3 y4 == EX (SP),HL: M1=SRL(3)
										STEP_SR(1);
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
							case 4: // x3 z4 == CALL cc[y],nn: M1=ODL(3)
								STEP_OD(1);
							break;
							case 5: // x3 z5
								switch(cpu->ods.q)
								{
									case 0: // x3 z5 q0 == PUSH rp2[p]: M1=IO(1)
										cpu->dT--;
										cpu->M++;
									break;
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
							case 7: // x3 z7 == RST y*8: M1=IO(1)
								cpu->dT--;
								cpu->M++;
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
			if(cpu->shiftstate&0x01) // CB
			{
				switch(cpu->ods.x)
				{
					case 0: // CB x0 == rot[y] r[z]
						if(cpu->shiftstate&0x0C) // FD/DD CB d x0 z6 == rot[y],(IXY+d): M2=MW(3)
						{
							STEP_MW((*IHL)+((signed char)cpu->internal[1]), cpu->internal[2]);
							if(cpu->M>2)
								cpu->M=0;
						}
						else
						{
							if(cpu->ods.z==6) // CB x0 z6 == rot[y] (HL): M2=MW(3)
							{
								STEP_MW(*HL, cpu->internal[1]);
								if(cpu->M>2)
									cpu->M=0;
							}
							else // no M2
							{
								fprintf(stderr, ZERR2);
								errupt++;
							}
						}
					break;
					case 2: // CB x2 == RES y,r[z]
					case 3: // CB x3 == SET y,r[z]
						if(cpu->shiftstate&0x0C) // FD/DD CB d x2/3 == LD r[z],RES/SET y,(IXY+d): M2=MW(3)
						{
							STEP_MW((*IHL)+((signed char)cpu->internal[1]), cpu->internal[2]);
							if(cpu->M>2)
								cpu->M=0;
						}
						else // CB x2/3 == RES/SET y,r[z]
						{
							if(cpu->ods.z==6) // CB x2/3 z6 = RES/SET y,(HL)
							{
								STEP_MW(*HL, cpu->internal[1]);
								if(cpu->M>2)
									cpu->M=0;
							}
							else // no M2
							{
								fprintf(stderr, ZERR2);
								errupt++;
							}
						}
					break;
					default:
						fprintf(stderr, ZERR1);
						errupt++;
					break;
				}
			}
			else if(cpu->shiftstate&0x02) // ED
			{
				switch(cpu->ods.x)
				{
					case 1: // ED x1
						switch(cpu->ods.z)
						{
							case 2: // ED x1 z2
								if(!cpu->ods.q) // ED x1 z2 q0 == SBC HL,rp[p]: M2=IO(3)
								{
									if(cpu->dT>=2)
									{
										op_sbc16(cpu);
										cpu->M=0;
										cpu->dT=-1;
									}
								}
								else // ED x1 z2 q1 == ADC HL,rp[p]: M2=IO(3)
								{
									if(cpu->dT>=2)
									{
										op_adc16(cpu);
										cpu->M=0;
										cpu->dT=-1;
									}
								}
							break;
							case 3: // ED x1 z3 == LD rp[p]<=>(nn): M2=ODH(3)
								STEP_OD(2);
							break;
							case 5: // ED x1 z5 == RETI/N: M2=SRH(3)
								STEP_SR(2);
								if(cpu->M>2)
								{
									cpu->IFF[0]=cpu->IFF[1];
									cpu->M=0;
									*PC=I16;
								}
							break;
							case 7:
								switch(cpu->ods.y)
								{
									case 4: // ED x1 z7 y4 == RRD: M2=IO(4)
									case 5: // ED x1 z7 y5 == RLD: M2=IO(4)
										cpu->M++;
										cpu->dT-=4;
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
					case 2: // ED x2
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
									case 3: // x0 z0 y3 == JR d: M2=IO(5)
										/* fallthrough, as it's basically like a JR cc,d where cc is true */
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
										*IRPP(cpu->ods.p)=I16;
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
							case 4: // x0 z4 == INC r[y]
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
									if(cpu->shiftstate&0xC) // DD/FD x0 z6 y6 = LD (IXY+d),n: M2=OD(3)
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
					case 2: // x2 == alu[y] A,r[z]
						if(cpu->ods.z==6) // r[z]=(HL)
						{
							if(cpu->shiftstate&0xC) // alu[y] A,(IXY+d): M2=IO(5)
							{
								cpu->dT-=5;
								cpu->M++;
							}
							else // alu[y] A,(HL); no M2
							{
								fprintf(stderr, ZERR3);
								errupt++;
							}
						}
						else // alu[y] A,r[z]; no M2
						{
							fprintf(stderr, ZERR3);
							errupt++;
						}
					break;
					case 3: // x3
						switch(cpu->ods.z)
						{
							case 0: // x3 z0 == RET cc[y]: M2=SRL(3)
								STEP_SR(1);
							break;
							case 1: // x3 z1
								switch(cpu->ods.q)
								{
									case 0: // x3 z1 q0 == POP rp2[p]: M2=SRH(3)
										STEP_SR(2);
										if(cpu->M>2)
										{
											cpu->M=0;
											*IRP2P(cpu->ods.p)=I16;
										}
									break;
									case 1: // x3 z1 q1
										switch(cpu->ods.p)
										{
											case 0: // x3 z1 q1 p0 == RET: M2=SRH(3)
												STEP_SR(2);
												if(cpu->M>2)
												{
													cpu->M=0;
													*PC=I16;
												}
											break;
											default:
												fprintf(stderr, ZERR3);
												errupt++;
											break;
										}
									break;
									default:
										fprintf(stderr, ZERR3);
										errupt++;
									break;
								}
							break;
							case 2: // x3 z2 == JP cc[y],nn: M2=ODH(3)
								STEP_OD(2);
								if(cpu->M>2)
								{
									if(cc(cpu->ods.y, cpu->regs[2]))
										*PC=I16;
									cpu->M=0;
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
									case 3: // x3 z3 y3 == IN A,(n): M2=PR(4)
										STEP_PR((cpu->regs[3]<<8)+cpu->internal[1], &cpu->regs[3]);
										if(cpu->M>2)
										{
											cpu->M=0;
											cpu->dT=-1;
										}
									break;
									case 4: // x3 z3 y4 == EX (SP),HL: M2=SRH(4)
										STEP_SR(2);
										if(cpu->M>2)
										{
											unsigned short tmp=*IHL;
											*IHL=I16;
											cpu->internal[1]=tmp&0xFF;
											cpu->internal[2]=tmp>>8;
											cpu->dT--;
										}
									break;
									default: // x3 z3 y?
										fprintf(stderr, ZERR3);
										errupt++;
									break;
								}
							break;
							case 4: // x3 z4 == CALL cc[y],nn: M2=ODH(cc?3:4)
								STEP_OD(2);
								if(cpu->M>2)
								{
									if(cc(cpu->ods.y, cpu->regs[2]))
										cpu->dT=-1;
									else
										cpu->M=0;
								}
							break;
							case 5: // x3 z5
								switch(cpu->ods.q)
								{
									case 0: // x3 z5 q0 == PUSH rp2[p]: M2=SWH(3)
										STEP_SW(cpu->regs[IRP(tbl_rp2[cpu->ods.p])+1]);
									break;
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
							case 7: // x3 z7 == RST y*8: M2=SWH(3)
								STEP_SW((*PC)>>8);
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
			if(cpu->shiftstate&0x01) // CB
			{
				fprintf(stderr, ZERR1);
				errupt++;
			}
			else if(cpu->shiftstate&0x02) // ED
			{
				switch(cpu->ods.x)
				{
					case 1: // ED x1
						switch(cpu->ods.z)
						{
							case 3: // ED x1 z3
								switch(cpu->ods.q)
								{
									case 0: // ED x1 z3 q0 == LD (nn), rp[p] : M3=MWL(3)
										STEP_MW(I16, cpu->regs[tbl_rp[cpu->ods.p]]);
									break;
									case 1: // ED x1 z3 q1 == LD rp[p], (nn) : M3=MRL(3)
										STEP_MR(I16, &cpu->regs[tbl_rp[cpu->ods.p]]);
									break;
								}
							break;
							case 7:
								switch(cpu->ods.y)
								{
									case 4: // ED x1 z7 y4 == RRD: M3=MW(3)
									case 5: // ED x1 z7 y5 == RLD: M3=MW(3)
										// Flags: SZ503P0-
										cpu->regs[2]&=FC;
										cpu->regs[2]|=cpu->regs[3]&(FS|F5|F3);
										if(!cpu->regs[3]) cpu->regs[2]|=FZ;
										if(parity(cpu->regs[3])) cpu->regs[2]|=FP;
										STEP_MW(*HL, cpu->internal[1]);
										if(cpu->M>3)
											cpu->M=0;
									break;
									default:
										fprintf(stderr, ZERR3);
										errupt++;
									break;
								}
							break;
							default: // ED x1 z?
								fprintf(stderr, ZERR2);
								errupt++;
							break;
						}
					break;
					case 2: // ED x2
						if((cpu->ods.z<4)&&(cpu->ods.y>3)) // bli[y,z]
						{
							op_bli(cpu, bus);
						}
						else // LNOP
						{
							cpu->M=0;
						}
					break;
					default: // ED x?
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
												if(cpu->M>3)
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
							case 6: // x0 z6 == LD r[y],n; M3 should only happen if DD/FD and r[y] is (HL)
								if(cpu->ods.y==6)
								{
									if(cpu->shiftstate&0xC) // DD/FD x0 z6 y6 == LD (IXY+d),n: M3=MW(3)
									{
										STEP_MW((*IHL)+((signed char)cpu->internal[1]), cpu->internal[2]);
										if(cpu->M>3)
										{
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
					case 2: // x2 == alu[y] A,r[z]
						if(cpu->ods.z==6) // r[z]=(HL)
						{
							if(cpu->shiftstate&0xC) // alu[y] A,(IXY+d): M3=MR(3)
							{
								STEP_MR(*IHL+(signed char)cpu->internal[1], &cpu->internal[2]);
								if(cpu->M>3)
								{
									op_alu(cpu, cpu->internal[2]);
									cpu->M=0;
								}
							}
							else // alu[y] A,(HL); no M3
							{
								fprintf(stderr, ZERR3);
								errupt++;
							}
						}
						else // alu[y] A,r[z]; no M3
						{
							fprintf(stderr, ZERR3);
							errupt++;
						}
					break;
					case 3: // x3
						switch(cpu->ods.z)
						{
							case 0: // x3 z0 == RET cc[y]: M3=SRH(3)
								STEP_SR(2);
								if(cpu->M>3)
								{
									*PC=I16;
									cpu->M=0;
								}
							break;
							case 3: // x3 z3
								switch(cpu->ods.y)
								{
									case 4: // x3 z3 y4 == EX (SP),HL: M3=SWH(3)
										STEP_SW(cpu->internal[2]);
									break;
									default: // x3 z3 y?
										fprintf(stderr, ZERR3);
										errupt++;
									break;
								}
							break;
							case 4: // x3 z4 == CALL cc[y],nn: cc true; M3=SWH(3)
								STEP_SW((*PC)>>8);
							break;
							case 5: // x3 z5
								switch(cpu->ods.q)
								{
									case 0: // x3 z5 q0 == PUSH rp2[p]: M3=SWL(3)
										STEP_SW(cpu->regs[IRP(tbl_rp2[cpu->ods.p])]);
										if(cpu->M>3)
											cpu->M=0;
									break;
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
							case 7: // x3 z7 == RST y*8: M3=SWL(3)
								STEP_SW(*PC);
								if(cpu->M>3)
								{
									cpu->M=0;
									(*PC)=cpu->ods.y<<3;
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
			if(cpu->shiftstate&0x01) // CB
			{
				fprintf(stderr, ZERR1);
				errupt++;
			}
			else if(cpu->shiftstate&0x02) // ED
			{
				switch(cpu->ods.x)
				{
					case 1: // ED x1
						switch(cpu->ods.z)
						{
							case 3: // ED x1 z3
								switch(cpu->ods.q)
								{
									case 0: // ED x1 z3 q0 == LD (nn), rp[p] : M4=MWH(3)
										STEP_MW(I16+1, cpu->regs[tbl_rp[cpu->ods.p]+1]);
										if(cpu->M>4)
											cpu->M=0;
									break;
									case 1: // ED x1 z3 q1 == LD rp[p], (nn) : M4=MRH(3)
										STEP_MR(I16+1, &cpu->regs[tbl_rp[cpu->ods.p]+1]);
										if(cpu->M>4)
											cpu->M=0;
									break;
								}
							break;
							default: // ED x1 z?
								fprintf(stderr, ZERR2);
								errupt++;
							break;
						}
					break;
					default: // ED x?
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
										STEP_MW((*IHL)+((signed char)cpu->internal[1]), ((cpu->ods.z&1)?op_dec8:op_inc8)(cpu, cpu->internal[2]));
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
							case 3: // x3 z3
								switch(cpu->ods.y)
								{
									case 4: // x3 z3 y4 == EX (SP),HL: M4=SWL(5)
										STEP_SW(cpu->internal[1]);
										if(cpu->M>4)
										{
											cpu->M=0;
											cpu->dT-=2;
										}
									break;
									default: // x3 z3 y?
										fprintf(stderr, ZERR3);
										errupt++;
									break;
								}
							break;
							case 4: // x3 z4 == CALL cc[y],nn: cc true; M4=SWL(3)
								STEP_SW(*PC);
								if(cpu->M>4)
								{
									*PC=I16;
									cpu->M=0;
									cpu->dT=-2;
								}
							break;
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
	cpu->nothing=max(cpu->nothing,-1-cpu->dT);
	if(oM&&!cpu->M) // when M is set to 0, shift is reset
	{
		if(bus->nmi&&!cpu->block_ints)
		{
			cpu->IFF[0]=false;
			cpu->nmiacc=true;
		}
		else if(bus->irq&&cpu->IFF[0]&&!cpu->block_ints)
		{
			cpu->IFF[0]=cpu->IFF[1]=false;
			cpu->intacc=true;
		}
		cpu->shiftstate=0;
		cpu->disp=false;
		cpu->block_ints=false;
	}
	return(errupt);
}
