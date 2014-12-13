
#include "stdafx.h"
#include "evolve.h"
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <math.h>
#include "evolve_wid.h"

bool initialized = false;

HFONT hFont = 0;
HBRUSH hBackBrush = 0;
HBRUSH hBrightBrush = 0;
HPEN hRiverPen = 0;
HPEN hBrightPen = 0;
COLORREF bkColor = RGB(32, 32, 32);

HDC hdcMem;
RECT hdcMemRect;
HBITMAP hbmMem;
HBITMAP hbmOld;

const char *SAVE_NAME = "winvolve.sav";
const int SAVE_VER = 3;
button_t *g_buttonClicking = NULL;
parm_slider_t *g_sliderEditing = NULL;
widget_t *g_Hovering = NULL;

extern int g_Refresh;
extern int g_Stats;
extern evolve_state_t g_EvolveState;

void resize(HWND hWnd = NULL);

int RIGHT_PANEL_SIZE = 230;

const char *g_statNames[] =
{
	"Generations:",			// ES_GENERATIONS
	"Species Now:",			// ES_SPECIES_NOW,
	"Species Ever:",		// ES_SPECIES_EVER
	"Species Max Age:",		// ES_SPECIES_MAX_AGE,
	"Extinctions:",			// ES_EXTINCTIONS,
	"Mass Extinctions:",	// ES_EXTINCTIONS_MASS,
};

evolve_parms_t g_EvolveParms;

parm_slider_t g_parmSliders[11] =
{
	{ &g_EvolveParms.predationLevel, 0, 1, 1, 0, "Predation" },
	{ &g_EvolveParms.procreationLevel, 0, 1, 1, 0, "Procreation" },
	{ &g_EvolveParms.speciesMatch, 0, 1, 1, 0, "Species Match Req" },
	{ &g_EvolveParms.ageDeath, 1, 20, 0, 0, "Age of Death" },
	{ &g_EvolveParms.ageMature, 0, 19, 0, 0, "Age of Maturity" },
	{ &g_EvolveParms.rebirthGenerations, 0, 100, 0, 0, "Rebirth Generations" },
	{ &g_EvolveParms.speciesNew, 1, 50, 0, 0, "Min Species Pop" },
	{ &g_EvolveParms.envChangeRate, 1, 1000, 0, 0, "Enviro Change Rate" },
	{ &g_EvolveParms.genes, 1, 32, 0, 1, "Gene Count (*)" },
	{ &g_EvolveParms.popRows, 1, 32, 0, 1, "Population Rows (*)" },
	{ &g_EvolveParms.popCols, 1, 32, 0, 1, "Population Cols (*)" },

//	char	alphabet[MAX_ALPHABET];	// Genome alphabet

};

void click_defaults()
{
	evolve_parms_default(&g_EvolveParms);
	evolve_init(&g_EvolveState, &g_EvolveParms);
	build_color_table();
	g_Refresh = 1;
	resize();
}

void click_predation()
{
	if (g_EvolveState.predation > 0.0f)
		g_EvolveState.predation = 0.0f;
	else
		g_EvolveState.predation = g_EvolveState.parms.predationLevel;
}

void click_earthquake()
{
	evolve_earthquake(&g_EvolveState);
}

void click_asteroid()
{
	evolve_asteroid(&g_EvolveState);
}

void click_save()
{
	FILE *f = fopen(SAVE_NAME, "wb");

	if (f)
	{
		fseek(f, 0, SEEK_SET);
		int *start = (int*)(&g_EvolveState);
		int *write = start;
		fwrite(&SAVE_VER, sizeof(SAVE_VER), 1, f);
		fwrite(&g_Stats, sizeof(g_Stats), 1, f);
		while ((write - start) < (sizeof(g_EvolveState) / sizeof(int)))
		{
			if (fwrite(write++, sizeof(int), 1, f) != 1)
			{
				fclose(f);
				return;
			}
		}

		fclose(f);
	}
}

