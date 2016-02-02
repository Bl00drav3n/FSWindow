#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

#define PRINT_ERR(x) OutputDebugStringA((x))

#define DEFAULT_SEARCH_NAME "The Witness - D3D11"

static BOOL CALLBACK EnumWindowsProc(HWND Window, LPARAM lParam)
{
	char *SearchWindowName = (char*)lParam;
	if(!GetParent(Window)) {
		LONG_PTR Style = GetWindowLongPtrA(Window, GWL_STYLE);
		if(Style) {
			int TextLength = GetWindowTextLengthA(Window);
			if(TextLength > 0) {
				char Buffer[256];
				if(GetWindowTextA(Window, Buffer, sizeof(Buffer))) {
					if(strncmp(SearchWindowName, Buffer, sizeof(Buffer)) == 0) {
						printf("Found window: %s\n", Buffer);

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

						// NOTE: Found the window, exit enumeration here.
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

	return TRUE;
}

int main(int argc, char *argv[])
{
	char *SearchWindowName = DEFAULT_SEARCH_NAME;
	if(!EnumWindows(EnumWindowsProc, (LPARAM)SearchWindowName)) {
		PRINT_ERR("Could not enumerate windows.\n");
	}
	
	system("pause");
	return 0;
}
