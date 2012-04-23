#pragma once
/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	z80.h - Z80 communication
*/

#include <stdbool.h>
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

typedef enum {TRIS_OFF,TRIS_IN,TRIS_OUT} tristate;

typedef struct
{
	unsigned char x;
	unsigned char y;
	unsigned char z;
	unsigned char p;
	unsigned char q;
}
od;

typedef enum
{
	MW,
	PR,
	PW,
	SW
}
stepper;

typedef struct
{
	unsigned char regs[26]; // Registers (in little-endian pairs); see helper_notes for details
	bool IFF[2]; // Interrupts Flip Flops
	bool block_ints; // was the last opcode an EI or other TRIS_INT-blocking opcode?
	int intmode; // Interrupt Mode
	bool disp; // have we had the displacement byte? (DD/FD CB)
	bool halt; // are we HALTed?
	int dT; // T-state within M-cycle
	int M; // note: my M-cycles do not correspond to official labelling
	unsigned char internal[3]; // Internal Z80 registers
	od ods;
	int shiftstate;	// The 'shift state' resulting from prefixes.  bits as follows: 1=CB 2=ED 4=DD 8=FD.  Valid states: CBh/1, EDh/2. DDh/4. FDh/8. DDCBh/5 and FDCBh/9.
	bool intacc; // accepted an TRIS_INTerrupt?
	bool nmiacc; // accepted an NMI?
	int nothing; // number of Tstates doing nothing
	int steps; // number of Tstates running a step_ and doing nothing else
	stepper ste; // which step_?
	int sta;
	unsigned char stv, *stp;
}
z80;

typedef struct
{
	unsigned short int addr; // Address bus (A0-A15)
	unsigned char data; // Data bus (D0-D7)
	tristate tris; // encapsulates the ¬RD and ¬WR lines
	bool iorq; // ¬IORQ line
	bool mreq; // ¬MREQ line
	bool m1; // ¬M1 line
	bool rfsh; // ¬RFSH line
	bool waitline; // ¬WAIT line
	bool clk_inhibit; // when true, hold the CLK line low (used by ULA)
	bool reset; // ¬RESET line
	bool irq; // ¬TRIS_INT line
	bool nmi; // ¬NMI line
	bool halt; // ¬HALT line
	bool reti; // was the last opcode RETI?	(some hardware detects this, eg. PIO)
	unsigned char portfe; // last byte written to port 0xFE (used by ULA; should really be part of ULA internal data)
}
bus_t;

void z80_init(void);
void z80_reset(z80 *cpu, bus_t *bus);
void bus_reset(bus_t *bus);
int z80_tstep(z80 *cpu, bus_t *bus, int errupt);
