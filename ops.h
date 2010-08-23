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
unsigned char tbl_hl[3];
unsigned short int *tbl_phl[3];

int parity(unsigned short int num);

void op_alu(od ods, unsigned char regs[27], unsigned char operand);
