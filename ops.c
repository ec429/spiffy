/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010
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

void step_od(int *dT, unsigned char *internal, int ernal, int *M, tristate *tris, unsigned short *portno, bool *mreq, unsigned char ioval, unsigned char regs[27], bool waitline)
{
	switch(*dT)
	{
		case 0:
			*tris=OFF;
			*portno=(*PC);
		break;
		case 1:
			*tris=IN;
			*mreq=true;
		break;
		case 2:
			if(waitline)
			{
				(*dT)--;
			}
			else
			{
				(*PC)++;
				internal[ernal]=ioval;
				*tris=OFF;
				*portno=0;
				*mreq=false;
				(*M)++;
				*dT=-1;
			}
		break;
	}
}

void step_mw(unsigned short addr, unsigned char val, int *dT,int *M, tristate *tris, unsigned short *portno, bool *mreq, unsigned char *ioval, bool waitline)
{
	switch(*dT)
	{
		case 0:
			*tris=OFF;
			*portno=addr;
			*mreq=true;
			*ioval=val;
		break;
		case 1:
			*tris=OUT;
		break;
		case 2:
			if(waitline)
			{
				(*dT)--;
			}
			else
			{
				*tris=OFF;
				*portno=0;
				*mreq=false;
				(*M)++;
				*dT=-1;
			}
		break;
	}
}

void step_pw(unsigned short addr, unsigned char val, int *dT, int *M, tristate *tris, unsigned short *portno, bool *iorq, unsigned char *ioval, bool waitline)
{
	switch(*dT)
	{
		case 0:
			*tris=OFF;
			*portno=addr;
			*iorq=true;
			*ioval=val;
		break;
		case 1:
			*tris=OUT;
		break;
		case 2:
		break;
		case 3:
			if(waitline)
			{
				(*dT)--;
			}
			else
			{
				*tris=OFF;
				*portno=0;
				*iorq=false;
				(*M)++;
				*dT=-1;
			}
		break;
	}
}

void step_sr(int *dT, unsigned char *internal, int ernal, int *M, tristate *tris, unsigned short *portno, bool *mreq, unsigned char ioval, unsigned char regs[27], bool waitline)
{
	switch(*dT)
	{
		case 0:
			*tris=OFF;
			*portno=(*SP);
		break;
		case 1:
			*tris=IN;
			*mreq=true;
		break;
		case 2:
			if(waitline)
			{
				(*dT)--;
			}
			else
			{
				(*SP)++;
				internal[ernal]=ioval;
				*tris=OFF;
				*portno=0;
				*mreq=false;
				(*M)++;
				*dT=-1;
			}
		break;
	}
}

