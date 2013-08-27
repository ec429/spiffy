#pragma once
#include <stdio.h>
#include "z80.h"
#include "machine.h"

typedef struct
{
	bool memwait;
	bool iowait;
	bool t1;
	bool ulaplus_enabled;
	uint8_t ulaplus_regsel; // only used when ULAplus enabled
	uint8_t ulaplus_regs[64]; // only used when ULAplus enabled
	uint8_t ulaplus_mode; // only used when ULAplus enabled
	bool timex_enabled; // 8x1 attribute mode, *rampantly* inaccurate no doubt
	uint8_t portfffd;
}
ula_t;

typedef struct
{
	unsigned int banks;
	bool *write;
	uint8_t (*bank)[0x4000]; // 16384 bytes per bank
	unsigned int paged[4];
	bool plock; // paging locked out?
}
ram_t;

typedef struct
{
	uint8_t key;
	bool twokey;
	uint8_t row[2], col[2];
}
keymap;

unsigned int nkmaps;
keymap *kmap;

int ram_init(ram_t *ram, FILE *rom, machine m);
// for use by eg. debugger
uint8_t ram_read(const ram_t *ram, uint16_t addr);
void ram_write(ram_t *ram, uint16_t addr, uint8_t val);
uint16_t ram_read_word(const ram_t *ram, uint16_t addr);
void ram_write_word(ram_t *ram, uint16_t addr, uint16_t val);
void ram_read_bytes(const ram_t *ram, uint16_t addr, uint16_t len, uint8_t *buf);
void ram_write_bytes(ram_t *ram, uint16_t addr, uint16_t len, uint8_t *buf);

void do_ram(const ram_t *ram, bus_t *bus);
int init_keyboard(void);
void mapk(uint8_t k, bool kstate[8][5], bool down);

#define PORTFE_MIC	0x08
#define PORTFE_SPEAKER	0x10
