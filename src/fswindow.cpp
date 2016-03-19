#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>
#include <Strsafe.h>
#include <stdio.h>
#include <stdlib.h>

#define PRINT_ERR(x) OutputDebugStringA((x))
#define WINDOW_CLASS_NAME "FSWindowClass"
#define ENUM_ERR_ABORTED 0x20000000
#define MAX_WND_ENUM_COUNT 512

#define MAIN_WINDOW_WIDTH 400
#define MAIN_WINDOW_HEIGHT 150

// NOTE: Layout specification
#define MARGIN_X 20
#define MARGIN_Y 8
#define SPACING_X 20
#define SPACING_Y 20
#define NUM_ELEMENTS_X 2
#define NUM_ELEMENTS_Y 3

struct window_data
{
	DWORD Style;
	HWND Window;
};

struct window_data_pool
{
	DWORD DataCount;
	DWORD MaxDataCount;
	window_data *Data;
};

struct enum_data
{
	HWND ComboBox;
	window_data_pool *Pool;
};

struct wnd_ctrls
{
	HWND ComboBox;
	HWND StaticText;
	HWND ButtonOK;
	HWND ButtonCancel;

	window_data_pool Pool;
	window_data *CurSelection;
};

static wnd_ctrls Controls;

static void
AllocateWindowDataPool(window_data_pool *Pool, DWORD MaxCount)
{
	if(Pool) {
		if(Pool->Data && MaxCount != Pool->MaxDataCount) {
			VirtualFree(Pool->Data, 0, MEM_RELEASE);
		}

		DWORD MemSize = MaxCount * sizeof(window_data);
		Pool->Data = (window_data*)VirtualAlloc(0, MemSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		Pool->DataCount = 0;
		Pool->MaxDataCount = MaxCount;
	}
}

static window_data *
PushWindowData(window_data_pool *Pool)
{
	window_data *Result = 0;
	if(Pool->DataCount < Pool->MaxDataCount) {
		Result = Pool->Data + Pool->DataCount++;
	}

	return Result;
}

static void
SetFullscreen(window_data Data)
{
	HWND Window = Data.Window;
	DWORD Style = Data.Style;
	LONG_PTR ExStyle = GetWindowLongPtrA(Window, GWL_EXSTYLE);
	if(ExStyle) {
		SetWindowLong(Window, GWL_STYLE, Style & ~(WS_CAPTION | WS_THICKFRAME));
		SetWindowLong(Window, GWL_EXSTYLE, ExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

		MONITORINFO MonitorInfo = {};
		MonitorInfo.cbSize = sizeof(MonitorInfo);
		if(GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTONEAREST), &MonitorInfo)) {
			RECT Rect = MonitorInfo.rcMonitor;
			int X = Rect.left;
			int Y = Rect.top;
			int Width = Rect.right - Rect.left;
			int Height = Rect.bottom - Rect.top;
			UINT Flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
			if(!SetWindowPos(Window, 0, X, Y, Width, Height, Flags)) {
				PRINT_ERR("Call to SetWindowPos failed.\n");
			}
		}
		else {
			PRINT_ERR("Could not get monitor info.\n");
		}
	}
	else {
		PRINT_ERR("Could not get extended style info.\n");
	}
}

static BOOL CALLBACK 
EnumWindowsProc(HWND Window, LPARAM lParam)
{
	BOOL Result = TRUE;
	enum_data *EnumData = (enum_data*)lParam;
	window_data_pool *Pool = EnumData->Pool;
	HWND ComboBox = EnumData->ComboBox;
	if(!GetParent(Window)) {
		LONG_PTR Style = GetWindowLongPtrA(Window, GWL_STYLE);
		if(Style & WS_VISIBLE) {
			int TextLength = GetWindowTextLengthA(Window);
			if(TextLength > 0) {
				char Buffer[256];
				if(GetWindowTextA(Window, Buffer, sizeof(Buffer))) {
					window_data *WndData = PushWindowData(Pool);
					if(WndData) {
						WndData->Style = Style;
						WndData->Window = Window;
						DWORD Pos = (DWORD)SendMessageA(ComboBox, CB_ADDSTRING, 0, (LPARAM)Buffer);
						SendMessageA(ComboBox, CB_SETITEMDATA, Pos, (LPARAM)WndData);
					}
					else {
						PRINT_ERR("Maximum window count reached.\n");
						SetLastError(ENUM_ERR_ABORTED);
						return FALSE;
					}
				}
				else {
					PRINT_ERR("Could not get window title.\n");
				}
			}
			else {
				PRINT_ERR("Could not get window text length.\n");
			}
		}
	}

	return Result;
}

