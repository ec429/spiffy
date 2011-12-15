/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-11
	ops - Z80 core operations
*/

#include "ops.h"

int parity(unsigned short int num)
{
	int i,p=1;
	for(i=0;i<16;i++)
	{
		p+=((num&(1<<i))?1:0); // It's the naive way, I know, but it's not as if we're desperate for speed...
	}
	return(p%2);
}

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

void step_od(z80 *cpu, int ernal, bus_t *bus)
{
	switch(cpu->dT)
	{
		case 0:
			bus->tris=OFF;
			bus->addr=(*PC);
		break;
		case 1:
			bus->tris=IN;
			bus->addr=(*PC);
			bus->mreq=true;
		break;
		case 2:
			if(bus->waitline)
			{
				cpu->dT--;
			}
			else
			{
				(*PC)++;
				cpu->internal[ernal]=bus->data;
				bus->tris=OFF;
				bus->addr=0;
				bus->mreq=false;
				cpu->M++;
				cpu->dT=-1;
			}
		break;
	}
}

void step_mr(z80 *cpu, unsigned short addr, unsigned char *val, bus_t *bus)
{
	switch(cpu->dT)
	{
		case 0:
		case 1: /* fallthrough */
			bus->tris=IN;
			bus->addr=addr;
			bus->mreq=true;
		break;
		case 2:
			if(bus->waitline)
			{
				cpu->dT--;
			}
			else
			{
				*val=bus->data;
				bus->tris=OFF;
				bus->mreq=false;
				cpu->dT=-1;
				cpu->M++;
			}
		break;
	}
}

void step_mw(z80 *cpu, unsigned short addr, unsigned char val, bus_t *bus)
{
	switch(cpu->dT)
	{
		case 0:
			bus->tris=OFF;
			bus->addr=addr;
			bus->mreq=true;
			bus->data=val;
		break;
		case 1:
			bus->tris=OUT;
			bus->addr=addr;
			bus->mreq=true;
			bus->data=val;
		break;
		case 2:
			if(bus->waitline)
			{
				cpu->dT--;
			}
			else
			{
				bus->tris=OFF;
				bus->mreq=false;
				bus->data=val;
				cpu->M++;
				cpu->dT=-1;
			}
		break;
	}
}

void step_pr(z80 *cpu, unsigned short addr, unsigned char *val, bus_t *bus)
{
	switch(cpu->dT)
	{
		case 0:
			bus->tris=OFF;
			bus->addr=addr;
		break;
		case 1: /* fallthrough */
		case 2:
			bus->tris=IN;
			bus->addr=addr;
			bus->iorq=true;
		break;
		case 3:
			if(bus->waitline)
			{
				cpu->dT--;
			}
			else
			{
				*val=bus->data;
				bus->tris=OFF;
				bus->iorq=false;
				cpu->M++;
				cpu->dT=-1;
			}
		break;
	}
}

void step_pw(z80 *cpu, unsigned short addr, unsigned char val, bus_t *bus)
{
	switch(cpu->dT)
	{
		case 0:
			bus->tris=OFF;
			bus->addr=addr;
			bus->iorq=false;
			bus->data=val;
		break;
		case 1: /* fallthrough */
		case 2:
			bus->tris=OUT;
			bus->addr=addr;
			bus->iorq=true;
			bus->data=val;
		break;
		case 3:
			if(bus->waitline)
			{
				cpu->dT--;
			}
			else
			{
				bus->tris=OFF;
				bus->iorq=false;
				cpu->M++;
				cpu->dT=-1;
			}
		break;
	}
}

void step_sr(z80 *cpu, int ernal, bus_t *bus)
{
	switch(cpu->dT)
	{
		case 0: /* fallthrough */
		case 1:
			bus->tris=IN;
			bus->addr=(*SP);
			bus->mreq=true;
		break;
		case 2:
			if(bus->waitline)
			{
				cpu->dT--;
			}
			else
			{
				(*SP)++;
				cpu->internal[ernal]=bus->data;
				bus->tris=OFF;
				bus->mreq=false;
				cpu->M++;
				cpu->dT=-1;
			}
		break;
	}
}

