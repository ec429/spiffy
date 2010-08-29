/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010
	ops - Z80 core operations
*/

// Flags... [7]SZ5H3PNC[0]
#define FS 0x80
#define FZ 0x40
#define F5 0x20
#define FH 0x10
#define F3 0x08
#define FP 0x04
#define FV FP // oVerflow and Parity are same flag
#define FN 0x02
#define FC 0x01

typedef struct
{
	unsigned char x;
	unsigned char y;
	unsigned char z;
	unsigned char p;
	unsigned char q;
}
od;

// decoding tables (registers)
unsigned char tbl_r[8];
unsigned char tbl_rp[4];
unsigned char tbl_rp2[4];

// Names/ptrs for the common regs; these tricks rely on the system being little-endian
#define PC (unsigned short int *)regs
#define AF (unsigned short int *)(regs+2)
#define BC (unsigned short int *)(regs+4)
#define DE (unsigned short int *)(regs+6)
#define HL (unsigned short int *)(regs+8)
#define Ix (unsigned short int *)(regs+10)
#define Iy (unsigned short int *)(regs+12)
#define Intvec (unsigned char *)(regs+15)
#define Refresh (unsigned char *)(regs+14)
#define SP (unsigned short int *)(regs+16)

#define I16 ((internal[2]<<8)+internal[1]) // 16 bit literal from internal registers

int parity(unsigned short int num);

void op_alu(od ods, unsigned char regs[27], unsigned char operand);
void op_add16(od ods, unsigned char regs[27], int shiftstate);
void op_adc16(unsigned char *, int, int);
void op_sbc16(unsigned char *, int, int);
