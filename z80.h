#pragma once
/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-13
	z80.h - Z80 communication
*/

#include <stdbool.h>
#include "bits.h"

typedef enum {TRIS_OFF,TRIS_IN,TRIS_OUT} tristate;

typedef struct
{
	uint8_t x;
	uint8_t y;
	uint8_t z;
	uint8_t p;
	uint8_t q;
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
	uint8_t regs[26]; // Registers (in little-endian pairs); see helper_notes for details
	bool IFF[2]; // Interrupts Flip Flops
	bool block_ints; // was the last opcode an EI or other INT-blocking opcode?
	int intmode; // Interrupt Mode
	bool disp; // have we had the displacement byte? (DD/FD CB)
	bool halt; // are we HALTed?
	int dT; // T-state within M-cycle
	int M; // note: my M-cycles do not correspond to official labelling
	uint8_t internal[3]; // Internal Z80 registers
	od ods;
	int shiftstate;	// The 'shift state' resulting from prefixes.  bits as follows: 1=CB 2=ED 4=DD 8=FD.  Valid states: CBh/1, EDh/2. DDh/4. FDh/8. DDCBh/5 and FDCBh/9.
	bool intacc; // accepted an INTerrupt?
	bool nmiacc; // accepted an NMI?
	int nothing; // number of Tstates doing nothing
	int steps; // number of Tstates running a step_ and doing nothing else
	stepper ste; // which step_?
	int sta;
	uint8_t stv, *stp;
}
z80;

typedef struct
{
	uint16_t addr; // Address bus (A0-A15)
	uint8_t data; // Data bus (D0-D7)
	tristate tris; // encapsulates the !RD and !WR lines
	bool iorq; // !IORQ line
	bool mreq; // !MREQ line
	bool m1; // !M1 line
	bool rfsh; // !RFSH line
	bool waitline; // !WAIT line
	bool clk_inhibit; // when true, hold the CLK line low (used by ULA)
	bool reset; // !RESET line
	bool irq; // !INT line
	bool nmi; // !NMI line
	bool halt; // !HALT line
	bool reti; // was the last opcode RETI?	(some hardware detects this, eg. PIO)
	uint8_t portfe; // last byte written to port 0xFE (used by ULA; should really be part of ULA internal data)
	uint8_t kempbyte; // byte presented by Kempston joystick
}
bus_t;

void z80_init(void);
void z80_reset(z80 *cpu, bus_t *bus);
void bus_reset(bus_t *bus);
int z80_tstep(z80 *cpu, bus_t *bus, int errupt);