void step_sw(z80 *cpu, unsigned char val, bus_t *bus)
{
	switch(cpu->dT)
	{
		case 0:
			bus->tris=OFF;
			bus->mreq=true;
			bus->addr=--(*SP);
			bus->data=val;
		break;
		case 1:
			bus->tris=OUT;
			bus->addr=(*SP);
			bus->mreq=true;
			bus->data=val;
		break;
		case 2:
			if(bus->waitline)
			{
				cpu->dT--;
			}
			else
			{
				bus->tris=OFF;
				bus->mreq=false;
				cpu->M++;
				cpu->dT=-1;
			}
		break;
	}
}

void op_alu(z80 *cpu, unsigned char operand) // ALU[y] A,operand
{
	switch(cpu->ods.y)
	{
		case 0: // ADD A,op: A+=operand, F=(XXVX0X)=[SZ5H3V0C]
		{
			signed short int res = cpu->regs[3]+operand;
			signed char hd=(cpu->regs[3]&0x0f)+(operand&0x0f);
			cpu->regs[2]=(res&FS); // S = sign bit
			cpu->regs[2]|=((res&0xff)==0?FZ:0); // Z true if res=0
			cpu->regs[2]|=(res&(F5|F3)); // 53 cf bits 5,3 of res
			cpu->regs[2]|=(hd>0x0f?FH:0); // H true if half-carry (here be dragons)
			cpu->regs[2]|=((operand<0x80)==((signed char)res<(signed char)cpu->regs[3]))?FV:0; // V if overflow
			cpu->regs[2]|=(res>0xff?FC:0); // C if carry
			cpu->regs[3]=res;
		}
		break;
		case 1: // ADC A,op: A+=operand, F=(XXVX0X)=[SZ5H3V0C]
		{
			int C=cpu->regs[2]&FC;
			signed short int res = cpu->regs[3]+operand+C;
			signed char hd=(cpu->regs[3]&0x0f)+(operand&0x0f)+C;
			cpu->regs[2]=(res&FS); // S = sign bit
			cpu->regs[2]|=((res&0xff)==0?FZ:0); // Z true if res=0
			cpu->regs[2]|=(res&(F5|F3)); // 53 cf bits 5,3 of res
			cpu->regs[2]|=(hd>0x0f?FH:0); // H true if half-carry (here be dragons)
			cpu->regs[2]|=((operand<0x80)==((signed char)res<(signed char)cpu->regs[3]))?FV:0; // V if overflow
			cpu->regs[2]|=(res>0xff?FC:0); // C if carry
			cpu->regs[3]=res;
		}
		break;
		case 2: // SUB op: A-=operand, F=(XXVX1X)=[SZ5H3V1C]
		{
			signed short int d=cpu->regs[3]-operand;
			signed char sd=(signed char)cpu->regs[3]-(signed char)operand;
			signed char hd=(cpu->regs[3]&0x0f)-(operand&0x0f);
			cpu->regs[2]=d&FS;
			cpu->regs[2]|=(d==0?FZ:0); // Z true if d=0
			cpu->regs[2]|=(d&(F5|F3)); // 53 cf bits 5,3 of d; this is not the same as CP r (which takes them from r)
			cpu->regs[2]|=(hd<0?FH:0); // H true if half-carry (here be dragons)
			cpu->regs[2]|=((sd>(signed char)cpu->regs[3])==((signed char)operand>0)?FV:0); // V if overflow
			cpu->regs[2]|=FN; // N always true
			cpu->regs[2]|=(d<0?FC:0);
			cpu->regs[3]=d;
		}
		break;
		case 3: // SBC op: A-=operand, F=(XXVX1X)=[SZ5H3V1C]
		{
			int C=cpu->regs[2]&FC;
			signed short int d=cpu->regs[3]-operand-C;
			signed char sd=(signed char)cpu->regs[3]-(signed char)operand;
			signed char hd=(cpu->regs[3]&0x0f)-(operand&0x0f)-C;
			cpu->regs[2]=d&FS;
			cpu->regs[2]|=(d==0?FZ:0); // Z true if d=0
			cpu->regs[2]|=(d&(F5|F3)); // 53 cf bits 5,3 of d; this is not the same as CP r (which takes them from r)
			cpu->regs[2]|=(hd<0?FH:0); // H true if half-carry (here be dragons)
			cpu->regs[2]|=((sd>(signed char)cpu->regs[3])==((signed char)operand>0)?FV:0); // V if overflow
			cpu->regs[2]|=FN; // N always true
			cpu->regs[2]|=(d<0?FC:0);
			cpu->regs[3]=d;
		}
		break;
		case 4: // AND A,op: A&=operand, F=(0XPX01)=[SZ513P00]
			cpu->regs[3]&=operand;
			cpu->regs[2]=(cpu->regs[3]&FS)+(cpu->regs[3]==0?FZ:0); // SZ set according to res
			cpu->regs[2]|=FH; // H true
			cpu->regs[2]|=(cpu->regs[3]&(F5|F3)); // 53 cf bits 5,3 of A
			cpu->regs[2]|=FP*parity(cpu->regs[3]);
		break;
		case 5: // XOR r: A^=operand, F=(0XPX00)=[SZ503P00]
			cpu->regs[3]^=operand;
			cpu->regs[2]=(cpu->regs[3]&FS)+(cpu->regs[3]==0?FZ:0); // SZ set according to res
			cpu->regs[2]|=(cpu->regs[3]&(F5|F3)); // 53 cf bits 5,3 of A
			cpu->regs[2]|=FP*parity(cpu->regs[3]);
		break;
		case 6: // OR op: A|=operand, F=(0XPX00)=[SZ503P00]
			cpu->regs[3]|=operand;
			cpu->regs[2]=(cpu->regs[3]&FS)+(cpu->regs[3]==0?FZ:0); // SZ set according to res
			cpu->regs[2]|=(cpu->regs[3]&(F5|F3)); // 53 cf bits 5,3 of A
			cpu->regs[2]|=FP*parity(cpu->regs[3]);
		break;
		case 7: // CP op: Compare operand with A, F=(XXVX1X)=[SZ5H3V1C]
		{
			signed short int d=cpu->regs[3]-operand;
			signed char sd=(signed char)cpu->regs[3]-(signed char)operand;
			signed char hd=(cpu->regs[3]&0x0f)-(operand&0x0f);
			cpu->regs[2]=d&FS;
			cpu->regs[2]|=((d&0xff)==0?FZ:0); // Z true if d=0
			cpu->regs[2]|=(operand&(F5|F3)); // 53 cf bits 5,3 of r (NOT D!!!)
			cpu->regs[2]|=(hd<0?FH:0); // H true if half-carry (here be dragons)
			cpu->regs[2]|=((sd>(signed char)cpu->regs[3])==((signed char)operand>0)?FV:0); // V if overflow
			cpu->regs[2]|=FN; // N always true
			cpu->regs[2]|=(d<0?FC:0);
		}
		break;
	}
}

