// EvolveWin.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "EvolveWin.h"
#include "evolve.h"
#include <time.h>
#include <math.h>
#include <io.h>
#include <stdio.h>
#include <OleAuto.h>

#pragma warning(disable:4996) // This function or variable may be unsafe.

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
HWND g_hWnd;										// current window
HACCEL hAccelTable;
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
int g_Quit = 0;
int g_Refresh = 1;
int g_Stats = 0;
evolve_state_t g_EvolveState;

extern HFONT hFont;
extern HBRUSH hBackBrush;
extern HBRUSH hBrightBrush;
extern HPEN hRiverPen;
extern HPEN hBrightPen;
extern int RIGHT_PANEL_SIZE;

extern HDC hdcMem;
extern RECT hdcMemRect;
extern HBITMAP hbmMem;
extern HBITMAP hbmOld;

void click_save();

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void				evolve_win_tick();
void				evolve_win_draw(HWND hwnd);
void				evolve_win_mouse_move(HWND hwnd, HDC dc, int x, int y);
void				evolve_win_mouse_down(HWND hwnd, HDC dc, int x, int y);
void				evolve_win_mouse_up(HWND hwnd, HDC dc, int x, int y);

void update_title()
{
	static clock_t lastTitleUpdate = 0;
	if (clock() - lastTitleUpdate > 1000)
	{
		char title[256];
		BSTR unicodestr = SysAllocStringLen(NULL, 512);
		sprintf(title, "Winvolve [G: %i SE: %i ME: %i]", g_EvolveState.stats[ES_GENERATIONS], g_EvolveState.stats[ES_SPECIES_EVER], g_EvolveState.stats[ES_EXTINCTIONS_MASS]);
		::MultiByteToWideChar(CP_ACP, 0, title, -1, unicodestr, 512);
		SetWindowText(g_hWnd, unicodestr);
		SysFreeString(unicodestr);
		lastTitleUpdate = clock();
	}
}

void update_evolve()
{
	static clock_t lastP = 0;
	static WINDOWPLACEMENT p;

	// start timer
	timer_mark();

	if (!lastP || clock() - lastP > 1000)
	{
		GetWindowPlacement(g_hWnd, &p);
		lastP = clock();
	}

	evolve_win_tick();

	if (p.showCmd != SW_SHOWMINIMIZED && (g_EvolveState.step % 2 == 0))
		evolve_win_draw(g_hWnd);

	// Clamp it when viewing creatures so it doesnt evolve so fast you can't see whats going on
	float dur = (float) timer_mark();
	float stepCnt = (float)(g_EvolveState.parms.popCols * g_EvolveState.parms.popRows * 2);
	float genPerSec = 3;
	float minMs = (1.0f / genPerSec) / stepCnt;
	if (g_Stats != 2 && dur < minMs)
		Sleep( max( 1, (DWORD) (minMs - dur) ) );
}

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	MSG msg;

	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_EVOLVEWIN, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInst, MAKEINTRESOURCE(IDC_EVOLVEWIN));

	while (!g_Quit)
	{
		update_evolve();

		update_title();

		if (PeekMessage(&msg, g_hWnd, 0, 0, 1))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	return (int)0;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EVOLVEWIN));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_EVOLVEWIN);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

void resize(HWND hWnd = NULL)
{
	RECT windowOld;
	RECT window;
	int cW;
	int cH;

	if (hWnd == NULL)
		hWnd = g_hWnd;

	GetWindowRect(hWnd, &windowOld);
	memcpy(&window, &windowOld, sizeof(RECT));

	int panelH = 950;

	cH = ((g_EvolveState.parms.popRows + 2) * 20) + 15;
	cW = ((g_EvolveState.parms.popCols * (g_EvolveState.parms.genes + 1)) * 10) + 25;

	if ( g_Stats == 1)
	{
		window.bottom = window.top + panelH;
		window.right = window.left + cW + RIGHT_PANEL_SIZE + 5;
	}
	else if (g_Stats == 2)
	{
		window.bottom = window.top + (ES_COUNT * 20) + 80;
		window.right = window.left + RIGHT_PANEL_SIZE;
	}
	else
	{
		window.bottom = window.top + cH + 50;
		window.right = window.left + cW;
	}

	if (memcmp(&window, &windowOld, sizeof(RECT)))
		MoveWindow(hWnd, window.left, window.top, window.right - window.left, window.bottom - window.top, true);
}

