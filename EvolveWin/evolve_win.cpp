
#include "stdafx.h"
#include "evolve.h"
#include <string.h>
#include <OleAuto.h>
#include <time.h>
#include <stdio.h>

#pragma warning(disable:4996) // This function or variable may be unsafe.

bool initialized = false;
int evolve_step = 0;
HFONT hFont = 0;
HBRUSH hBackBrush = 0;
HPEN hRiverPen = 0;
BSTR unicodestr = SysAllocStringLen(NULL, 1024);
COLORREF bkColor = RGB(32, 32, 32);

extern int g_Refresh;
extern int g_Stats;

void draw_text(HDC dc, LPRECT rect, const char *text, COLORREF color )
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
	static COLORREF color[8] =  
	{
		RGB(200, 0, 150), 
		RGB(255, 100,64),
		RGB(128, 150, 0), 
		RGB(96, 200, 0),
		RGB(0, 200, 96), 
		RGB(0, 150, 128),
		RGB(64, 100, 255),
		RGB(150, 0, 200), 
	};

	SetBkColor(dc, bkColor);

	for (int i = 0; i < GENES_LEN; i++)
	{
		letter[0] = creature->genes[i];
		if (creature->genes[i] == 0)
			letter[0] = ' ';
		::MultiByteToWideChar(CP_ACP, 0, letter, -1, unicodestr, GENES_SIZE);
		int colorIdx = letter[0] - 'a';
		if (letter[0] <= 'Z')
			colorIdx = letter[0] - 'A';
		COLORREF clr = color[colorIdx];
		float fade = 0.2f + ((1.0f - (float(creature->age) / float(AGE_DEATH))) * 0.8f);
		clr = RGB(fade * (clr & 0xFF), fade * ((clr & 0xFF00) >> 8), fade * ((clr & 0xFF0000) >> 16));
		SetTextColor(dc, clr);
		DrawText(dc, unicodestr, -1, rect, 0);
		rect->left += 10;
	}
}

void draw(HWND hwnd, int step, int refresh)
{
	int i;
	int j;

	static clock_t lastStats = 0;

	//	PAINTSTRUCT ps;
	HDC hdc;
	RECT rect;
	HFONT hFontold;
	HBRUSH hBrushold;
	HPEN hPenold;
	char msg[100];

	//hdc = BeginPaint(hwnd, &ps);
	hdc = GetDC(hwnd);

	rect.left = 10;
	rect.right = 5000;
	rect.top = 10;
	rect.bottom = 5000;

	if (hFont == 0)
	{
		hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Consolas"));

		hBackBrush = CreateSolidBrush(bkColor);
		hRiverPen = CreatePen( PS_SOLID, 2, RGB(64, 64, 96) );
	}

	hFontold = (HFONT)SelectObject(hdc, (HGDIOBJ)hFont);
	hBrushold = (HBRUSH)SelectObject(hdc, (HGDIOBJ)hBackBrush);
	hPenold = (HPEN)SelectObject(hdc, (HGDIOBJ)hRiverPen);

	RECT fill;

	GetWindowRect(hwnd, &fill);
	fill.bottom += fill.top;
	fill.right += fill.left;
	fill.top = fill.left = 0;

	static int last_col;

	if (last_col != river_col)
	{
		refresh = 1;
		last_col = river_col;
	}

	if (step == 0 || refresh)
	{
		if (refresh)
		{
			FillRect(hdc, &fill, hBackBrush);

			if (g_Stats)
			{
				rect.left = 6;
				rect.top = 20 * POP_ROWS + 20 + 20;
				draw_text(hdc, &rect, "Generations:", RGB(200, 200, 200));
				rect.left += 175;
				draw_text(hdc, &rect, "Species:", RGB(200, 200, 200));
				rect.left += 120;
				draw_text(hdc, &rect, "Extinctions:", RGB(200, 200, 200));
				rect.left += 150;
				draw_text(hdc, &rect, "Mass Extincts:", RGB(200, 200, 200));
				rect.left += 160;
				draw_text(hdc, &rect, "Predation:", RGB(200, 200, 200));
			}

			lastStats = 0;

			if (river_col)
			{
				int x = (10 + (river_col * GENES_SIZE * 10)) - 5;
				MoveToEx(hdc, x, 35, (LPPOINT)NULL);
				LineTo(hdc, x, 35 + (POP_ROWS * 20));
			}
		}

		for (i = 0; i < POP_COLS; i++)
		{
			rect.left = 10 + (i * GENES_SIZE * 10);
			rect.top = 10;
			draw_creature(hdc, &rect, &environment[i]);
		}

		for (i = 0; i < POP_ROWS; i++)
		{
			for (j = 0; j < POP_COLS; j++)
			{
				rect.left = 10 + (j * GENES_SIZE * 10);
				rect.top = 35 + (i * 20);
				draw_creature(hdc, &rect, &creatures[(i*POP_COLS) + j]);
			}
		}
	}

	i = (step % MAX_POP) / POP_COLS;
	j = (step % MAX_POP) % POP_COLS;
	rect.left = 10 + (j * GENES_SIZE * 10);
	rect.top = 35 + (i *20);
	draw_creature(hdc, &rect, &creatures[(i*POP_COLS) + j]);

	if ( g_Stats && clock() - lastStats > 250)
	{
		rect.left = 6;
		rect.top = 20 * POP_ROWS + 20 + 20;
		rect.left += 110;
		sprintf(msg, "%i", generation);
		draw_text(hdc, &rect, msg, RGB(200, 200, 200));
		rect.left += 140;
		sprintf(msg, "%i", species);
		draw_text(hdc, &rect, msg, RGB(200, 200, 200));
		rect.left += 155;
		sprintf(msg, "%i", extinctions);
		draw_text(hdc, &rect, msg, RGB(200, 200, 200));
		rect.left += 170;
		sprintf(msg, "%i", massExtinctions);
		draw_text(hdc, &rect, msg, RGB(200, 200, 200));
		rect.left += 121;
		static int lastPredation;
		if (predation > 0.0f && rebirth <= 0)
		{
			draw_text(hdc, &rect, "ON", RGB(255, 200, 200));
			if (!lastPredation)
				g_Refresh = 1;
		}
		else
		{
			draw_text(hdc, &rect, "OFF", RGB(200, 255, 200));
			if (lastPredation)
				g_Refresh = 1;
		}
		lastPredation = predation > 0.0f && rebirth <= 0;
		lastStats = clock();
	}

	SelectObject(hdc, hFontold);
	SelectObject(hdc, hBrushold);
	SelectObject(hdc, hPenold);

	ReleaseDC(hwnd, hdc);

//	EndPaint(hwnd, &ps);
}

void evolve_win_tick(HWND hwnd, int refresh)
{
	if (!initialized)
	{
		evolve_init();
		initialized = true;
	}

	int drawStep = evolve_step;

	evolve_step = evolve_simulate(evolve_step);

	draw(hwnd, drawStep, refresh);
}

