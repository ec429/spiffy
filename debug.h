/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	debug.h - debugger functions
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "z80.h"
#include "ops.h"

typedef enum
{
	DEBUGTYPE_BYTE, // b
	DEBUGTYPE_WORD, // w
	DEBUGTYPE_FLOAT, // f
	DEBUGTYPE_GRID, // 8
	DEBUGTYPE_ROW, // R
	DEBUGTYPE_ERR, // used to signal a syntax error
}
debugtype;

typedef union
{
	unsigned char b;
	uint16_t w;
	double f;
	unsigned char r[16]; // used by 8 and R
}
debugval_val;

typedef struct
{
	debugtype type;
	debugval_val val;
	unsigned char *p; // NULL if it's not an lvalue
}
debugval;

char typename(debugtype type);
void debugger_tokenise(char *line, int *drgc, char *drgv[256]);
debugval debugger_expr(FILE *f, int ec, const char *const ev[256], unsigned char *RAM, z80 *cpu);
void show_state(const unsigned char * RAM, const z80 *cpu, int Tstates, const bus_t *bus);
void debugval_display(FILE *f, debugval val);
int reg16(const char *name);

/* debugger help text */
#define h_h	"spiffy debugger: help sections\n\
h              commands list\n\
h h            this section list\n\
h p            expression evaluation\n\
h m            memory commands\n\
h v            BASIC variables\n\
h k            BASIC listing\n\
h =            register assignments\n\
h u            ULAplus state\n"

#define h_u "spiffy debugger: ULAplus state\n\
u              display ULAplus summary\n\
u xx           decode ULAplus palette entry literal xx\n\
u [xx]         decode xxth ULAplus palette entry\n\
u [xx] yy      write yy to xxth ULAplus palette entry\n\
u m xx         write xx to ULAplus mode register\n"

#define h_p "spiffy debugger: expression evaluation\n\
\tp-expressions are given in Polish Notation; that is, every operator\n\
\tis a prefix operator.  So to read a byte from IX+0x25, use\n\
\t\t.b + #IX 25\n\
\tFor full details see the debugger manual.\n\
\tSome more examples:\n\
.b 8ccc            - byte at 0x8ccc\n\
.f dead            - 5-byte float at 0xdead\n\
.w @VARS           - word at VARS\n\
.8 + .w @CHARS 118 - 8 bytes from [[CHARS]+280]\n\
= .b #DE .b #HL    - copies a byte from [HL] to [DE]\n\
.b:AY 5            - byte from AY register 5\n\
>> #A 1            - value of A, shifted right once\n\
<<< #A 3           - value of A, rotated left three times\n"

#define h_m	"spiffy debugger: memory commands\n\
\tThe m[emory] command has been superseded by p[rint], see help p\n\
\tHere are p versions of the old m commands\n\
m r xxxx       p .b xxxx\n\
m w xxxx yy    p = .b xxxx yy\n\
m lr xxxx      p .w xxxx\n\
m lw xxxx yyyy p = .w xxxx yyyy\n\
m fr xxxx      p .f xxxx\n\
m fw xxxx d    p = .f xxxx _d\n\
m 8r xxxx      p .8 xxxx\n\
m Rr xxxx      p .R xxxx\n"

#define h_v "spiffy debugger: BASIC variables\n\
v              list all variables\n\
v foo          examine (numeric) foo\n\
v a$           examine (string) a$\n\
v a 1 2 3      examine numeric array a\n\
v a$ 4         examine character array a$\n\
v a ()         examine array a rather than variable a\n\
    If the variable is numeric, the listed address is that of the 5-byte float\n\
    value; if string, the listed address is the start of the variable's\n\
    metadata (the string data starts 3 bytes later).  If it's an array, the\n\
    address of the first element of the array is given (that is, the address\n\
    when all the remaining subscripts are filled out with 1s); this is the\n\
    case regardless of whether the array is numeric or character.\n"

#define h_k "spiffy debugger: BASIC listing\n\
k              entire program listing\n\
kn             listing with float numbers\n\
k 10           display line 10 of the program\n"

#define h_eq "spiffy debugger: registers\n\
p #reg         displays the value of register reg\n\
p = #reg val   assigns val (hex) to register reg\n\
\n\
The (16-bit) registers are: PC AF BC DE HL IX IY SP AF' BC' DE' HL'\n\
8-bit reads/writes can be made as follows:\n\
 MSB LSB register\n\
  A   F     AF\n\
  B   C     BC\n\
  D   E     DE\n\
  H   L     HL\n\
  X   x     IX\n\
  Y   y     IY\n\
  I   R     IR (can't access as 16-bit)\n\
  S   P     SP\n\
  a   f     AF'\n\
  b   c     BC'\n\
  d   e     DE'\n\
  h   l     HL'\n"

#define h_cmds "spiffy debugger:\n\
n[ext]         single-step the Z80\n\
[!]1           enable/disable Tstate stepping\n\
c[ont]         continue emulation\n\
h[elp] [sect]  get debugger help (see 'h h')\n\
s[tate]        show Z80 state\n\
t[race]        trace Z80 state\n\
b[reak] xxxx   set a breakpoint\n\
!b[reak] xxxx  delete a breakpoint\n\
l[ist]         list breakpoints\n\
p[rint] ...    evaluate & print expression (see 'h p')\n\
ei             enable interrupts\n\
di             disable interrupts\n\
r[eset]        reset the Z80\n\
[!]i[nt]       set/clear INT line\n\
[!]nmi         set/clear NMI line\n\
v[ars] ...     examine BASIC variables (see 'h v')\n\
k ...          examine BASIC listing (see 'h k')\n\
y              examine system variables\n\
a[ystate]      show AY state (if AY enabled)\n\
u[laplus]      show ULAplus state (if ULA+ enabled)\n\
q[uit]         quit Spiffy\n"
