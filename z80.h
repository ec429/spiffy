#pragma once
/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-11
	z80 - structures for Z80 communication
*/

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

typedef struct
{
	unsigned char regs[26]; // Registers (in little-endian pairs); see helper_notes for details
	bool IFF[2]; // Interrupts Flip Flops
	bool block_ints; // was the last opcode an EI or other INT-blocking opcode?
	int intmode; // Interrupt Mode
	int waitlim; // internal; max dT to allow while WAIT is active
	bool disp; // have we had the displacement byte? (DD/FD CB)
	int dT; // T-state within M-cycle
	int M; // note: my M-cycles do not correspond to official labelling
	unsigned char internal[3]; // Internal Z80 registers
	od ods;
	int shiftstate;	// The 'shift state' resulting from prefixes.  bits as follows: 1=CB 2=ED 4=DD 8=FD.  Valid states: CBh/1, EDh/2. DDh/4. FDh/8. DDCBh/5 and FDCBh/9.
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
	unsigned char portfe; // last byte written to port 0xFE (used by ULA; should really be part of ULA internal data)
}
bus_t;