void op_alu(od ods, unsigned char regs[27], unsigned char operand) // ALU[y] A,operand
{
	switch(ods.y)
	{
		case 0: // ADD A,op: A+=operand, F=(XXVX0X)=[SZ5H3V0C]
		{
			signed short int res = regs[3]+operand;
			signed char hd=(regs[3]&0x0f)+(operand&0x0f);
			regs[2]=(res&FS); // S = sign bit
			regs[2]|=((res&0xff)==0?FZ:0); // Z true if res=0
			regs[2]|=(res&(F5|F3)); // 53 cf bits 5,3 of res
			regs[2]|=(hd>0x0f?FH:0); // H true if half-carry (here be dragons)
			regs[2]|=((regs[3]<0x80 && operand>0xff-regs[3])?FV:0); // V if overflow
			regs[2]|=(res>0xff?FC:0); // C if carry
			regs[3]=res;
		}
		break;
		case 1: // ADC A,op: A+=operand, F=(XXVX0X)=[SZ5H3V0C]
		{
			int C=regs[2]&FC;
			signed short int res = regs[3]+operand+C;
			signed char hd=(regs[3]&0x0f)+(operand&0x0f)+C;
			regs[2]=(res&FS); // S = sign bit
			regs[2]|=((res&0xff)==0?FZ:0); // Z true if res=0
			regs[2]|=(res&(F5|F3)); // 53 cf bits 5,3 of res
			regs[2]|=(hd>0x0f?FH:0); // H true if half-carry (here be dragons)
			regs[2]|=(((regs[3]<0x80 && operand>0xff-regs[3])-C)?FV:0); // V if overflow (strange rules, may not be accurately implemented in other op_ functions yet)
			regs[2]|=(res>0xff?FC:0); // C if carry
			regs[3]=res;
		}
		break;
		case 2: // SUB A,op: A-=operand, F=(XXVX1X)=[SZ5H3V1C]
		{
			signed short int d=regs[3]-operand;
			signed char hd=(regs[3]&0x0f)-(operand&0x0f);
			regs[2]=((d>=-0x80 && d<0)?FS:0); // S true if -128<=d<0
			regs[2]|=(d==0?FZ:0); // Z true if d=0
			regs[2]|=(d&(F5|F3)); // 53 cf bits 5,3 of d; this is not the same as CP r (which takes them from r)
			regs[2]|=(hd<0?FH:0); // H true if half-carry (here be dragons)
			regs[2]|=(d<-0xff?FV:0); // V if overflow
			regs[2]|=FN; // N always true
			regs[2]|=(d<0?FC:0);
			regs[3]=d;
		}
		break;
		case 3: // SBC op: A-=operand, F=(XXVX1X)=[SZ5H3V1C]
		{
			int C=regs[2]&FC;
			signed short int d=regs[3]-operand-C;
			signed char hd=(regs[3]&0x0f)-(operand&0x0f)-C;
			regs[2]=((d>=-0x80 && d<0)?FS:0); // S true if -128<=d<0
			regs[2]|=(d==0?FZ:0); // Z true if d=0
			regs[2]|=(d&(F5|F3)); // 53 cf bits 5,3 of d; this is not the same as CP r (which takes them from r)
			regs[2]|=(hd<0?FH:0); // H true if half-carry (here be dragons)
			regs[2]|=(d<-0xff?FV:0); // V if overflow
			regs[2]|=FN; // N always true
			regs[2]|=(d<0?FC:0);
			regs[3]=d;
		}
		break;
		case 4: // AND A,op: A&=operand, F=(0XPX01)=[SZ513P00]
			regs[3]&=operand;
			regs[2]=(regs[3]&FS)+(regs[3]==0?FZ:0); // SZ set according to res
			regs[2]|=FH; // H true
			regs[2]|=(regs[3]&(F5|F3)); // 53 cf bits 5,3 of A
			regs[2]|=FP*parity(regs[3]);
		break;
		case 5: // XOR r: A^=operand, F=(0XPX00)=[SZ503P00]
			regs[3]^=operand;
			regs[2]=(regs[3]&FS)+(regs[3]==0?FZ:0); // SZ set according to res
			regs[2]|=(regs[3]&(F5|F3)); // 53 cf bits 5,3 of A
			regs[2]|=FP*parity(regs[3]);
		break;
		case 6: // OR op: A|=operand, F=(0XPX00)=[SZ503P00]
			regs[3]|=operand;
			regs[2]=(regs[3]&FS)+(regs[3]==0?FZ:0); // SZ set according to res
			regs[2]|=(regs[3]&(F5|F3)); // 53 cf bits 5,3 of A
			regs[2]|=FP*parity(regs[3]);
		break;
		case 7: // // CP op: Compare operand with A, F=(XXVX1X)=[SZ5H3V1C]
		{
			signed short int d=regs[3]-operand;
			signed char hd=(regs[3]&0x0f)-(operand&0x0f);
			regs[2]=((d>=-0x80 && d<0)?FS:0); // S true if -128<=d<0
			regs[2]|=(d==0?FZ:0); // Z true if d=0
			regs[2]|=(operand&(F5|F3)); // 53 cf bits 5,3 of r (NOT D!!!)
			regs[2]|=(hd<0?FH:0); // H true if half-carry (here be dragons)
			regs[2]|=(d<-0xff?FV:0); // V if overflow
			regs[2]|=FN; // N always true
			regs[2]|=(d<0?FC:0);
		}
		break;
	}
}

