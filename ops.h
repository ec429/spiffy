#pragma once
/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	ops - Z80 core operations
*/

#include <stdbool.h>
#include "z80.h"

// M-cycle call wrappers
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
uint8_t tbl_r[8];
uint8_t tbl_rp[4];
uint8_t tbl_rp2[4];
// 	(other tables)
uint8_t tbl_im[4];

// Names/ptrs for the common regs; these tricks rely on the system being little-endian
#define AREG	cpu->regs[3]
#define FREG	cpu->regs[2]
#define BREG	cpu->regs[5]
#define CREG	cpu->regs[4]
#define DREG	cpu->regs[7]
#define EREG	cpu->regs[6]
#define HREG	cpu->regs[9]
#define LREG	cpu->regs[8]
#define aREG	cpu->regs[19]
#define fREG	cpu->regs[18]
#define bREG	cpu->regs[21]
#define cREG	cpu->regs[20]
#define dREG	cpu->regs[23]
#define eREG	cpu->regs[22]
#define hREG	cpu->regs[25]
#define lREG	cpu->regs[24]
#define PC (uint16_t *)cpu->regs
#define AF (uint16_t *)(cpu->regs+2)
#define BC (uint16_t *)(cpu->regs+4)
#define DE (uint16_t *)(cpu->regs+6)
#define HL (uint16_t *)(cpu->regs+8)
#define Ix (uint16_t *)(cpu->regs+10)
#define Iy (uint16_t *)(cpu->regs+12)
#define Intvec (uint8_t *)(cpu->regs+15)
#define Refresh (uint8_t *)(cpu->regs+14)
#define SP (uint16_t *)(cpu->regs+16)
#define AF_ (uint16_t *)(cpu->regs+18)
#define BC_ (uint16_t *)(cpu->regs+20)
#define DE_ (uint16_t *)(cpu->regs+22)
#define HL_ (uint16_t *)(cpu->regs+24)

#define I16 ((cpu->internal[2]<<8)+cpu->internal[1]) // 16 bit literal from internal registers
#define IHL (uint16_t *)((cpu->shiftstate&4)?Ix:(cpu->shiftstate&8)?Iy:HL) // HL except where modified by DD/FD prefixes (pointer to word)
#define IH	((cpu->shiftstate&4)?0xb:(cpu->shiftstate&8)?0xd:9) // H, IXh or IYh (regs offset)
#define IL	((cpu->shiftstate&4)?0xa:(cpu->shiftstate&8)?0xc:8) // L, IXl or IYl (regs offset)
#define IRP(r)	((r==8)?IL:r) // IXYfy an rp offset
#define IR(r)	((r==8)?IL:(r==9)?IH:r) // IXYfy a regs offset
#define IRPP(p)		(uint16_t *)(cpu->regs+IRP(tbl_rp[p])) // make a pointer to a 16-bit value from IRP register
#define IRP2P(p)	(uint16_t *)(cpu->regs+IRP(tbl_rp2[p])) // make a pointer to a 16-bit value from IRP register

// Helpers
int parity(uint16_t num);
od od_bits(uint8_t opcode);
bool cc(uint8_t which, uint8_t flags);

// M-cycle bus sequencers
void step_od(z80 *cpu, int ernal, bus_t *bus);
void step_mr(z80 *cpu, uint16_t addr, uint8_t *val, bus_t *bus);
void step_mw(z80 *cpu, uint16_t addr, uint8_t  val, bus_t *bus);
void step_pr(z80 *cpu, uint16_t addr, uint8_t *val, bus_t *bus);
void step_pw(z80 *cpu, uint16_t addr, uint8_t  val, bus_t *bus);
void step_sr(z80 *cpu, int ernal, bus_t *bus);
void step_sw(z80 *cpu, uint8_t val, bus_t *bus);

// Opcodes and Opcode Groups
void op_alu(z80 *cpu, uint8_t operand);
void op_bli(z80 *cpu, bus_t *bus);
void op_add16(z80 *cpu);
void op_adc16(z80 *cpu);
void op_sbc16(z80 *cpu);
uint8_t op_inc8(z80 *cpu, uint8_t operand);
uint8_t op_dec8(z80 *cpu, uint8_t operand);
void op_ra(z80 *cpu);
uint8_t op_r(z80 *cpu, uint8_t operand);
uint8_t op_s(z80 *cpu, uint8_t operand);
