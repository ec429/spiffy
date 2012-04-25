#include "SDL.h"
#include "bits.h"

SDL_Surface *pbm_string(string s);

int pbm_putheader(FILE *f, unsigned int w, unsigned int h); // if h==0, the height field will be padded with spaces for filling in later.  Returns byte offset of height field