void op_bli(od ods, unsigned char regs[27], int *dT, unsigned char *internal, int *M, tristate *tris, unsigned short *portno, bool *mreq, bool *iorq, unsigned char *ioval, bool waitline)
{
	/*
	y: 4=I 5=D 6=IR 7=DR	== b0: DEC (else INC); b1: REPEAT
	z: 0=LD 1=CP 2=IN 3=OUT
		LDxx: LD (DE),(HL); DE+-; HL+-; BC--; R? BC? PC-=2.
		CPxx: CP A,(HL); HL+-; BC--; R? BC&&(A!=HL)? PC-=2.
		INxx: IN (HL), port(BC); HL+-; B--; R? B? PC-=2.
		OTxx: BC--; OUT port(BC),(HL); HL+-; R? B? PC-=2.
	*/
	switch(*M)
	{
		case 1: // bli M1
			switch(ods.z)
			{
				case 0: // LDxx M1: MR(3)
				case 1: // CPxx M1: MR(3)
				case 3: // OTxx M1: MR(3)
					switch(*dT)
					{
						case 0:
							*portno=*HL;
						break;
						case 1:
							*tris=IN;
							*mreq=true;
						break;
						case 2:
							internal[1]=*ioval;
							*tris=OFF;
							*mreq=false;
							*portno=0;
							*dT=-1;
							(*M)++;
						break;
					}
				break;
				case 2: // INxx M1: PR(4)
					switch(*dT)
					{
						case 0:
							*portno=*BC;
						break;
						case 1:
							*tris=IN;
							*iorq=true;
						break;
						case 2:
							internal[1]=*ioval;
							*tris=OFF;
							*iorq=false;
							*portno=0;
							*dT=-2;
							(*M)++;
						break;
					}
				break;
			}
		break;
		case 2: // bli M2
			switch(ods.z)
			{
				case 0: // LDxx M2: MW(5)
					switch(*dT)
					{
						case 0:
							*portno=*DE;
						break;
						case 1:
							*tris=OUT;
							*mreq=true;
							*ioval=internal[1];
						break;
						case 2:
							*tris=OFF;
							*mreq=false;
							*portno=0;
							*DE=(ods.y&1)?(*DE)-1:(*DE)+1;
							*HL=(ods.y&1)?(*HL)-1:(*HL)+1;
							(*BC)--;
							*dT=-3;
							int mp=internal[1]+regs[3];
							// FLAGS!
							// SZ5H3VNC
							// --*0**0-
							//	P/V set if BC not 0
                            //	5 is bit 1 of (transferred byte + A)
                            //	3 is bit 3 of (transferred byte + A)
                            regs[2]&=(FS|FZ|FC); // SZC unaffected
                            regs[2]|=(mp&2)?F5:0;
                            regs[2]|=(mp&F3);
                            regs[2]|=(*BC)?FV:0;
							if((ods.y&2) && (*BC))
							{
								(*M)++;
							}
							else
							{
								*M=0;
							}
						break;
					}
				break;
				case 1: // CPxx M2: IO(5)
					if(*dT==0)
					{
						*HL=(ods.y&1)?(*HL)-1:(*HL)+1;
						(*BC)--;
						*dT=-5;
						signed short int d=regs[3]-internal[1];
						signed char hd=(regs[3]&0x0f)-(internal[1]&0x0f);
						bool half=(hd<0); // half-carry
						int mp=d-(half?1:0);
						// FLAGS!
						// SZ5H3VNC
						//	SZ*H**1-  P/V set if BC not 0
                        //	S,Z,H from (A - (HL) ) as in CP (HL)
                        //	3 is bit 3 of (A - (HL) - half)
                        //	5 is bit 1 of (A - (HL) - half)
                        regs[2]&=(FC); // C unaffected
                        regs[2]|=(mp&2)?F5:0;
                        regs[2]|=(mp&F3);
                        regs[2]|=(*BC)?FV:0;
						regs[2]|=((d>=-0x80 && d<0)?FS:0); // S true if -128<=d<0
						regs[2]|=(d==0?FZ:0); // Z true if d=0
						regs[2]|=(half?FH:0); // half-carry
						regs[2]|=FN; // N always true
						if((ods.y&2) && (*BC) && (regs[3]!=internal[1]))
						{
							(*M)++;
						}
						else
						{
							*M=0;
						}
					}
				break;
				case 2: // INxx M2: MW(3)
					step_mw(*HL, internal[1], dT, M, tris, portno, mreq, ioval, waitline);
					if((*M)>2)
					{
						*HL=(ods.y&1)?(*HL)-1:(*HL)+1;
						regs[5]=op_dec8(regs, regs[5]);
						// FLAGS
						// SZ5H3VNC
						// SZ5*3***  SZ53 affected as in DEC B
						// N=internal[1].7
						// Take register C, add one to it if the instruction increases HL otherwise decrease it by one. Now, add the the value of the I/O port (read or written) to it, and the carry of this last addition is copied to the C and H flag (so C and H flag are the same).
						// C=H=carry of add((C+-1), internal[1])
						// P/V flag: weirdness!
						regs[2]&=(FS|FZ|F5|F3);
						regs[2]|=(internal[1]&0x80)?FN:0;
						unsigned char mp=(ods.y&1)?regs[4]+1:regs[4]-1;
						int r=mp+internal[1];
						if((r>0x7f)||(r<-0x80))
							regs[2]|=(FC|FH); // crazy stuff, but that's what http://www.gaby.de/z80/z80undoc3.txt says
						// The ludicrously INSANE P/V flag weirdness, based on http://www.gaby.de/z80/z80undoc3.txt
						bool bits1[]={0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1};
						bool bits2[]={0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0};
						bool temp1=(ods.y&1?bits1:bits2)[((regs[4]&3)<<2)|(internal[0]&3)];
						bool temp2=(regs[5]&0xF)?((regs[5]&1)||((regs[5]&4)&&!(regs[5]&2))):((regs[5]&0x10)||((regs[5]&0x40)&&!(regs[5]&0x20)));
						if(parity(regs[5]))
							temp2=!temp2;
						regs[2]|=((temp1?1:0+temp2?1:0+(regs[4]>>2)+(internal[1]>>2))&1)?FP:0;
						if((ods.y&2) && (regs[5]))
						{
							// *M has already been incremented
						}
						else
						{
							*M=0;
						}
					}
				break;
				case 3: // OTxx M2: PW(4)
					step_pw(*HL, internal[1], dT, M, tris, portno, iorq, ioval, waitline);
					if((*M)>2)
					{
						*HL=(ods.y&1)?(*HL)-1:(*HL)+1;
						regs[5]=op_dec8(regs, regs[5]);
						// FLAGS
						// SZ5H3VNC
						// SZ5*3***  SZ53 affected as in DEC B
						// N=internal[1].7
						// Take register C, add one to it if the instruction increases HL otherwise decrease it by one. Now, add the the value of the I/O port (read or written) to it, and the carry of this last addition is copied to the C and H flag (so C and H flag are the same).
						// C=H=carry of add((C+-1), internal[1])
						// P/V flag: weirdness!
						regs[2]&=(FS|FZ|F5|F3);
						regs[2]|=(internal[1]&0x80)?FN:0;
						unsigned char mp=(ods.y&1)?regs[4]+1:regs[4]-1;
						int r=mp+internal[1];
						if((r>0x7f)||(r<-0x80))
							regs[2]|=(FC|FH); // crazy stuff, but that's what http://www.gaby.de/z80/z80undoc3.txt says
						// The ludicrously INSANE P/V flag weirdness, based on http://www.gaby.de/z80/z80undoc3.txt
						bool bits1[]={0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1};
						bool bits2[]={0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0};
						bool temp1=(ods.y&1?bits1:bits2)[((regs[4]&3)<<2)|(internal[0]&3)];
						bool temp2=(regs[5]&0xF)?((regs[5]&1)||((regs[5]&4)&&!(regs[5]&2))):((regs[5]&0x10)||((regs[5]&0x40)&&!(regs[5]&0x20)));
						if(parity(regs[5]))
							temp2=!temp2;
						regs[2]|=((temp1?1:0+temp2?1:0+(regs[4]>>2)+(internal[1]>>2))&1)?FP:0;
						if((ods.y&2) && (regs[5]))
						{
							// *M has already been incremented
						}
						else
						{
							*M=0;
						}
					}
				break;
			}
		break;
		case 3: // bli M3: IO(5)
			if(*dT==0)
			{
				(*PC)-=2;
				*dT=-5;
				*M=0;
			}
		break;
	}
}

