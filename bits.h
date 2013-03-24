#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	char *buf; // string data buffer (may contain embedded NULs)
	size_t l; // size of RAM allocation
	size_t i; // length of string data
	// invariant: i<l
}
string;

#ifndef min
#define min(a,b)	((a)<(b)?(a):(b))
#endif /* min */
#ifndef max
#define max(a,b)	((a)>(b)?(a):(b))
#endif /* max */

/* branch predictor hints - GCC specific */
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define peek16(a)	(RAM[(a)]|(RAM[((a)+1)&0xffff]<<8))
#define poke16(a,v)	(RAM[(a)]=(v),RAM[((a)+1)&0xffff]=((v)>>8))

char *fgetl(FILE *); // gets a line of string data; returns a malloc-like pointer
char *finpl(FILE *); // gets a line of string data; returns a malloc-like pointer
char *slurp(FILE *); // gets a file of string data; returns a malloc-like pointer
string sslurp(FILE *fp); // gets a file of string data possibly containing NULs; return contains a malloc-like pointer
string init_string(void); // initialises a string buffer in heap
string null_string(void); // returns a null string (no allocation)
string make_string(const char *str); // initialises a string buffer in heap, with initial contents copied from str
void append_char(string *s, char c); // adds a character to a string buffer in heap (and realloc()s if needed)
void append_str(string *s, const char *str); // adds a cstring to a string buffer in heap (and realloc()s if needed)
void append_string(string *s, const string t); // adds a string to a string buffer in heap (and realloc()s if needed)
void free_string(string *s); // frees a string (is just free(s->buf); really)

FILE *configopen(const char *name, const char *mode);