void op_bli(z80 *cpu, bus_t *bus)
{
	/*
	y: 4=I 5=D 6=IR 7=DR	== b0: DEC (else INC); b1: REPEAT
	z: 0=LD 1=CP 2=IN 3=OUT
		LDxx: LD (DE),(HL); DE+-; HL+-; BC--; R? BC? PC-=2.
		CPxx: CP A,(HL); HL+-; BC--; R? BC&&(A!=HL)? PC-=2.
		INxx: IN (HL), port(BC); HL+-; B--; R? B? PC-=2.
		OTxx: BC--; OUT port(BC),(HL); HL+-; R? B? PC-=2.
	*/
	switch(cpu->M)
	{
		case 1: // bli M1
			switch(cpu->ods.z)
			{
				case 0: // LDxx M1: MR(3)
				case 1: // CPxx M1: MR(3)
				case 3: // OTxx M1: MR(3)
					STEP_MR(*HL, &cpu->internal[1]);
				break;
				case 2: // INxx M1: PR(4)
					STEP_PR(*BC, &cpu->internal[1]);
				break;
			}
		break;
		case 2: // bli M2
			switch(cpu->ods.z)
			{
				case 0: // LDxx M2: MW(5)
					STEP_MW(*DE, cpu->internal[1]);
					if(cpu->M>2)
					{
						*DE=(cpu->ods.y&1)?(*DE)-1:(*DE)+1;
						*HL=(cpu->ods.y&1)?(*HL)-1:(*HL)+1;
						(*BC)--;
						cpu->dT-=2;
						int mp=cpu->internal[1]+cpu->regs[3];
						// FLAGS!
						// SZ5H3VNC
						// --*0**0-
						//	P/V set if BC not 0
                        //	5 is bit 1 of (transferred byte + A)
                        //	3 is bit 3 of (transferred byte + A)
                        cpu->regs[2]&=(FS|FZ|FC); // SZC unaffected
                        cpu->regs[2]|=(mp&2)?F5:0;
                        cpu->regs[2]|=(mp&F3);
                        cpu->regs[2]|=(*BC)?FV:0;
						if((cpu->ods.y&2) && (*BC))
						{
							// M has already been incremented
						}
						else
						{
							cpu->M=0;
						}
					}
				break;
				case 1: // CPxx M2: IO(5)
					if(cpu->dT==0)
					{
						*HL=(cpu->ods.y&1)?(*HL)-1:(*HL)+1;
						(*BC)--;
						cpu->dT=-5;
						signed short int d=cpu->regs[3]-cpu->internal[1];
						signed char hd=(cpu->regs[3]&0x0f)-(cpu->internal[1]&0x0f);
						bool half=(hd<0); // half-carry
						int mp=d-(half?1:0);
						// FLAGS!
						// SZ5H3VNC
						//	SZ*H**1-  P/V set if BC not 0
                        //	S,Z,H from (A - (HL) ) as in CP (HL)
                        //	3 is bit 3 of (A - (HL) - half)
                        //	5 is bit 1 of (A - (HL) - half)
                        cpu->regs[2]&=(FC); // C unaffected
                        cpu->regs[2]|=(mp&2)?F5:0;
                        cpu->regs[2]|=(mp&F3);
                        cpu->regs[2]|=(*BC)?FV:0;
						cpu->regs[2]|=((d>=-0x80 && d<0)?FS:0); // S true if -128<=d<0
						cpu->regs[2]|=(d==0?FZ:0); // Z true if d=0
						cpu->regs[2]|=(half?FH:0); // half-carry
						cpu->regs[2]|=FN; // N always true
						if((cpu->ods.y&2) && (*BC) && (cpu->regs[3]!=cpu->internal[1]))
						{
							cpu->M++;
						}
						else
						{
							cpu->M=0;
						}
					}
				break;
				case 2: // INxx M2: MW(3)
					STEP_MW(*HL, cpu->internal[1]);
					if(cpu->M>2)
					{
						*HL=(cpu->ods.y&1)?(*HL)-1:(*HL)+1;
						cpu->regs[5]=op_dec8(cpu, cpu->regs[5]);
						// FLAGS
						// SZ5H3VNC
						// SZ5*3***  SZ53 affected as in DEC B
						// N=internal[1].7
						// Take register C, add one to it if the instruction increases HL otherwise decrease it by one. Now, add the the value of the I/O port (read or written) to it, and the carry of this last addition is copied to the C and H flag (so C and H flag are the same).
						// C=H=carry of add((C+-1), internal[1])
						// P/V flag: parity of (((internal[1] + L) & 7) ^ B)
						cpu->regs[2]&=(FS|FZ|F5|F3);
						cpu->regs[2]|=(cpu->internal[1]&0x80)?FN:0;
						unsigned char mp=(cpu->ods.y&1)?cpu->regs[4]+1:cpu->regs[4]-1;
						unsigned short r=mp+cpu->internal[1];
						if(r>255)
							cpu->regs[2]|=(FC|FH); // crazy stuff, but that's what http://www.gaby.de/z80/z80undoc3.txt says
						if(parity((mp&7)^cpu->regs[5]))
							cpu->regs[2]|=FP;
						if((cpu->ods.y&2) && (cpu->regs[5]))
						{
							// M has already been incremented
						}
						else
						{
							cpu->M=0;
						}
					}
				break;
				case 3: // OTxx M2: PW(4)
					STEP_PW(*HL, cpu->internal[1]);
					if(cpu->M>2)
					{
						*HL=(cpu->ods.y&1)?(*HL)-1:(*HL)+1;
						cpu->regs[5]=op_dec8(cpu, cpu->regs[5]);
						// FLAGS
						// SZ5H3VNC
						// SZ5*3***  SZ53 affected as in DEC B
						// N=internal[1].7
						// C=H=carry of add(L, internal[1])
						// P/V flag: parity of (((internal[1] + L) & 7) ^ B)
						cpu->regs[2]&=(FS|FZ|F5|F3);
						cpu->regs[2]|=(cpu->internal[1]&0x80)?FN:0;
						if((unsigned short)cpu->internal[1]+(unsigned short)cpu->regs[8]>0xff) cpu->regs[2]|=(FH|FC);
						if(parity(((cpu->internal[1]+cpu->regs[8])&7)^cpu->regs[5])) cpu->regs[2]|=FP;
						if((cpu->ods.y&2) && (cpu->regs[5]))
						{
							// M has already been incremented
						}
						else
						{
							cpu->M=0;
						}
					}
				break;
			}
		break;
		case 3: // bli M3: IO(5)
			if(cpu->dT==0)
			{
				(*PC)-=2;
				cpu->dT=-5;
				cpu->M=0;
			}
		break;
	}
}