static void
ApplySystemParameters(wnd_ctrls *Controls)
{
	if(Controls) {
		NONCLIENTMETRICS Metrics = {};
		Metrics.cbSize = sizeof(Metrics);

		SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(Metrics), &Metrics, 0);

		HFONT Font = CreateFontIndirectA(&Metrics.lfMessageFont);
		SendMessageA(Controls->ButtonCancel, WM_SETFONT, (WPARAM)Font, 0);
		SendMessageA(Controls->ButtonOK, WM_SETFONT, (WPARAM)Font, 0);
		SendMessageA(Controls->ComboBox, WM_SETFONT, (WPARAM)Font, 0);
		SendMessageA(Controls->StaticText, WM_SETFONT, (WPARAM)Font, 0);
	}
}

struct wnd_dim
{
	int Width;
	int Height;
};
static inline wnd_dim
GetWindowDimensions(HWND Window)
{
	wnd_dim Result = {};

	RECT Rect;
	if(GetClientRect(Window, &Rect)) {
		Result.Width = Rect.right - Rect.left;
		Result.Height = Rect.bottom - Rect.top;
	}

	return Result;
}

static void
ApplyGridLayout(wnd_ctrls *Controls, int WindowWidth, int WindowHeight, int MarginX, int MarginY)
{
	if(Controls) {
		// NOTE: Build a nice grid layout
		float GridSpacingX = (float)WindowWidth / (float)NUM_ELEMENTS_X;
		float GridSpacingY = (float)WindowHeight / (float)NUM_ELEMENTS_Y;

		wnd_dim Dim;
		UINT SetPosFlags = SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOZORDER | SWP_SHOWWINDOW;

		Dim = GetWindowDimensions(Controls->ComboBox);
		SetWindowPos(Controls->ComboBox, 0, MarginX, (int)((GridSpacingY - Dim.Height) / 2), MAIN_WINDOW_WIDTH - 2 * MarginX, MAIN_WINDOW_HEIGHT, SetPosFlags);

		// NOTE: CW_USEDEFAULT does not work for some reason. Thanks Microsoft.
		// TODO: To support different fonts this has to figure out how big the actual button really should be.
		int TextBoxWidth = (int)(WindowWidth - 2 * MarginX);
		int TextBoxHeight = (int)(GridSpacingY - 2 * MarginY);
		int TextY = (int)(GridSpacingY + MarginY);
		SetWindowPos(Controls->StaticText, 0, MarginX, TextY, TextBoxWidth, TextBoxHeight, SetPosFlags);

		int ButtonWidth = (int)(GridSpacingX - 2 * MarginX);
		int ButtonHeight = (int)(GridSpacingY - 2 * MarginY);
		int ButtonOffsetX = MarginX;
		int ButtonY = (int)(2 * GridSpacingY + MarginY);
		SetWindowPos(Controls->ButtonOK, 0, ButtonOffsetX, ButtonY, ButtonWidth, ButtonHeight, SetPosFlags);
		SetWindowPos(Controls->ButtonCancel, 0, (int)(GridSpacingX + ButtonOffsetX), ButtonY, ButtonWidth, ButtonHeight, SetPosFlags);
	}
}