void op_add16(od ods, unsigned char regs[27], int shiftstate) // ADD HL(IxIy),rp2[p]
{
	// ADD dd,ss: dd+=ss, F=(X   0?)=[  5?3 0C].  Note: It's not the same as ADC
	unsigned short int *DD = IHL;
	unsigned short int *SS = (unsigned short int *)(regs+tbl_rp2[ods.p]);
	if(ods.p==2) SS=DD;
	signed long int res = (*DD)+(*SS);
	signed short int hd=((*DD)&0x0fff)+((*SS)&0x0fff);
	regs[2]&=(FS+FZ+FP); // SZP unaffected
	regs[2]|=((res&0x2800)/0x100); // 53 cf bits 13,11 of res
	regs[2]|=(hd>0x0fff?FH:0); // H true if half-carry (here be dragons)
	regs[2]|=(res>0xffff?FC:0); // C if carry
	*DD=res;
}

void op_adc16(unsigned char * regs, int dest, int src)
{
	// ADC dd,ss: dd-=ss, F=(XXVX0X)=[SZ5H3V0C]
	signed short int *DD = (signed short int *)(regs+dest);
	signed short int *SS = (signed short int *)(regs+src);
	int C=regs[2]&1;
	signed long int res = (*DD)+(*SS)+C;
	signed short int hd=((*DD)&0x0fff)+((*SS)&0x0fff)+C;
	regs[2]=((res&0x8000)?FS:0); // S high bit of res
	regs[2]|=((res&0xffff)==0?FZ:0); // Z true if res=0
	regs[2]|=((res&0x2800)/0x100); // 53 cf bits 13,11 of res
	regs[2]|=(hd>0x0fff?FH:0); // H true if half-carry in the high byte (here be dragons)
	regs[2]|=((res>0x7fff)||(res<-0x8000)?FV:0); // V if overflow (not sure about this code)
	regs[2]|=((unsigned long)*DD+(unsigned long)*SS>0xffff?FC:0); // C if carry (not sure about this code either)
	*DD=res&0xffff;
}

