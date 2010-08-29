/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010
	ops - Z80 core operations
*/

#include "ops.h"

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

void op_add16(od ods, unsigned char regs[27], int shiftstate) // ADD HL(IxIy),rp2[p]
{
	// ADD dd,ss: dd+=ss, F=(X   0?)=[  5?3 0C].  Note: It's not the same as ADC
	unsigned short int *DD = (shiftstate&4)?Ix:(shiftstate&8)?Iy:HL;
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

int parity(unsigned short int num)
{
	int i,p=1;
	for(i=0;i<16;i++)
	{
		p+=((num&(1<<i))?1:0); // It's the naive way, I know, but it's not as if we're desperate for speed...
	}
	return(p%2);
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
