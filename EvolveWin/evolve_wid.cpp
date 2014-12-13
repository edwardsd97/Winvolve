
#include "stdafx.h"
#include "evolve_wid.h"
#include <OleAuto.h>
#include <stdio.h>
#include <math.h>

BSTR unicodestr = SysAllocStringLen(NULL, 1024);

extern evolve_state_t g_EvolveState;
extern evolve_parms_t g_EvolveParms;
extern int RIGHT_PANEL_SIZE;

COLORREF g_Colors[256];

void draw_text(HDC dc, LPRECT rect, const char *text, COLORREF color)
{
	memset(unicodestr, 0, 1024 * 2);
	::MultiByteToWideChar(CP_ACP, 0, text, -1, unicodestr, 1024);
	SetBkColor(dc, bkColor);
	SetTextColor(dc, color);
	DrawText(dc, unicodestr, -1, rect, 0);
}

void draw_creature(HDC dc, LPRECT rect, const creature_t *creature)
{
	static char letter[2] = { 0, 0 };

	SetBkColor(dc, bkColor);

	for (int i = 0; i < g_EvolveState.parms.genes; i++)
	{
		float alpha = 0.2f + ((1.0f - (float(creature->age) / float(g_EvolveState.parms.ageDeath))) * 0.8f);

		letter[0] = creature->genes[i];

		COLORREF clrBase = g_Colors[LOWER(letter[0])];
		COLORREF clr = RGB(alpha * (clrBase & 0xFF), alpha * ((clrBase & 0xFF00) >> 8), alpha * ((clrBase & 0xFF0000) >> 16));

		if (creature->genes[i] == 0)
		{
			if (creature->death == 2)
			{
				letter[0] = 'x';
				clr = RGB(128, 0, 0);
			}
			else if ( creature->death == 1 )
			{
				letter[0] = '-';
				clr = RGB(64, 64, 64);
			}
			else
			{
				letter[0] = ' ';
			}
		}
		::MultiByteToWideChar(CP_ACP, 0, letter, -1, unicodestr, GENES_MAX);
		SetTextColor(dc, clr);
		DrawText(dc, unicodestr, -1, rect, 0);
		rect->left += 10;
	}
}

void draw_button(HDC dc, LPRECT rect, button_t *button)
{
	RECT temp;

	memcpy(&temp, rect, sizeof(RECT));
	memcpy(&button->w.frameRect, rect, sizeof(RECT));

	HFONT fFontOld = (HFONT)SelectObject(dc, (HGDIOBJ)hFont);
	HBRUSH hBrushold = (HBRUSH)SelectObject(dc, (HGDIOBJ)hBackBrush);
	HPEN hPenold;

	if (button->w.focus)
		hPenold = (HPEN)SelectObject(dc, (HGDIOBJ)hBrightPen);
	else
		hPenold = (HPEN)SelectObject(dc, (HGDIOBJ)hRiverPen);

	if (button->w.active)
		FillRect(dc, rect, hBrightBrush);
	else
		FillRect(dc, rect, hBackBrush);
	MoveToEx(dc, rect->left, rect->top, NULL);
	LineTo(dc, rect->right, rect->top);
	LineTo(dc, rect->right, rect->bottom);
	LineTo(dc, rect->left, rect->bottom);
	LineTo(dc, rect->left, rect->top);

	temp.top = temp.top + (((temp.bottom - temp.top) / 2) - 10);
	temp.left = temp.left + ((temp.right - temp.left) / 2) - ((10 * strlen(button->title)) / 2);
	draw_text(dc, &temp, button->title, RGB(200, 200, 200));

	SelectObject(dc, (HGDIOBJ)fFontOld);
	SelectObject(dc, (HGDIOBJ)hBrushold);
	SelectObject(dc, (HGDIOBJ)hPenold);
}