void op_add16(z80 *cpu) // ADD HL(IxIy),rp2[p]
{
	// ADD dd,ss: dd+=ss, F=(X   0?)=[  5?3 0C].  Note: It's not the same as ADC
	unsigned short int *DD = IHL;
	unsigned short int *SS = (unsigned short int *)(cpu->regs+tbl_rp[cpu->ods.p]);
	if(cpu->ods.p==2) SS=DD;
	signed long int res = (*DD)+(*SS);
	signed short int hd=((*DD)&0x0fff)+((*SS)&0x0fff);
	cpu->regs[2]&=(FS+FZ+FP); // SZP unaffected
	cpu->regs[2]|=((res&0x2800)/0x100); // 53 cf bits 13,11 of res
	cpu->regs[2]|=(hd>0x0fff?FH:0); // H true if half-carry (here be dragons)
	cpu->regs[2]|=(res>0xffff?FC:0); // C if carry
	*DD=res;
}

void op_adc16(z80 *cpu)
{
	// ADC dd,ss: dd-=ss, F=(XXVX0X)=[SZ5H3V0C]
	signed short int *DD = (signed short int *)IHL;
	signed short int *SS = (signed short int *)(cpu->regs+tbl_rp[cpu->ods.p]);
	int C=cpu->regs[2]&1;
	signed long int res = (*DD)+(*SS)+C;
	signed short int hd=((*DD)&0x0fff)+((*SS)&0x0fff)+C;
	cpu->regs[2]=((res&0x8000)?FS:0); // S high bit of res
	cpu->regs[2]|=((res&0xffff)==0?FZ:0); // Z true if res=0
	cpu->regs[2]|=((res&0x2800)/0x100); // 53 cf bits 13,11 of res
	cpu->regs[2]|=(hd>0x0fff?FH:0); // H true if half-carry in the high byte (here be dragons)
	cpu->regs[2]|=((res>0x7fff)||(res<-0x8000)?FV:0); // V if overflow (not sure about this code)
	cpu->regs[2]|=((long)(unsigned short)*DD+(long)(unsigned short)*SS+(long)C>0xffff?FC:0); // C if carry
	*DD=res&0xffff;
}

