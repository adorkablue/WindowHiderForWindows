#include "pch.h"

#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>
#include <string>

struct NAME_HANDLE
{
	char* name;
	HWND hwnd;
	BOOL found;
};

BOOL CALLBACK WindowFoundCB(HWND hwnd, LPARAM param)
{
	LPTSTR str = LPTSTR(param);
	GetWindowText(hwnd, str, 256);
	if (!IsWindowVisible(hwnd))
		return TRUE;
	if (wcslen(str) == 0)
		return TRUE;
	printf("0x%08lld -- %ws\n", long long(hwnd), str);
	return TRUE;
}

BOOL CALLBACK FindWindowCB(HWND hwnd, LPARAM param)
{
	struct NAME_HANDLE* nh = reinterpret_cast<struct NAME_HANDLE *>(param);
	if (nh->found)
		return FALSE;
	const LPTSTR str = LPTSTR(malloc(256));
	GetWindowText(hwnd, str, 256);
	char str_narrow[256];
	size_t temp;
	wcstombs_s(&temp, str_narrow, str, 256);
	wchar_t* c = wcsstr(str, GetCommandLine());
	char* r = strstr(str_narrow, nh->name);
	free(str);
	if (c != nullptr)
		return TRUE;
	if (r != nullptr)
	{
		nh->found = TRUE;
		nh->hwnd = hwnd;
	}
	return TRUE;
}

void error(const char* prefix)
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

	fprintf(stderr, "%s%ws\n", prefix, lpMsgBuf);
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		LPTSTR str = LPTSTR(malloc(256));
		EnumWindows(WindowFoundCB, LPARAM(str));
		free(str);
	}
	else
	{
		BOOL hide = TRUE;
		auto* wnd = argv[1];
		auto* result = wnd;
		long long handle = strtol(wnd, &result, 0);

		if (result[0] != '\0')
		{
			struct NAME_HANDLE nh{};
			hide = wnd[0] != '-';
			auto* name = wnd[0] == '-' || wnd[0] == '+' ? wnd + 1 : wnd;
			nh.found = FALSE;
			nh.name = name;
			EnumWindows(FindWindowCB, LPARAM(&nh));
			if (!nh.found)
			{
				fprintf(stderr, "Invalid window handle: %s\n", wnd);
				return 1;
			}
			handle = intptr_t(nh.hwnd);
		}
		if (handle < 0)
		{
			handle = abs(handle);
			hide = FALSE;
		}
		HWND hwnd = HWND(handle);

		const int flag = hide ? SW_HIDE : SW_RESTORE;
		if (!IsWindow(hwnd))
		{
			fprintf(stderr, "%s is not a valid window id\n", wnd);
			return 1;
		}
		NOTIFYICONDATA iconData;
		TCHAR title[256];
		iconData.cbSize = NOTIFYICONDATA_V2_SIZE; // required to show bubble
		iconData.hWnd = hwnd;
		iconData.uID = UINT(hwnd);
		iconData.uFlags = NIF_ICON | NIF_TIP | NIF_INFO;
		GetWindowText(hwnd, title, 256);
		_snwprintf_s(iconData.szTip, 64, L"[0x%x] - %s", UINT(hwnd), title);

		if (hide)
		{
			HICON icon = HICON(GetClassLongPtr(hwnd, GCLP_HICONSM));
			iconData.hIcon = icon;
			iconData.hBalloonIcon = icon;
			wcscpy_s(iconData.szInfoTitle, 64, L"Window Hidden");
			_snwprintf_s(iconData.szInfo, 256, L"[0x%08x] - %s", UINT(hwnd), title);
			iconData.dwInfoFlags = NIIF_INFO;
		}
		Shell_NotifyIcon(hide ? NIM_ADD : NIM_DELETE, &iconData);
		ShowWindow(hwnd, flag);
	}

	return 0;
}