void click_load()
{
	FILE *f = fopen(SAVE_NAME, "rb");

	if (f)
	{
		fseek(f, 0, SEEK_SET);
		int *start = (int*)(&g_EvolveState);
		int *write = start;
		int saveVer;
		fread(&saveVer, sizeof(SAVE_VER), 1, f);
		fread(&g_Stats, sizeof(g_Stats), 1, f);
		if (saveVer != SAVE_VER)
		{
			fclose(f);
			return;
		}
		while ((write - start) < (sizeof(g_EvolveState) / sizeof(int)))
		{
			if (fread(write++, sizeof(int), 1, f) != 1)
			{
				fclose(f);
				return;
			}
		}
		fclose(f);

		memcpy(&g_EvolveParms, &g_EvolveState.parms, sizeof(g_EvolveParms));

		for (int i = 0; i < POP_MAX; i++)
			g_EvolveState.creatures[i].state = &g_EvolveState;
		for (int i = 0; i < POP_COLS_MAX; i++)
			g_EvolveState.environment[i].state = &g_EvolveState;
		for (int x = 0; x < SPECIES_HIST_ROWS; x++)
		{
			for (int y = 0; y < SPECIES_HIST_COLS; y++)
			{
				g_EvolveState.record[x][y].creature.state = &g_EvolveState;
				g_EvolveState.record[x][y].environment.state = &g_EvolveState;
			}
		}
		for (int i = 0; i < 2; i++)
			g_EvolveState.creatureLastAlive[i].state = &g_EvolveState;

		g_Refresh = 1;
		return;
	}
}

button_t g_Buttons[6] =
{
	{ "Defaults (*)",	click_defaults },
	{ "Save",			click_save },
	{ "Load",			click_load },
	{ "Predation",		click_predation },
	{ "Earthquake",		click_earthquake },
	{ "Asteroid",		click_asteroid },
};

