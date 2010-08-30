/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010
	ops - Z80 core operations
*/

#include <stdbool.h>

#define STEP_OD(n)		step_od(&dT, internal, n, &M, &tris, &portno, &mreq, ioval, regs, waitline)
#define STEP_MW(a,v)	step_mw(a, v, &dT, &M, &tris, &portno, &mreq, &ioval, waitline)
#define STEP_PW(a,v)	step_pw(a, v, &dT, &M, &tris, &portno, &iorq, &ioval, waitline)
#define STEP_SR(n)		step_sr(&dT, internal, n, &M, &tris, &portno, &mreq, ioval, regs, waitline)

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

typedef enum {OFF,IN,OUT} tristate;

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
// 	(other tables)
unsigned char tbl_im[4];

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
#define IHL (unsigned short int *)((shiftstate&4)?Ix:(shiftstate&8)?Iy:HL) // HL except where modified by DD/FD prefixes (pointer to word)
#define IH	((shiftstate&4)?0xb:(shiftstate&8)?0xd:9) // H, IXh or IYh (regs offset)
#define IL	((shiftstate&4)?0xa:(shiftstate&8)?0xc:8) // L, IXl or IYl (regs offset)
#define IRP(r)	((r==8)?IL:r) // IXYfy an rp offset
#define IR(r)	((r==8)?IL:(r==9)?IH:r) // IXYfy a regs offset

// Helpers
int parity(unsigned short int num);
od od_bits(unsigned char opcode);
bool cc(unsigned char which, unsigned char flags);

// M-cycles
void step_od(int *dT, unsigned char *internal, int ernal, int *M, tristate *tris, unsigned short *portno, bool *mreq, unsigned char ioval, unsigned char regs[27], bool waitline);
void step_mw(unsigned short addr, unsigned char val, int *dT, int *M, tristate *tris, unsigned short *portno, bool *mreq, unsigned char *ioval, bool waitline);
void step_pw(unsigned short addr, unsigned char val, int *dT, int *M, tristate *tris, unsigned short *portno, bool *iorq, unsigned char *ioval, bool waitline);
void step_sr(int *dT, unsigned char *internal, int ernal, int *M, tristate *tris, unsigned short *portno, bool *mreq, unsigned char ioval, unsigned char regs[27], bool waitline);

// Opcodes and Opcode Groups
void op_alu(od ods, unsigned char regs[27], unsigned char operand);
void op_bli(od ods, unsigned char regs[27], int *dT, unsigned char *internal, int *M, tristate *tris, unsigned short *portno, bool *mreq, bool *iorq, unsigned char *ioval, bool waitline);
void op_add16(od ods, unsigned char regs[27], int shiftstate);
void op_adc16(unsigned char *, int, int);
void op_sbc16(unsigned char *, int, int);
unsigned char op_inc8(unsigned char * regs, unsigned char operand);
unsigned char op_dec8(unsigned char * regs, unsigned char operand);