void SetStdOutToNewConsole()
{
	// allocate a console for this app
	AllocConsole();

	// redirect unbuffered STDOUT to the console
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	int fileDescriptor = _open_osfhandle((intptr_t)consoleHandle, 0);// _O_TEXT);
	FILE *fp = _fdopen(fileDescriptor, "w");
	*stdout = *fp;
	setvbuf(stdout, NULL, _IONBF, 0);

	// give the console window a nicer title
	SetConsoleTitle(L"Debug Output");

	// give the console window a bigger buffer size
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(consoleHandle, &csbi))
	{
		COORD bufferSize;
		bufferSize.X = csbi.dwSize.X;
		bufferSize.Y = 9999;
		SetConsoleScreenBufferSize(consoleHandle, bufferSize);
	}
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   int dwStyle = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

   g_hWnd = CreateWindow(szWindowClass, szTitle, dwStyle,
      CW_USEDEFAULT, 0, 950, 950, NULL, NULL, hInstance, NULL);

   if (!g_hWnd)
   {
      return FALSE;
   }

   ShowWindow(g_hWnd, nCmdShow);
   UpdateWindow(g_hWnd);

   //SetStdOutToNewConsole();

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	int x, y;
	RECT clRect;
	HDC hdc;
	PAINTSTRUCT ps;


	if (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
	{
		x = LOWORD(lParam);
		y = HIWORD(lParam);
		GetClientRect(hWnd, &clRect);
	}

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			g_Quit = 1;
			DeleteObject((HGDIOBJ)hFont);
			DeleteObject((HGDIOBJ)hBackBrush);
			DeleteObject((HGDIOBJ)hRiverPen);
			DeleteObject((HGDIOBJ)hBrightBrush);
			DeleteObject((HGDIOBJ)hBrightPen);
			if (hbmOld)
				SelectObject(hdcMem, hbmOld);
			if (hbmMem)
				DeleteObject(hbmMem);
			if (hdcMem)
				DeleteDC(hdcMem);
			click_save();
			DestroyWindow(hWnd);
			break;
		case IDM_PREDATION:
			if (g_EvolveState.predation > 0.0f)
				g_EvolveState.predation = 0.0f;
			else
				g_EvolveState.predation = g_EvolveState.parms.predationLevel;
			g_Refresh = 1;
			break;
		case IDM_ASTEROID:
			evolve_asteroid(&g_EvolveState);
			g_Refresh = 1;
			break;
		case IDM_EARTHQUAKE:
			evolve_earthquake(&g_EvolveState);
			g_Refresh = 1;
			break;
		case IDM_STATS:
			g_Stats++;
			if ( g_Stats > 2 )
				g_Stats = 0;
			resize(hWnd);
			g_Refresh = 1;
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		g_Refresh = 1;
		EndPaint(hWnd, &ps);
		break;
	case WM_MOUSEMOVE:
		evolve_win_mouse_move(hWnd, hdcMem, x, y);
		if ( x > clRect.right || x < 0 || y > clRect.bottom || y < 0 )
			evolve_win_mouse_up(hWnd, hdcMem, x, y);
		break;
	case WM_LBUTTONDOWN:
		evolve_win_mouse_down(hWnd, hdcMem, x, y);
		break;
	case WM_LBUTTONUP:
		evolve_win_mouse_up(hWnd, hdcMem, x, y);
		break;
	case WM_DESTROY:
		g_Quit = 1;
		click_save();
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}


double timer_mark(int timerIndex /* = 0*/)
{
	if (timerIndex < 0 || timerIndex > 15)
		return 0.0;

	static LARGE_INTEGER last[16] = { 0 };
	LARGE_INTEGER now;
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&now);
	double elapsedTime = (now.QuadPart - last[timerIndex].QuadPart) * 1000.0 / freq.QuadPart;
	QueryPerformanceCounter(&last[timerIndex]);
	return elapsedTime;
}