void draw_parm_slider(HDC dc, LPRECT rect, parm_slider_t *slider)
{
	char msg[100];
	float fval;
	float fmin;
	float fmax;
	float fpct;

	HFONT fFontOld = (HFONT)SelectObject(dc, (HGDIOBJ)hFont);
	HBRUSH hBrushold = (HBRUSH)SelectObject(dc, (HGDIOBJ)hBackBrush);
	HPEN hPenold;
	int k = 2;

	memcpy(&slider->w.frameRect, rect, sizeof(RECT));

	rect->top += 20 + k;
	rect->right = rect->left + RIGHT_PANEL_SIZE - 20;
	rect->bottom = rect->top + 20;

	if (slider->type)
		fval = *((float*)(slider->data));
	else
		fval = (float)*((int*)(slider->data));
	fmax = max((float)slider->min, (float)slider->max);
	fmin = min((float)slider->min, (float)slider->max);
	fval = min(fmax, max(fmin, fval));
	fpct = (fval - fmin) / (fmax - fmin);

	RECT clear;
	memcpy(&clear, rect, sizeof(RECT));
	clear.left -= k + 1;
	clear.top -= k + 1;
	clear.right += k + 1;
	clear.bottom += k + 1;
	FillRect(dc, &clear, hBackBrush);
	if (slider->w.focus || slider->w.active)
		hPenold = (HPEN)SelectObject(dc, (HGDIOBJ)hBrightPen);
	else
		hPenold = (HPEN)SelectObject(dc, (HGDIOBJ)hRiverPen);
	MoveToEx(dc, rect->left, rect->top, NULL);
	LineTo(dc, rect->right, rect->top);
	LineTo(dc, rect->right, rect->bottom);
	LineTo(dc, rect->left, rect->bottom);
	LineTo(dc, rect->left, rect->top);
	memcpy(&slider->w.scrRect, rect, sizeof(RECT));
	int x = (int)(rect->left + ((rect->right - rect->left) * fpct));
	HGDIOBJ oldObj = SelectObject(dc, (HGDIOBJ)hBrightPen);
	for (int i = 0; i < 3; i++)
	{
		MoveToEx(dc, x + i, rect->top - k, NULL);
		LineTo(dc, x + i, rect->bottom + k);
		if (i > 0)
		{
			MoveToEx(dc, x - i, rect->top - k, NULL);
			LineTo(dc, x - i, rect->bottom + k);
		}
	}
	SelectObject(dc, oldObj);

	rect->top -= 20 + k;
	sprintf(msg, "%s:", slider->title);
	draw_text(dc, rect, msg, RGB(200, 200, 200));
	if (slider->type)
		sprintf(msg, "%4.2f", *((float*)(slider->data)));
	else
		sprintf(msg, "%4i", *((int*)(slider->data)));
	rect->right = rect->left + RIGHT_PANEL_SIZE;
	rect->left = rect->right - (strlen(msg) * 10) - 20;
	draw_text(dc, rect, msg, RGB(200, 200, 200));

	rect->top += 45;

	SelectObject(dc, (HGDIOBJ)fFontOld);
	SelectObject(dc, (HGDIOBJ)hBrushold);
	SelectObject(dc, (HGDIOBJ)hPenold);
}

void move_parm_slider(HWND hwnd, HDC dc, parm_slider_t *slider, int x)
{
	float fval;
	float fmin;
	float fmax;
	float fpct;
	RECT fill;

	if (slider->type)
		fval = *((float*)(slider->data));
	else
		fval = (float)*((int*)(slider->data));

	fmax = max((float)slider->min, (float)slider->max);
	fmin = min((float)slider->min, (float)slider->max);

	x = max(slider->w.scrRect.left, min(x, slider->w.scrRect.right));
	fpct = float(x - slider->w.scrRect.left) / float(slider->w.scrRect.right - slider->w.scrRect.left);

	fval = fmin + (fpct * (fmax - fmin));

	if (slider->type)
		*((float*)(slider->data)) = fval;
	else
		*((int*)(slider->data)) = (int)fval;

	GetClientRect(hwnd, &fill);
	g_EvolveParms.historySpecies = min(SPECIES_HIST_ROWS, (fill.bottom - ((g_EvolveParms.popRows + 1) * 20)) / 50);

	evolve_parms_update(&g_EvolveState, &g_EvolveParms);

	RECT rect;

	memcpy(&rect, &slider->w.frameRect, sizeof(RECT));

	draw_parm_slider(dc, &rect, slider);

	build_color_table();
}

COLORREF color_lerp(COLORREF a, COLORREF b, float lerp, float alpha)
{
	COLORREF clr;

	int ar = (a & 0xFF);
	int ag = ((a & 0xFF00) >> 8);
	int ab = ((a & 0xFF0000) >> 16);

	int br = (b & 0xFF);
	int bg = ((b & 0xFF00) >> 8);
	int bb = ((b & 0xFF0000) >> 16);

	int rc = (int)(float(ar + ((br - ar) * lerp)));
	int gc = (int)(float(ag + ((bg - ag) * lerp)));
	int bc = (int)(float(ab + ((bb - ab) * lerp)));

	clr = RGB(rc, gc, bc);

	return clr;
}

void build_color_table()
{
	static COLORREF color[4] =
	{
		RGB(255, 128, 128),
		RGB(128, 255, 128),
		RGB(128, 128, 255),
		RGB(255, 128, 128),
	};

	for (int i = 0; i < g_EvolveState.alphabet_size; i++)
	{
		float colorIdx = (float)i;
		float maxIdx = (float)(g_EvolveState.alphabet_size);
		float colorFloat = (colorIdx / maxIdx) * ((sizeof(color) / sizeof(color[0]) - 1));
		COLORREF clr = color_lerp(color[int(colorFloat)], color[int(colorFloat) + 1], colorFloat - floor(colorFloat), 1.0f);
		g_Colors[LOWER(g_EvolveState.parms.alphabet[i])] = clr;
	}
}

