/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	debug.h - debugger functions
*/

#include "z80.h"
#include "ops.h"

void debugger_tokenise(char *line, int *drgc, char *drgv[256]);
void show_state(const unsigned char * RAM, const z80 *cpu, int Tstates, const bus_t *bus);
void mdisplay(unsigned char *RAM, unsigned int addr, const char *what, const char *rest);
int reg16(const char *name);

/* debugger help text */
#define h_h	"spiffy debugger: help sections\n\
h              commands list\n\
h h            this section list\n\
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

#define h_m	"spiffy debugger: memory commands\n\
\tValues xxxx and yy[yy] are hex\n\
\tYou can also use %%sysvar[+x] instead of xxxx,\n\
\t or (%%sysvar)[+x] if the sysvar is of address type\n\
\t where sysvar is any of the sysvar names from the 'y' listing\n\
\t and x is a (decimal) offset.\n\
\tYou can use *reg[+x] as well, where reg is a 16-bit register\n\
m r xxxx       read byte from memory at address xxxx\n\
m w xxxx [yy]  write byte yy or 0 to address xxxx\n\
m lr xxxx      read word from memory at address xxxx\n\
m lw xxxx yyyy write word yyyy or 0 to address xxxx\n\
m fr xxxx      read 5-byte float from address xxxx\n\
m fw xxxx d    write 5-byte float d (decimal) to address xxxx\n\
m 8r xxxx      read 8 bytes from xxxx, display as binary grid\n\
m Rr xxxx      read 16 bytes from xxxx, display as hex\n"

#define h_v "spiffy debugger: BASIC variables\n\
v              list all variables\n\
v foo          examine (numeric) foo\n\
v a$           examine (string) a$\n\
v a(1,2,3)     examine numeric array a\n\
v a$(4)        examine character array a$\n\
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
= reg          displays the value of register reg\n\
= reg val      assigns val (hex) to register reg\n\
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
= reg [val]    read/write a register (see 'h =')\n\
m[emory] ...   read/write memory (see 'h m')\n\
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
