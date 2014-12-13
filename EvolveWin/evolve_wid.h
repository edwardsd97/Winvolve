
#include "stdafx.h"
#include "evolve.h"

extern HFONT hFont;
extern HBRUSH hBackBrush;
extern HBRUSH hBrightBrush;
extern HPEN hRiverPen;
extern HPEN hBrightPen;
extern COLORREF bkColor;

#pragma warning(disable:4996) // This function or variable may be unsafe.

typedef struct widget_s
{
	RECT	frameRect;
	RECT	scrRect;
	bool	focus;
	bool	active;

} widget_t;

typedef struct parm_slider_s
{
	void	*data;
	int		min;
	int		max;
	int		type;
	int		fullRefresh;
	char	*title;

	widget_t	w;

} parm_slider_t;

typedef void(*button_func)();

typedef struct button_s
{
	char		*title;
	button_func	func;

	widget_t	w;

} button_t;

void draw_text(HDC dc, LPRECT rect, const char *text, COLORREF color);
void draw_button(HDC dc, LPRECT rect, button_t *button);
void draw_parm_slider(HDC dc, LPRECT rect, parm_slider_t *slider);
void move_parm_slider(HWND hwnd, HDC dc, parm_slider_t *slider, int x);
void draw_creature(HDC dc, LPRECT rect, const creature_t *creature);
void build_color_table();