void op_sbc16(z80 *cpu)
{
	// SBC dd,ss: dd-=ss, F=(XXVX1X)=[SZ5H3V1C]
	signed short int *DD = (signed short int *)IHL;
	signed short int *SS = (signed short int *)(cpu->regs+tbl_rp[cpu->ods.p]);
	int C=cpu->regs[2]&1;
	signed long int res = (signed long int)(*DD)-(signed long int)(*SS)-(signed long int)C;
	signed short int hd=((*DD)&0x0fff)-((*SS)&0x0fff)-C;
	cpu->regs[2]=((res&0x8000)?FS:0); // S high bit of res
	cpu->regs[2]|=((res&0xffff)==0?FZ:0); // Z true if d=0
	cpu->regs[2]|=((res&0x2800)/0x100); // 53 cf bits 13,11 of res
	cpu->regs[2]|=(hd<0?FH:0); // H true if half-carry in the high byte (here be dragons)
	cpu->regs[2]|=((res>0x7fff)||(res<-0x8000)?FV:0); // V if overflow (not sure about this code)
	cpu->regs[2]|=FN; // N always true
	cpu->regs[2]|=((unsigned)*DD<(unsigned)*SS+(long)C?FC:0); // C if carry (not sure about this code either)
	*DD=res&0xffff;
}