static void
InitWindowComponents(HWND MainWindow, wnd_ctrls *Controls)
{
	HINSTANCE Module = GetModuleHandle(0);

	HWND ComboBox, TextBox, ButtonOK, ButtonCancel;
	ComboBox = CreateWindowA(
		WC_COMBOBOXA, 
		0, 
		WS_CHILD | WS_OVERLAPPED | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_SORT,
		0, 0,
		CW_USEDEFAULT, CW_USEDEFAULT,
		MainWindow,
		0, 
		Module,
		0);

	TextBox = CreateWindowA(
		WC_STATICA,
		"Please choose a window from the list above.",
		WS_CHILD | WS_OVERLAPPED | SS_CENTER | SS_CENTERIMAGE,
		0, 0,
		CW_USEDEFAULT, CW_USEDEFAULT,
		MainWindow,
		0,
		Module,
		0);

	ButtonCancel = CreateWindowA(
		WC_BUTTONA,
		"Cancel",
		WS_TABSTOP | WS_CHILD | WS_OVERLAPPED | BS_DEFPUSHBUTTON,
		0, 0,
		CW_USEDEFAULT, CW_USEDEFAULT,
		MainWindow,
		0, 
		Module, 
		0);

	ButtonOK = CreateWindowA(
		WC_BUTTONA,
		"Set to fullscreen",
		WS_TABSTOP | WS_CHILD | WS_OVERLAPPED | BS_DEFPUSHBUTTON,
		0, 0,
		CW_USEDEFAULT, CW_USEDEFAULT,
		MainWindow,
		0, 
		Module, 
		0);

	Controls->ComboBox = ComboBox;
	Controls->StaticText = TextBox;
	Controls->ButtonOK = ButtonOK;
	Controls->ButtonCancel = ButtonCancel;
	Controls->CurSelection = 0;

	ApplySystemParameters(Controls);

	AllocateWindowDataPool(&Controls->Pool, MAX_WND_ENUM_COUNT);
	enum_data EnumData = {};
	EnumData.ComboBox = ComboBox;
	EnumData.Pool = &Controls->Pool;
	if(!EnumWindows(EnumWindowsProc, (LPARAM)&EnumData)) {
		if(GetLastError() != ENUM_ERR_ABORTED) {
			PRINT_ERR("Could not enumerate windows.\n");
		}
	}

	if(Controls->Pool.DataCount > 0) {
		SetFocus(ComboBox);
	}
	else {
		SetWindowText(TextBox, "No windows found.");
		SetFocus(ButtonOK);
	}

	ApplyGridLayout(Controls, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT, MARGIN_X, MARGIN_Y);
}

static void 
DrawGradient(HDC hDC, RECT Region, COLORREF TopLeft, COLORREF TopRight, COLORREF BottomLeft, COLORREF BottomRight)
{
	GRADIENT_TRIANGLE GradientTriangle[2] = { {0, 1, 2}, {2, 0, 3} };
    TRIVERTEX Vertices[4] = {
        Region.left,
        Region.top,
        GetRValue(TopLeft) << 8,
        GetGValue(TopLeft) << 8,
        GetBValue(TopLeft) << 8,
        0x0000,

		Region.left,
		Region.bottom,
        GetRValue(BottomLeft) << 8,
        GetGValue(BottomLeft) << 8,
        GetBValue(BottomLeft) << 8,
        0x0000,

        Region.right,
        Region.bottom,
        GetRValue(BottomRight) << 8,
        GetGValue(BottomRight) << 8,
        GetBValue(BottomRight) << 8,
        0x0000,

		Region.right,
        Region.top,
        GetRValue(TopRight) << 8,
        GetGValue(TopRight) << 8,
        GetBValue(TopRight) << 8,
        0x0000
    };

    GradientFill(hDC, Vertices, 4, &GradientTriangle, 2, GRADIENT_FILL_TRIANGLE);
}