void op_sbc16(unsigned char * regs, int dest, int src)
{
	// SBC dd,ss: dd-=ss, F=(XXVX1X)=[SZ5H3V1C]
	signed short int *DD = (signed short int *)(regs+dest);
	signed short int *SS = (signed short int *)(regs+src);
	int C=regs[2]&1;
	signed long int res = (*DD)-(*SS)-C;
	signed short int hd=((*DD)&0x0fff)-((*SS)&0x0fff)-C;
	regs[2]=((res>=-0x8000 && res<0)?FS:0); // S true if -0x8000<=result<0
	regs[2]|=((res&0xffff)==0?FZ:0); // Z true if d=0
	regs[2]|=((res&0x2800)/0x100); // 53 cf bits 13,11 of res
	regs[2]|=(hd<0?FH:0); // H true if half-carry in the high byte (here be dragons)
	regs[2]|=((res>0x7fff)||(res<-0x8000)?FV:0); // V if overflow (not sure about this code)
	regs[2]|=FN; // N always true
	regs[2]|=((unsigned)*DD<(unsigned)*SS?FC:0); // C if carry (not sure about this code either)
	*DD=res&0xffff;
}

unsigned char op_inc8(unsigned char * regs, unsigned char operand)
{
	// INC r: Increment r, F=( XVX0X)=[SZ5H3V0 ]
	unsigned char src=operand+1;
	regs[2]&=FC; // retain C (Carry) flag unchanged
	regs[2]|=(src&FS); // S = Sign bit
	regs[2]|=(src==0?FZ:0); // Z = Zero flag
	regs[2]|=(src&(F5|F3)); // 53 = bits 5,3 of result
	regs[2]|=(src%0x10==0?FH:0); // H = Half-carry (Here be dragons)
	regs[2]|=(src==0x80?FV:0); // V = Overflow
	return(src);
}

unsigned char op_dec8(unsigned char * regs, unsigned char operand)
{
	// DEC r: Decrement r, F=( XVX1X)=[SZ5H3V1 ]
	unsigned char src=operand-1;
	regs[2]&=FC; // retain C (Carry) flag unchanged
	regs[2]|=(src&FS); // S = Sign bit
	regs[2]|=(src==0?FZ:0); // Z = Zero flag
	regs[2]|=(src&(F5|F3)); // 53 = bits 5,3 of result
	regs[2]|=(src%0x10==0x0f?FH:0); // H = Half-carry (Here be dragons)
	regs[2]|=(src==0x7f?FV:0); // V = Overflow
	regs[2]|=FN; // N (subtraction) always set
	return(src);
}