unsigned char op_inc8(z80 *cpu, unsigned char operand)
{
	// INC r: Increment r, F=( XVX0X)=[SZ5H3V0 ]
	unsigned char src=operand+1;
	cpu->regs[2]&=FC; // retain C (Carry) flag unchanged
	cpu->regs[2]|=(src&FS); // S = Sign bit
	cpu->regs[2]|=(src==0?FZ:0); // Z = Zero flag
	cpu->regs[2]|=(src&(F5|F3)); // 53 = bits 5,3 of result
	cpu->regs[2]|=(src%0x10==0?FH:0); // H = Half-carry (Here be dragons)
	cpu->regs[2]|=(src==0x80?FV:0); // V = Overflow
	return(src);
}

unsigned char op_dec8(z80 *cpu, unsigned char operand)
{
	// DEC r: Decrement r, F=( XVX1X)=[SZ5H3V1 ]
	unsigned char src=operand-1;
	cpu->regs[2]&=FC; // retain C (Carry) flag unchanged
	cpu->regs[2]|=(src&FS); // S = Sign bit
	cpu->regs[2]|=(src==0?FZ:0); // Z = Zero flag
	cpu->regs[2]|=(src&(F5|F3)); // 53 = bits 5,3 of result
	cpu->regs[2]|=(src%0x10==0x0f?FH:0); // H = Half-carry (Here be dragons)
	cpu->regs[2]|=(src==0x7f?FV:0); // V = Overflow
	cpu->regs[2]|=FN; // N (subtraction) always set
	return(src);
}

void op_ra(z80 *cpu)
{
	// R{L|R}[C]A: Rotate {Left|Right} [Circular] Accumulator, F=--503-0C.  5 and 3 from the NEW value of A
	bool r=(cpu->ods.y&1);
	bool hi=r?cpu->regs[3]&0x01:cpu->regs[3]&0x80;
	bool c=cpu->regs[2]&FC;
	cpu->regs[3]=r?cpu->regs[3]>>1:cpu->regs[3]<<1;
	cpu->regs[2]&=FS|FZ|F5|F3|FV;
	cpu->regs[2]|=hi?FC:0;
	if(cpu->ods.y&2) // THRU Carry (9-bit)
	{
		if(c) cpu->regs[3]|=r?0x80:0x01; // old carry (RLA/RRA)
	}
	else // INTO Carry (8-bit)
	{
		if(hi) cpu->regs[3]|=r?0x80:0x01; // new carry (RLCA/RRCA)
	}
	cpu->regs[2]|=(cpu->regs[3]&(F5|F3)); // 5 and 3 from the NEW value of A
}

unsigned char op_r(z80 *cpu, unsigned char operand)
{
	// R{L|R}[C] r[z]: Rotate {Left|Right} [Circular] register, F=SZ503P0C.  5 and 3 from the result
	bool r=(cpu->ods.y&1);
	bool hi=r?operand&0x01:operand&0x80;
	bool c=cpu->regs[2]&FC;
	operand=r?operand>>1:operand<<1;
	cpu->regs[2]=hi?FC:0;
	if(cpu->ods.y&2) // THRU Carry (9-bit)
	{
		if(c) operand|=r?0x80:0x01; // old carry (RL/RR)
	}
	else // INTO Carry (8-bit)
	{
		if(hi) operand|=r?0x80:0x01; // new carry (RLC/RRC)
	}
	cpu->regs[2]|=(operand&(FS|F5|F3)); // 5 and 3 from the NEW value of r[z]
	if(parity(operand)) cpu->regs[2]|=FP;
	if(!operand) cpu->regs[2]|=FZ;
	return(operand);
}

unsigned char op_s(z80 *cpu, unsigned char operand)
{
	// S{L|R}{L|A} r[z]: Shift {Left|Right} {Logical|Arithmetic} register, F=SZ503P0C.  5 and 3 from the result
	bool r=(cpu->ods.y&1);
	bool hi=r?operand&0x01:operand&0x80;
	bool s=operand&0x80;
	operand=r?operand>>1:operand<<1;
	cpu->regs[2]=hi?FC:0;
	if(cpu->ods.y&2) // Logical
	{
		if(!r) // SLL undocumented opcode
			operand|=0x01;
	}
	else // Arithmetic
	{
		if(r&&s) // SRA sign extension
			operand|=0x80;
	}
	cpu->regs[2]|=(operand&(FS|F5|F3)); // 5 and 3 from the NEW value of r[z]
	if(parity(operand)) cpu->regs[2]|=FP;
	if(!operand) cpu->regs[2]|=FZ;
	return(operand);
}