void draw(HWND hwnd, HDC dc, int refresh)
{
	int i;
	int j;

	static clock_t lastStats = 0;

	//	PAINTSTRUCT ps;
	RECT rect;
	HFONT hFontold;
	HBRUSH hBrushold;
	HPEN hPenold;
	char msg[100];

	rect.left = 10;
	rect.right = 5000;
	rect.top = 10;
	rect.bottom = 5000;

	if (hFont == 0)
	{
		hFont = CreateFont(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
		CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Consolas"));

		hBackBrush = CreateSolidBrush(bkColor);
		hBrightBrush = CreateSolidBrush(RGB(128, 128, 192));
		hRiverPen = CreatePen( PS_SOLID, 2, RGB(64, 64, 96) );
		hBrightPen = CreatePen(PS_SOLID, 2, RGB(128, 128, 192));
	}

	hFontold = (HFONT)SelectObject(dc, (HGDIOBJ)hFont);
	hBrushold = (HBRUSH)SelectObject(dc, (HGDIOBJ)hBackBrush);
	hPenold = (HPEN)SelectObject(dc, (HGDIOBJ)hRiverPen);

	RECT fill;
	static int lastStep = 0;

	int step = g_EvolveState.step;

	GetClientRect(hwnd, &fill);

	static int last_col;

	if (last_col != g_EvolveState.river_col)
	{
		refresh = 1;
		last_col = g_EvolveState.river_col;
	}

	if (lastStep > step || refresh)
	{
		// Started a new generation
		if (refresh)
		{
			FillRect(dc, &fill, hBackBrush);

			if (g_Stats)
			{
				MoveToEx(dc, fill.right - RIGHT_PANEL_SIZE, fill.top, NULL);
				LineTo(dc, fill.right - RIGHT_PANEL_SIZE, fill.bottom);

				rect.left = fill.right - RIGHT_PANEL_SIZE + 10;
				rect.top = 10;
				for (i = 0; i < ES_COUNT; i++)
				{
					draw_text(dc, &rect, g_statNames[i], RGB(200, 200, 200));
					rect.top += 20;
				}
				draw_text(dc, &rect, "Predation:", RGB(200, 200, 200));
			}

			lastStats = 0;

			if (g_EvolveState.river_col)
			{
				int x = (10 + (g_EvolveState.river_col * (g_EvolveState.parms.genes + 1) * 10)) - 5;
				MoveToEx(dc, x, 35, (LPPOINT)NULL);
				LineTo(dc, x, 35 + (g_EvolveState.parms.popRows * 20));
			}

			if (g_Stats)
			{
				MoveToEx(dc, fill.left + 5, 20 * g_EvolveState.parms.popRows + 20 + 23, (LPPOINT)NULL);
				LineTo(dc, fill.right - RIGHT_PANEL_SIZE - 10, 20 * g_EvolveState.parms.popRows + 20 + 23);
			}
		}

		for (i = 0; i < g_EvolveState.parms.popCols; i++)
		{
			rect.left = 10 + (i * (g_EvolveState.parms.genes + 1) * 10);
			rect.top = 10;
			draw_creature(dc, &rect, &g_EvolveState.environment[i]);
		}

		for (i = 0; i < g_EvolveState.parms.popRows; i++)
		{
			for (j = 0; j < g_EvolveState.parms.popCols; j++)
			{
				rect.left = 10 + (j * (g_EvolveState.parms.genes + 1) * 10);
				rect.top = 35 + (i * 20);
				creature_t *creature = &g_EvolveState.creatures[(i*g_EvolveState.parms.popCols) + j];
				draw_creature(dc, &rect, creature);
			}
		}
	}

	while (lastStep != step && lastStep < (g_EvolveState.popSize * 2))
	{
		int index = g_EvolveState.orderTable[lastStep / 2];
		i = (index % g_EvolveState.popSize) / g_EvolveState.parms.popCols;
		j = (index % g_EvolveState.popSize) % g_EvolveState.parms.popCols;
		rect.left = 10 + (j * (g_EvolveState.parms.genes + 1) * 10);
		rect.top = 35 + (i * 20);
		creature_t *creature = &g_EvolveState.creatures[(i*g_EvolveState.parms.popCols) + j];
		draw_creature(dc, &rect, creature);
		creature->death = 0;
		lastStep += 2;
		if (lastStep > (g_EvolveState.popSize * 2))
			lastStep = 0;
	}

	if ( g_Stats && clock() - lastStats > 250)
	{
		rect.top = 10;
		for (i = 0; i < ES_COUNT; i++)
		{
			sprintf(msg, "%i", g_EvolveState.stats[i]);
			rect.left = (fill.right - 10) - strlen(msg) * 10;
			draw_text(dc, &rect, msg, RGB(200, 200, 200));
			rect.top += 20;
		}

		static int lastPredation;
		if (g_EvolveState.predation > 0.0f && g_EvolveState.rebirth <= 0)
		{
			sprintf(msg, " ON");
			rect.left = (fill.right - 10) - 3 * 10;
			draw_text(dc, &rect, msg, RGB(255, 200, 200));
			if (!lastPredation)
				g_Refresh = 1;
		}
		else
		{
			sprintf(msg, "OFF");
			rect.left = (fill.right - 10) - 3 * 10;
			draw_text(dc, &rect, msg, RGB(200, 255, 200));
			if (lastPredation)
				g_Refresh = 1;
		}
		lastPredation = g_EvolveState.predation > 0.0f && g_EvolveState.rebirth <= 0;

		rect.top += 20;
		rect.top += 20;

		for (i = 0; i < sizeof(g_parmSliders) / sizeof(parm_slider_t); i++)
		{
			rect.left = fill.right - RIGHT_PANEL_SIZE + 10;
			draw_parm_slider(dc, &rect, &g_parmSliders[i]);
		}

		rect.top += 10;
		rect.left = fill.right - RIGHT_PANEL_SIZE + 10;
		rect.bottom = rect.top + 30;
		rect.right = fill.right - 10;
		for (i = 0; i < sizeof(g_Buttons) / sizeof(button_t); i++)
		{
			draw_button(dc, &rect, &g_Buttons[i]);
			rect.top += 35;
			rect.bottom += 35;
		}

		for (i = 0; i < g_EvolveState.parms.historySpecies; i++)
		{
			if (!refresh)
			{ 
				bool anyAlive = false;
				for (j = 0; j < g_EvolveState.parms.popCols; j++)
				{
					if (g_EvolveState.record[i][0].creature.age == 0)
					{
						anyAlive = true;
						break;
					}
				}
				if ( !anyAlive )
					continue;
			}

			int clientW = fill.right;
			fill.top = 20 * g_EvolveState.parms.popRows + 20 + 20 + 25 + (45 * i);
			fill.bottom = fill.top + 45;
			fill.right -= RIGHT_PANEL_SIZE + 1;
			FillRect(dc, &fill, hBackBrush);

			rect.left = 5;
			rect.top = 20 * g_EvolveState.parms.popRows + 20 + 25;
			if (clientW - RIGHT_PANEL_SIZE > (17 * 10))
				draw_text(dc, &rect, "Species History:", RGB(200, 200, 200));

			if (!g_EvolveState.record[i][0].creature.genes[0])
				continue;

			int spc = 4;
			int oldest = 0;
			int width = ((g_EvolveState.parms.genes + 1) * 10) - spc;
			for (j = 0; j < g_EvolveState.parms.popCols; j++)
			{
				if (!g_EvolveState.record[i][j].creature.genes[0])
					break;

				oldest = g_EvolveState.record[i][j].generation;

				rect.left = 5 + (j * width);
				rect.top = 20 * g_EvolveState.parms.popRows + 20 + 20 + 25 + (45 * i);
				g_EvolveState.record[i][j].environment.age = g_EvolveState.record[i][j].creature.age + 1;
				draw_creature(dc, &rect, &g_EvolveState.record[i][j].environment);
				rect.left = 5 + (j * width);
				rect.top += 20;
				draw_creature(dc, &rect, &g_EvolveState.record[i][j].creature);
			}

			rect.top = 20 * g_EvolveState.parms.popRows + 20 + 20 + 25 + (45 * i) + 10;
			rect.left = 5 + (j * width);
			if (clientW - RIGHT_PANEL_SIZE > (17 * 10))
			{
				sprintf(msg, "%i", (oldest - g_EvolveState.record[i][0].generation) + 1);
				if (g_EvolveState.record[i][0].creature.age == 0)
					draw_text(dc, &rect, msg, RGB(200, 200, 200));
				else
					draw_text(dc, &rect, msg, RGB(128, 128, 128));
			}
		}

		lastStats = clock();
	}

	lastStep = step & ~1; // always rounded down to even number
	SelectObject(dc, hFontold);
	SelectObject(dc, hBrushold);
	SelectObject(dc, hPenold);
}

void evolve_win_tick()
{
	if (!initialized)
	{
		g_EvolveState.stats[ES_GENERATIONS] = 0;

		click_load();

		if ( g_EvolveState.stats[ES_GENERATIONS] == 0 )
		{
			evolve_parms_default(&g_EvolveParms);
			evolve_init(&g_EvolveState, &g_EvolveParms);
		}

		initialized = true;

		resize();

		build_color_table();
	}

	int extinctions = g_EvolveState.stats[ES_EXTINCTIONS];
	evolve_simulate(&g_EvolveState);
	if (extinctions != g_EvolveState.stats[ES_EXTINCTIONS])
		g_Refresh = 1;
}

void evolve_win_draw(HWND hwnd)
{
	if (!initialized)
		return;

	int refresh = g_Refresh;
	g_Refresh = 0;

	// Avoid flickering by drawing to offscreen HDC then BitBlt to window dc
	HDC dc = GetDC(hwnd);
	RECT clRect;
	GetClientRect(hwnd, &clRect);
	if (clRect.right != hdcMemRect.right || clRect.bottom != hdcMemRect.bottom)
	{
		if (hbmOld)
			SelectObject(hdcMem, hbmOld);
		if (hbmMem)
			DeleteObject(hbmMem);
		if (hdcMem)
			DeleteDC(hdcMem);

		hdcMem = CreateCompatibleDC(dc);
		hbmMem = CreateCompatibleBitmap(dc, clRect.right - clRect.left, clRect.bottom - clRect.top);
		hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

		BitBlt(hdcMem, clRect.left, clRect.top, clRect.right - clRect.left, clRect.bottom - clRect.top, dc, 0, 0, SRCCOPY);

		memcpy(&hdcMemRect, &clRect, sizeof(clRect));
	}

	draw(hwnd, hdcMem, refresh);

	BitBlt(dc, clRect.left, clRect.top, clRect.right - clRect.left, clRect.bottom - clRect.top, hdcMem, 0, 0, SRCCOPY);

	ReleaseDC(hwnd, dc);
}

void evolve_win_mouse_move(HWND hwnd, HDC dc, int x, int y)
{
	if (!g_Stats)
		return;

	if (g_Hovering)
		g_Hovering->focus = false;

	if (g_sliderEditing)
	{
		g_sliderEditing->w.active = true;
		move_parm_slider(hwnd, dc, g_sliderEditing, x);
	}

	for (int i = 0; i < sizeof(g_parmSliders) / sizeof(parm_slider_t); i++)
	{
		if (g_parmSliders[i].w.scrRect.left <= x && g_parmSliders[i].w.scrRect.right >= x &&
			g_parmSliders[i].w.scrRect.top <= y && g_parmSliders[i].w.scrRect.bottom >= y)
		{
			g_Hovering = &g_parmSliders[i].w;
			g_parmSliders[i].w.focus = true;
			RECT r;
			memcpy(&r, &g_parmSliders[i].w.frameRect, sizeof(RECT));
			draw_parm_slider(dc, &r, &g_parmSliders[i]);
		}
	}

	for (int i = 0; i < sizeof(g_Buttons) / sizeof(button_t); i++)
	{
		if (g_Buttons[i].w.frameRect.left <= x && g_Buttons[i].w.frameRect.right >= x &&
			g_Buttons[i].w.frameRect.top <= y && g_Buttons[i].w.frameRect.bottom >= y)
		{
			g_Hovering = &g_Buttons[i].w;
			g_Buttons[i].w.focus = true;
			draw_button(dc, &g_Buttons[i].w.frameRect, &g_Buttons[i]);
		}
	}
}

void evolve_win_mouse_down(HWND hwnd, HDC dc, int x, int y)
{
	if (!g_Stats)
		return;

	for (int i = 0; i < sizeof(g_parmSliders) / sizeof(parm_slider_t); i++)
	{
		if (g_parmSliders[i].w.scrRect.left <= x && g_parmSliders[i].w.scrRect.right >= x &&
			g_parmSliders[i].w.scrRect.top <= y && g_parmSliders[i].w.scrRect.bottom >= y)
		{
			g_sliderEditing = &g_parmSliders[i];
			g_sliderEditing->w.active = true;
			return;
		}
	}

	for (int i = 0; i < sizeof(g_Buttons) / sizeof(button_t); i++)
	{
		if (g_Buttons[i].w.frameRect.left <= x && g_Buttons[i].w.frameRect.right >= x &&
			g_Buttons[i].w.frameRect.top <= y && g_Buttons[i].w.frameRect.bottom >= y)
		{
			g_buttonClicking = &g_Buttons[i];
			g_buttonClicking->w.active = true;
			draw_button(dc, &g_buttonClicking->w.frameRect, g_buttonClicking);
			break;
		}
	}
}

void evolve_win_mouse_up(HWND hwnd, HDC dc, int x, int y)
{
	if (!g_Stats)
		return;

	if (g_sliderEditing)
	{
		g_sliderEditing->w.active = false;
		draw_parm_slider(dc, &g_sliderEditing->w.frameRect, g_sliderEditing);

		if (g_sliderEditing->fullRefresh)
		{
			if (memcmp(&g_EvolveState.parms, &g_EvolveParms, sizeof( g_EvolveParms) ))
			{
				evolve_init(&g_EvolveState, &g_EvolveParms);
				g_Refresh = 1;
			}
		}

		build_color_table();
		resize();
	}

	g_sliderEditing = NULL;

	if (g_buttonClicking)
	{
		button_t *drawButton = g_buttonClicking;
		g_buttonClicking->w.active = false;
		g_buttonClicking = NULL;
		draw_button(dc, &drawButton->w.frameRect, drawButton);

		if (drawButton->w.frameRect.left <= x && drawButton->w.frameRect.right >= x &&
			drawButton->w.frameRect.top <= y && drawButton->w.frameRect.bottom >= y)
		{
			if (drawButton->func)
				drawButton->func();
		}
	}
}
