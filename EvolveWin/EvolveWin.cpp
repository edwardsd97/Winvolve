// EvolveWin.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "EvolveWin.h"
#include "evolve.h"
#include <time.h>
#include <math.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
HWND hWnd;										// current window
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
int g_Quit = 0;
int g_Refresh = 1;
int g_Stats = 1;
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

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_EVOLVEWIN, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_EVOLVEWIN));

	clock_t nextTick = clock() + 1;
	
	// Main message loop:
	while (!g_Quit)
	{
		if (clock() >= nextTick)
		{
			// Clamp to a maximum generations per second
			int generationsPerSecond = 3;
			clock_t start = clock();
			
			evolve_win_tick();

			if (g_EvolveState.step % 2 == 0)
				evolve_win_draw(hWnd);

			clock_t end = clock();
			nextTick = clock() + max(1, int(ceil((1000.0f / float(generationsPerSecond)) / float(g_EvolveState.popSize*2))) - (end - start));
		}

		if (PeekMessage(&msg, NULL, 0, 0, 1))
		{
			if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	return (int) msg.wParam;
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

void resize(HWND hWnd)
{
	RECT window;

	GetWindowRect(hWnd, &window);

	int standardW = 980;
	int standardH = 950;

	if (g_Stats)
	{
		window.right = window.left + standardW;
		window.bottom = window.top + standardH;
	}
	else
	{
		window.right = window.left + (standardW - RIGHT_PANEL_SIZE);
		int standardNeeded = ((22 + 1) * 20) + 10;
		int needed = ((g_EvolveState.parms.popRows + 1) * 20) + 10;
		window.bottom = (window.top + standardH) - 410 + ((standardH - standardNeeded) - (standardH - needed));
	}

	MoveWindow(hWnd, window.left, window.top, window.right - window.left, window.bottom - window.top, true);
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

   hWnd = CreateWindow(szWindowClass, szTitle, dwStyle,
      CW_USEDEFAULT, 0, 980, 950, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

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
	PAINTSTRUCT ps;
	HDC hdc;

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
			g_Stats = !g_Stats;
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
		evolve_win_mouse_move(hWnd, hdcMem, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_LBUTTONDOWN:
		evolve_win_mouse_down(hWnd, hdcMem, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_LBUTTONUP:
		evolve_win_mouse_up(hWnd, hdcMem, LOWORD(lParam), HIWORD(lParam));
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
