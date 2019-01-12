#include "pch.h"

#include <windows.h>
#include <cstdio>
#include <shellapi.h>
#include <commctrl.h>
#include <tlhelp32.h>

#pragma comment(lib,"Comctl32.lib")

#include "resource.h"

UINT WM_TRAY_ICON_NOTIFYICON;

LPTSTR error()
{
	LPTSTR lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		LPTSTR(&lpMsgBuf),
		0, nullptr);
	return lpMsgBuf;
}

BOOL GetProcessNameFromHwnd(HWND hWnd, HWND target, LPTSTR outName, int outNameSize)
{
	BOOL found = FALSE;
	DWORD pid;
	DWORD tid = GetWindowThreadProcessId(target, &pid);
	if (tid)
	{
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot)
		{
			PROCESSENTRY32 pe32;
			pe32.dwSize = sizeof(pe32);
			if (Process32First(hSnapshot, &pe32))
			{
				do
				{
					if (pe32.th32ProcessID == pid)
					{
						found = TRUE;
						break;
					}
				}
				while (Process32Next(hSnapshot, &pe32));
				CloseHandle(hSnapshot);
				if (found)
					wcscpy_s(outName, outNameSize, pe32.szExeFile);
			}
		}
		else
		{
			MessageBox(hWnd, error(), TEXT("CreateToolhelp32Snapshot Error"), MB_ICONERROR | MB_OK);
		}
	}
	return found;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND targetHwnd;
	static HBRUSH hbrstatic;
	static NOTIFYICONDATA iconData;
	static HWND iconStatic;
	static HWND parent = nullptr;
	if (!parent) parent = hWnd;

	if (msg == WM_TRAY_ICON_NOTIFYICON)
	{
		switch ((UINT)lParam)
		{
		case WM_LBUTTONUP:
			Shell_NotifyIcon(NIM_DELETE, &iconData);
			ShowWindow(targetHwnd, SW_RESTORE);
			SetForegroundWindow(targetHwnd);
			PostQuitMessage(0);
			break;
		default: ;
		}
		return 0;
	}
	RECT r;
	switch (msg)
	{
	case WM_PAINT:
		{
			if (hWnd == iconStatic)
			{
				RECT rc;
				PAINTSTRUCT ps;

				HDC hdc = BeginPaint(hWnd, &ps);
				HCURSOR cursor = LoadCursor(nullptr, IDC_CROSS);
				GetClientRect(hWnd, &rc);
				int w = GetSystemMetrics(SM_CXCURSOR);
				DrawIcon(hdc, (rc.right - rc.left - w) / 2, 0, cursor);
				SetBkMode(hdc, TRANSPARENT);
				EndPaint(hWnd, &ps);
				DeleteObject(cursor);
				return 0;
			}
			return DefWindowProc(hWnd, msg, wParam, lParam);
		}
	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			DWORD c = GetSysColor(COLOR_WINDOW);
			COLORREF color = RGB(GetRValue(c), GetGValue(c), GetBValue(c));
			SetBkColor(hdcStatic, color);
			if (!hbrstatic)
			{
				hbrstatic = CreateSolidBrush(color);
			}
			return INT_PTR(hbrstatic);
		}
	case WM_CREATE:
		if (hWnd != iconStatic)
		{
			GetWindowRect(hWnd, &r);
			CreateWindow(L"STATIC",
				L"Drag the cursor into any window to hide it",
				WS_CHILD | WS_VISIBLE | SS_CENTER,
				0, 16, r.right - r.left, 64,
				hWnd, NULL, GetModuleHandle(NULL), NULL);

			iconStatic =
				CreateWindow(L"Window Hider Static", L"Window Hider Static",
				WS_CHILD | WS_VISIBLE,
				0, 64, r.right - r.left, r.bottom - r.top - 96,
				hWnd, NULL, GetModuleHandle(NULL), NULL);
			SetWindowLongPtr(iconStatic, GWLP_WNDPROC, INT_PTR(WindowProc));
			if (!iconStatic)
			{
				MessageBox(hWnd,
				           error(),
				           TEXT("Error"),
				           MB_ICONERROR | MB_OK);
			}
		}
		break;
	case WM_DESTROY:
		DeleteObject(hbrstatic);
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;
	case WM_LBUTTONDOWN:
		SetCapture(hWnd);
		ShowWindow(iconStatic, SW_HIDE);
		SetCursor(LoadCursor(nullptr, IDC_CROSS));
		break;
	case WM_LBUTTONUP:
		{
			ReleaseCapture();
			POINT p;
			p.x = LOWORD(lParam);
			p.y = HIWORD(lParam);
			ClientToScreen(hWnd, &p);
			HWND target = ChildWindowFromPointEx(GetDesktopWindow(), p,
			                                     CWP_SKIPDISABLED | CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT);
			ShowWindow(iconStatic, SW_RESTORE);
			if (target == parent)
			{
				MessageBox(hWnd,
				           TEXT("Unable to hide self"),
				           TEXT("Error"),
				           MB_ICONERROR | MB_OK);
			}
			else if (target)
			{
				TCHAR title[MAX_PATH];
				TCHAR processName[MAX_PATH];
				BOOL rs = GetProcessNameFromHwnd(hWnd, target, processName, MAX_PATH);
				if (!rs) return 0;
				if (!wcscmp(processName, L"explorer.exe")) return 0;

				// required to show bubble
				iconData.cbSize = NOTIFYICONDATA_V2_SIZE;
				iconData.hWnd = hWnd;
				iconData.uID = UINT(target);
				iconData.uFlags = NIF_ICON | NIF_TIP | NIF_INFO | NIF_MESSAGE;
				iconData.uCallbackMessage = WM_TRAY_ICON_NOTIFYICON;
				GetWindowText(target, title, MAX_PATH);
				_snwprintf_s(iconData.szTip, 64, L"[0x%x] - %s", UINT(target), title);

				HICON icon = (HICON)GetClassLongPtr(target, GCLP_HICONSM);
				iconData.hIcon = icon;
				iconData.hBalloonIcon = icon;
				wcscpy_s(iconData.szInfoTitle, 64, L"Window Hidden");
				_snwprintf_s(iconData.szInfo, MAX_PATH, L"[0x%08x] - %s", UINT(target), title);
				iconData.dwInfoFlags = 0x4; // NIIF_USER
				Shell_NotifyIcon(NIM_ADD, &iconData);
				ShowWindow(target, SW_HIDE);
				ShowWindow(parent, SW_HIDE);
				targetHwnd = target;
			}
			else
			{
				MessageBox(hWnd,
				           TEXT("Unable to identify window, try again"),
				           TEXT("ChildWindowFromPoint Error"),
				           MB_ICONERROR | MB_OK);
			}
			break;
		}
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WM_TRAY_ICON_NOTIFYICON = RegisterWindowMessage(TEXT("NotifyIcon"));
	LPCTSTR MainWndClass = TEXT("Window Hider");

	HWND hWnd;
	MSG msg;

	WNDCLASSEX wc;
	ZeroMemory(&wc, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = &WindowProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
	wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYICON));
	wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TRAYICON));
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = MainWndClass;

	WNDCLASS wc2;
	ZeroMemory(&wc2, sizeof(wc2));
	wc2.style = CS_HREDRAW | CS_VREDRAW;
	wc2.lpfnWndProc = DefWindowProc;
	wc2.hInstance = hInstance;
	wc2.hbrBackground = HBRUSH(COLOR_WINDOW + 1);
	wc2.lpszClassName = TEXT("Window Hider Static");
	wc2.hCursor = LoadCursor(nullptr, IDC_ARROW);

	INITCOMMONCONTROLSEX icc;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&icc);

	if (!RegisterClassEx(&wc) || !RegisterClass(&wc2))
	{
		LPTSTR err = error();
		MessageBox(nullptr, err, TEXT("RegisterClassEx Error"), MB_ICONERROR | MB_OK);
		return 0;
	}

	hWnd =		CreateWindow(MainWndClass, MainWndClass,
		WS_OVERLAPPEDWINDOW ^ (WS_SIZEBOX | WS_MAXIMIZEBOX | WS_MINIMIZEBOX), CW_USEDEFAULT, CW_USEDEFAULT,
		320, 200, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		LPTSTR err = error();
		MessageBox(nullptr, err, TEXT("CreateWindow Error"), MB_ICONERROR | MB_OK);
		return 0;
	}

	ShowWindow(hWnd, nCmdShow);
	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	while (GetMessage(&msg, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return int(msg.wParam);
}
