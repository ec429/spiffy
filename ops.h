#pragma once
/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010
	ops - Z80 core operations
*/

#include <stdbool.h>
#include "z80.h"

#define STEP_OD(n)		step_od(cpu, n, bus)
#define STEP_MR(a,v)	step_mr(cpu, a, v, bus)
#define STEP_MW(a,v)	step_mw(cpu, a, v, bus)
#define STEP_PR(a,v)	step_pr(cpu, a, v, bus)
#define STEP_PW(a,v)	step_pw(cpu, a, v, bus)
#define STEP_SR(n)		step_sr(cpu, n, bus)
#define STEP_SW(v)		step_sw(cpu, v, bus)

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

// decoding tables (registers)
unsigned char tbl_r[8];
unsigned char tbl_rp[4];
unsigned char tbl_rp2[4];
// 	(other tables)
unsigned char tbl_im[4];

// Names/ptrs for the common regs; these tricks rely on the system being little-endian
#define PC (unsigned short int *)cpu->regs
#define AF (unsigned short int *)(cpu->regs+2)
#define BC (unsigned short int *)(cpu->regs+4)
#define DE (unsigned short int *)(cpu->regs+6)
#define HL (unsigned short int *)(cpu->regs+8)
#define Ix (unsigned short int *)(cpu->regs+10)
#define Iy (unsigned short int *)(cpu->regs+12)
#define Intvec (unsigned char *)(cpu->regs+15)
#define Refresh (unsigned char *)(cpu->regs+14)
#define SP (unsigned short int *)(cpu->regs+16)

#define I16 ((cpu->internal[2]<<8)+cpu->internal[1]) // 16 bit literal from internal registers
#define IHL (unsigned short int *)((cpu->shiftstate&4)?Ix:(cpu->shiftstate&8)?Iy:HL) // HL except where modified by DD/FD prefixes (pointer to word)
#define IH	((cpu->shiftstate&4)?0xb:(cpu->shiftstate&8)?0xd:9) // H, IXh or IYh (regs offset)
#define IL	((cpu->shiftstate&4)?0xa:(cpu->shiftstate&8)?0xc:8) // L, IXl or IYl (regs offset)
#define IRP(r)	((r==8)?IL:r) // IXYfy an rp offset
#define IR(r)	((r==8)?IL:(r==9)?IH:r) // IXYfy a regs offset

// Helpers
int parity(unsigned short int num);
od od_bits(unsigned char opcode);
bool cc(unsigned char which, unsigned char flags);

// M-cycles
void step_od(z80 *cpu, int ernal, bus_t *bus);
void step_mr(z80 *cpu, unsigned short addr, unsigned char *val, bus_t *bus);
void step_mw(z80 *cpu, unsigned short addr, unsigned char  val, bus_t *bus);
void step_pr(z80 *cpu, unsigned short addr, unsigned char *val, bus_t *bus);
void step_pw(z80 *cpu, unsigned short addr, unsigned char  val, bus_t *bus);
void step_sr(z80 *cpu, int ernal, bus_t *bus);
void step_sw(z80 *cpu, unsigned char val, bus_t *bus);

// Opcodes and Opcode Groups
void op_alu(z80 *cpu, unsigned char operand);
void op_bli(z80 *cpu, bus_t *bus);
void op_add16(z80 *cpu);
void op_adc16(z80 *cpu);
void op_sbc16(z80 *cpu);
unsigned char op_inc8(z80 *cpu, unsigned char operand);
unsigned char op_dec8(z80 *cpu, unsigned char operand);
void op_ra(z80 *cpu);