static LRESULT CALLBACK 
MainWindowProc(HWND MainWindow, UINT Message, WPARAM wParam, LPARAM lParam)
{
	LRESULT Result = 0;
	switch(Message) {
		case WM_CREATE:
			InitWindowComponents(MainWindow, &Controls);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		case WM_ERASEBKGND:
			// NOTE: Return a non-zero value. Literally.
			Result = !0;
			break;
		case WM_PAINT:
		{
			PAINTSTRUCT PaintStruct;
			HDC DC = BeginPaint(MainWindow, &PaintStruct);
			RECT FillRegion;
			GetClientRect(MainWindow, &FillRegion);
			COLORREF Top = RGB(239, 241, 245);
			COLORREF Bottom = RGB(218, 223, 233);
			DrawGradient(DC, FillRegion, Top, Top, Bottom, Bottom);
			EndPaint(MainWindow, &PaintStruct);
			ReleaseDC(MainWindow, DC);
		} break;
		case WM_COMMAND:
		{
			HWND Ctrl = (HWND)lParam;
			if(HIWORD(wParam) == CBN_SELCHANGE && Ctrl == Controls.ComboBox) {
				DWORD Pos = SendMessageA((HWND)lParam, CB_GETCURSEL, 0, 0);
				window_data *WndData = (window_data*)SendMessageA((HWND)lParam, CB_GETITEMDATA, Pos, 0);
				if(WndData) {
					Controls.CurSelection = WndData;
					HWND Window = WndData->Window;
					WINDOWINFO WindowInfo = {};
					WindowInfo.cbSize = sizeof(WindowInfo);
					if(GetWindowInfo(Window, &WindowInfo)) {
						RECT Rect = WindowInfo.rcWindow;
						LONG Width = Rect.right - Rect.left;
						LONG Height = Rect.bottom - Rect.top;

						// TODO: Display usefull data
						char Buffer[256];
						StringCbPrintfA(Buffer, sizeof(Buffer), "%dx%d starting at (%d,%d)", Width, Height, Rect.left, Rect.top);
						SetWindowTextA(Controls.StaticText, Buffer);
						ShowWindow(Controls.StaticText, TRUE);
					}
					else {
						PRINT_ERR("Could not get window info.\n");
					}
				}
			}
			else if(HIWORD(wParam) == BN_CLICKED) {
				if(Ctrl == Controls.ButtonOK) {
					if(Controls.CurSelection) {
						SetFullscreen(*Controls.CurSelection);
					}
					PostQuitMessage(0);
				}
				else if(Ctrl == Controls.ButtonCancel) {
					PostQuitMessage(0);
				}
			}
		} break;
		default:
			Result = DefWindowProcA(MainWindow, Message, wParam, lParam);
	}

	return Result;
}

int CALLBACK 
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	HINSTANCE Module = hInstance;
	WNDCLASSEX WindowClass = {};
	WindowClass.cbSize = sizeof(WindowClass);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = MainWindowProc;
	WindowClass.hInstance = Module;
	WindowClass.hIcon = LoadIcon(0, IDI_APPLICATION);
	WindowClass.hCursor = LoadCursorA(0, IDC_ARROW);
	WindowClass.lpszClassName = WINDOW_CLASS_NAME;
	RegisterClassExA(&WindowClass);

	DWORD WindowExStyle = WS_EX_APPWINDOW;
	DWORD WindowStyle = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
	HWND MainWindow = CreateWindowEx(
		WindowExStyle,
		WINDOW_CLASS_NAME,
		"FSWindow",
		WindowStyle,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		0, 0,
		Module,
		0);

	RECT AdjustRect = {};
	AdjustRect.right = MAIN_WINDOW_WIDTH;
	AdjustRect.bottom = MAIN_WINDOW_HEIGHT;
	AdjustWindowRectEx(&AdjustRect, WindowStyle, FALSE, WindowExStyle);
	SetWindowPos(MainWindow, 0, 
		0, 0, 
		AdjustRect.right - AdjustRect.left, AdjustRect.bottom - AdjustRect.top, 
		SWP_NOMOVE | SWP_NOCOPYBITS | SWP_SHOWWINDOW);

	BOOL Running = TRUE;
	while(Running) {
		MSG Message;
		while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE)) {
			if(Message.message == WM_QUIT) {
				Running = FALSE;
				break;
			}
			else {
				TranslateMessage(&Message);
				DispatchMessageA(&Message);
			}
		}

		Sleep(10);
	}

	return 0;
}
