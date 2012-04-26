/*
	spiffy - ZX spectrum emulator
	
	Copyright Edward Cree, 2010-12
	ui.h - User Interface
*/

#include <stdbool.h>
#include <SDL.h>
#include <SDL/SDL_ttf.h>

typedef struct
{
	SDL_Rect posn;
	SDL_Surface *img;
	uint32_t col;
}
button;

typedef struct _pos
{
	int x;
	int y;
} pos;

typedef enum {JS_C, JS_S, JS_K, JS_X} js_type;

SDL_Surface * gf_init();
void ui_init(SDL_Surface *screen, button **buttons, bool edgeload, bool pause, bool printer);
void pset(SDL_Surface * screen, int x, int y, unsigned char r, unsigned char g, unsigned char b);
int line(SDL_Surface * screen, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b);
void uparrow(SDL_Surface * screen, SDL_Rect where, unsigned long col, unsigned long bcol);
void downarrow(SDL_Surface * screen, SDL_Rect where, unsigned long col, unsigned long bcol);
int dtext(SDL_Surface * scrn, int x, int y, int w, const char * text, TTF_Font * font, unsigned char r, unsigned char g, unsigned char b);
bool pos_rect(pos p, SDL_Rect r);
void drawbutton(SDL_Surface *screen, button b);
void pget(SDL_Surface * screen, int x, int y, unsigned char *r, unsigned char *g, unsigned char *b);
void ksupdate(SDL_Surface * screen, button *buttons, js_type keystick);

#define loadbutton		buttons[0]
#define edgebutton		buttons[1]
#define playbutton		buttons[2]
#define nextbutton		buttons[3]
#define stopbutton		buttons[4]
#define rewindbutton	buttons[5]
#define pausebutton		buttons[6]
#define resetbutton		buttons[7]
#define bugbutton		buttons[8]
#define aw_up			buttons[9].posn
#define aw_down			buttons[10].posn
#define sr_up			buttons[11].posn
#define sr_down			buttons[12].posn
#define recordbutton	buttons[13]
#define snapbutton		buttons[14]
#define trecbutton		buttons[15]
#define feedbutton		buttons[16]
#define jscbutton		buttons[17]
#define jssbutton		buttons[18]
#define jskbutton		buttons[19]
#define jsxbutton		buttons[20]
#define bwbutton		buttons[21]
#define scanbutton		buttons[22]
#define blurbutton		buttons[23]
#define vblurbutton		buttons[24]
#define misgbutton		buttons[25]
#define nbuttons		26
