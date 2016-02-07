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
InitWindowComponents(HWND MainWindow, wnd_ctrls *Controls)
{
	int UpperLeftX = 20;
	int UpperLeftY = 10;
	int DiffY = 30;
	int FrameWidth = 340;

	HINSTANCE Module = GetModuleHandle(0);
	HWND ComboBox = CreateWindowA(
		WC_COMBOBOXA, 
		0, 
		WS_CHILD | WS_VISIBLE | WS_OVERLAPPED | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_SORT,
		UpperLeftX, UpperLeftY,
		FrameWidth, 200, 
		MainWindow,
		0, 
		Module,
		0);

	UpperLeftY += DiffY;

	HWND TextBox = CreateWindowA(
		WC_STATICA,
		0,
		WS_CHILD | WS_VISIBLE | WS_OVERLAPPED | SS_CENTER,
		UpperLeftX, UpperLeftY,
		FrameWidth, 20,
		MainWindow,
		0,
		Module,
		0);

	UpperLeftY += 3 * DiffY / 2;

	int ButtonWidth = 150;
	int ButtonX = FrameWidth - 2 * ButtonWidth;
	int ButtonMargin = 5;
	HWND ButtonOK = CreateWindowA(
		WC_BUTTONA,
		"OK",
		WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_OVERLAPPED | BS_DEFPUSHBUTTON,
		ButtonX - ButtonMargin, UpperLeftY,
		ButtonWidth, 30,
		MainWindow,
		0, 
		Module, 
		0);

	HWND ButtonCancel = CreateWindowA(
		WC_BUTTONA,
		"Cancel",
		WS_TABSTOP | WS_CHILD | WS_VISIBLE | WS_OVERLAPPED | BS_DEFPUSHBUTTON,
		ButtonX + ButtonWidth + ButtonMargin, UpperLeftY,
		ButtonWidth, 30,
		MainWindow,
		0, 
		Module, 
		0);

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

	Controls->ComboBox = ComboBox;
	Controls->StaticText = TextBox;
	Controls->ButtonOK = ButtonOK;
	Controls->ButtonCancel = ButtonCancel;
	Controls->CurSelection = 0;
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
	WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	WindowClass.lpszClassName = WINDOW_CLASS_NAME;
	RegisterClassExA(&WindowClass);

	HWND MainWindow = CreateWindowEx(
		WS_EX_APPWINDOW,
		WINDOW_CLASS_NAME,
		"FSWindow",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		0, 0,
		400, 180,
		0, 0,
		Module,
		0);

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
