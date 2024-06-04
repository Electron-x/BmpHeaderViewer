////////////////////////////////////////////////////////////////////////////////////////////////////////
// BmpHeaderViewer.cpp - Copyright (c) 2024 by W. Rolke.
//
// Licensed under the EUPL, Version 1.2 or - as soon they will be approved by
// the European Commission - subsequent versions of the EUPL (the "Licence");
// You may not use this work except in compliance with the Licence.
// You may obtain a copy of the Licence at:
//
// https://joinup.ec.europa.eu/software/page/eupl
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Licence is distributed on an "AS IS" basis,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the Licence for the specific language governing permissions and
// limitations under the Licence.
//
////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

////////////////////////////////////////////////////////////////////////////////////////////////
// Global Variables

#define VERSION TEXT("1.1")
#if defined(_WIN64)
#define PLATFORM TEXT("64-bit")
#else
#define PLATFORM TEXT("32-bit")
#endif

const TCHAR g_szTitle[]     = TEXT("BMP Header Viewer");
const TCHAR g_szAbout[]     = TEXT("BMP Header Viewer ") VERSION TEXT(" (") PLATFORM TEXT(")\r\n")
                              TEXT("Copyright (c) 2024 by W. Rolke\r\n");
const TCHAR g_szDlgClass[]  = TEXT("HeaderViewerDlgClass");
const TCHAR g_szRegPath[]   = TEXT("Software\\Electron\\BmpHeaderViewer");
const TCHAR g_szPlacement[] = TEXT("WindowPlacement");
const TCHAR g_szFntHeight[] = TEXT("FontHeight");
const TCHAR g_szFontEdit[]  = TEXT("Consolas");
const TCHAR g_szFontThumb[] = TEXT("Arial");
const TCHAR g_szWndTop[]    = TEXT("WndTop");
const TCHAR g_szWndBottom[] = TEXT("WndBottom");
const TCHAR g_szWndLeft[]   = TEXT("WndLeft");
const TCHAR g_szWndRight[]  = TEXT("WndRight");

WNDPROC g_lpPrevOutputWndProc = NULL;

HINSTANCE g_hInstance = NULL;
BOOL g_bUseDarkMode = FALSE;

HANDLE g_hDibDefault = NULL;
HANDLE g_hDibThumb = NULL;
HBITMAP g_hBitmapThumb = NULL;
HDRAWDIB g_hDrawDib = NULL;

int g_nIcmMode = ICM_OFF;

////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations of functions included in this code module

// Loads version 6 common controls and registers the window class of the main dialog box
ATOM MyRegisterClass(HINSTANCE hInstance);
// Loads the specified bitmap resource and converts the image to a DIB
HANDLE MyLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName, UINT fuLoad);
// Creates a copy of the command line and returns the argument between the first two quotes
LPTSTR AllocGetCmdLine(LPCTSTR lpCmdLine, LPTSTR* lpszFilename);

// Window function of the application
INT_PTR CALLBACK HeaderViewerDlgProc(HWND, UINT, WPARAM, LPARAM);
// Subclass function of the text output edit control
LRESULT CALLBACK OutputWndProc(HWND, UINT, WPARAM, LPARAM);

// Opens and parses the passed file and outputs the result
BOOL ParseFile(HWND hDlg, LPCTSTR lpszFileName);
// Decompresses a JPEG image and displays it as thumbnail
BOOL ReadJpeg(HWND hDlg, HANDLE hFile, DWORD dwFileSize);
// Displays a hex dump of the first 1024 bytes of a file
BOOL HexDump(HWND hwndEdit, HANDLE hFile, SIZE_T cbLen);

// Draws the thumbnail in a owner-drawn control
BOOL OnDrawItem(HWND hDlg, const LPDRAWITEMSTRUCT lpDrawItem);
// Draws a DIB with StretchDIBits or DrawDibDraw
BOOL DrawDib(HDC hdc, LPBITMAPINFOHEADER lpbi, int xDest, int yDest, int wDest,
	int hDest, int xSrc, int ySrc, int wSrc, int hSrc);
// Draws a DIB section with pre-multiplied alpha on a checkerboard pattern
BOOL AlphaBlendBitmap(HDC hdc, HBITMAP hbm, int xDest, int yDest, int wDest, int hDest,
	int xSrc, int ySrc, int wSrc, int hSrc);
// Draws text over the thumbnail image
BOOL DrawThumbnailText(HDC hdc, LPRECT lpRect, LPCTSTR lpszText, COLORREF color, int mode);
// Creates a font that fits the size of a window
HFONT CreateScaledFont(HDC hdc, LPCRECT lpRect, LPCTSTR lpszFormat);

// Selects the text at the position of the caret
int SelectText(HWND hwndEdit);
// Activates or deactivates line break of the edit control
BOOL SetWordWrap(HWND hDlg, BOOL bWordWrap);

// Loads the dialog placement and the font height of the edit control from the registry
BOOL LoadSettings(LPWINDOWPLACEMENT lpWindowPlacement, LPLONG lpFontHeight);
// Saves the dialog placement and the font height of the edit control to the registry
void SaveSettings(HWND hDlg, HFONT hFont);

// Replacement for SetWindowPlacement, which allows not to call ShowWindow
BOOL MySetWindowPlacement(HWND hWnd, const LPWINDOWPLACEMENT lpwndpl, BOOL bUseShowWindow);

// Saves the size of a window in the property list of the window
void StoreWindowRect(HWND hwnd, LPRECT lprc);
// Retrieves a saved window size from the property list of a window
void RetrieveWindowRect(HWND hwnd, LPRECT lprc);
// Removes previously created entries from the property list of a window
void DeleteWindowRect(HWND hwnd);

// Determines whether a high contrast theme is enabled
BOOL IsHighContrast();
// Determines if Windows 10 1809+ dark mode is enabled
BOOL ShouldAppsUseDarkMode();
// Draws a dark mode title bar
HRESULT UseImmersiveDarkMode(HWND hwndMain, BOOL bUse);
// Activates dark mode for all controls of a window
BOOL AllowDarkModeForWindow(HWND hwndParent, BOOL bAllow);

////////////////////////////////////////////////////////////////////////////////////////////////
// Entry-point function of the application
//
int APIENTRY _tWinMain(__in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPTSTR lpCmdLine, __in int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);

	// Register the window class of the application and specific common control classes
	if (!MyRegisterClass(hInstance))
		return 0;

	g_hInstance = hInstance;
	// Use light or dark mode
	g_bUseDarkMode = IsAppThemed() && ShouldAppsUseDarkMode() && !IsHighContrast();
	// Load the default thumbnail from the resources
	g_hDibDefault = MyLoadBitmap(hInstance, MAKEINTRESOURCE(IDB_HEADERVIEWER), LR_CREATEDIBSECTION);

	LPTSTR lpszFilename = NULL;
	LPCTSTR lpszCommandLine = AllocGetCmdLine(lpCmdLine, &lpszFilename);

	// Create and display the main window
	INT_PTR nResult = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_HEADERVIEWER),
		NULL, (DLGPROC)HeaderViewerDlgProc, (LPARAM)lpszFilename);

	if (lpszCommandLine != NULL)
		MyGlobalFreePtr((LPVOID)lpszCommandLine);

	if (g_hDrawDib != NULL)
		DrawDibClose(g_hDrawDib);

	g_hBitmapThumb = FreeBitmap(g_hBitmapThumb);
	g_hDibThumb = FreeDib(g_hDibThumb);
	g_hDibDefault = FreeDib(g_hDibDefault);

	return (int)nResult;
}

////////////////////////////////////////////////////////////////////////////////////////////////

ATOM MyRegisterClass(HINSTANCE hInstance)
{
	INITCOMMONCONTROLSEX iccex = { 0 };
	iccex.dwSize = sizeof(iccex);
	iccex.dwICC = ICC_BAR_CLASSES;	// For the scrollbar size box
	InitCommonControlsEx(&iccex);

	WNDCLASSEX wcex     = { 0 };
	wcex.cbSize         = sizeof(wcex);
	wcex.style          = CS_DBLCLKS;
	wcex.lpfnWndProc    = DefDlgProc;
	wcex.cbWndExtra     = DLGWINDOWEXTRA;
	wcex.hInstance      = hInstance;
	wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HEADERVIEWER));
	wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
	wcex.lpszMenuName   = NULL;
	wcex.lpszClassName  = g_szDlgClass;
	wcex.hIconSm        = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_HEADERVIEWER), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

	return RegisterClassEx(&wcex);
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE MyLoadBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName, UINT fuLoad)
{
	if (lpBitmapName == NULL)
		return NULL;

	HBITMAP hBitmap = (HBITMAP)LoadImage(hInstance, lpBitmapName, IMAGE_BITMAP, 0, 0, fuLoad);
	if (hBitmap == NULL)
		return NULL;

	HANDLE hDib = ConvertBitmapToDib(hBitmap);

	DeleteBitmap(hBitmap);

	return hDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

LPTSTR AllocGetCmdLine(LPCTSTR lpCmdLine, LPTSTR* lpszFilename)
{
	if (lpCmdLine == NULL || lpCmdLine[0] == '\0')
		return NULL;

	SIZE_T cchCmdLineLen = _tcslen(lpCmdLine);
	LPTSTR lpszCommandLine = (LPTSTR)MyGlobalAllocPtr(GHND, (cchCmdLineLen + 1) * sizeof(TCHAR));
	if (lpszCommandLine == NULL)
		return NULL;

	MyStrNCpy(lpszCommandLine, lpCmdLine, (int)cchCmdLineLen + 1);

	if (lpszFilename == NULL)
		return lpszCommandLine;

	*lpszFilename = lpszCommandLine;
	TCHAR* pch = _tcschr(*lpszFilename, TEXT('\"'));
	if (pch != NULL)
	{
		*lpszFilename = pch + 1;
		pch = _tcschr(*lpszFilename, TEXT('\"'));
		if (pch != NULL)
			*pch = TEXT('\0');
	}

	return lpszCommandLine;
}

////////////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK HeaderViewerDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static int      s_nDpi = USER_DEFAULT_SCREEN_DPI;
	static POINT    s_ptMinTrackSize = { 0 };
	static BOOL     s_IsListener = FALSE;
	static BOOL     s_bWordWrap = FALSE;
	static HBRUSH   s_hbrBkgnd = NULL;
	static HFONT    s_hfontDlgOrig = NULL;
	static HFONT    s_hfontDlgCurr = NULL;
	static HFONT    s_hfontEditBox = NULL;
	static HWND     s_hwndSizeBox = NULL;
	static DWORD    s_dwFilterIndex = 1;
	static TCHAR    s_szFileName[MY_OFN_MAX_PATH];

	switch (message)
	{
		case WM_DRAWITEM:
			return OnDrawItem(hDlg, (const LPDRAWITEMSTRUCT)lParam);

		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
			if (g_bUseDarkMode)
			{
				SetTextColor((HDC)wParam, RGB_DARKMODE_TEXTCOLOR);
				SetBkColor((HDC)wParam, RGB_DARKMODE_BKCOLOR);
				if (s_hbrBkgnd == NULL)
					s_hbrBkgnd = CreateSolidBrush(RGB_DARKMODE_BKCOLOR);
				return (INT_PTR)s_hbrBkgnd;
			}
			else if (GetDlgItem(hDlg, IDC_OUTPUT) == (HWND)lParam)
			{
				SetTextColor((HDC)wParam, GetSysColor(COLOR_WINDOWTEXT));
				SetBkColor((HDC)wParam, GetSysColor(COLOR_WINDOW));
				return (INT_PTR)GetSysColorBrush(COLOR_WINDOW);
			}

		case WM_CTLCOLORBTN:
			if (GetDlgItem(hDlg, IDC_THUMB) == (HWND)lParam)
				return (INT_PTR)GetStockBrush(NULL_BRUSH);

		case WM_QUERYNEWPALETTE:
		{
			UINT colChanged = 0;
			if (g_hDibDefault != NULL || g_hDibThumb != NULL)
			{
				HPALETTE hpal = CreateDibPalette(g_hDibThumb != NULL ? g_hDibThumb : g_hDibDefault);
				if (hpal != NULL)
				{
					HDC hdc = GetDC(hDlg);
					HPALETTE hpalOld = SelectPalette(hdc, hpal, FALSE);
					colChanged = RealizePalette(hdc);
					if (hpalOld != NULL)
						SelectPalette(hdc, hpalOld, FALSE);
					ReleaseDC(hDlg, hdc);
					DeletePalette(hpal);
					if (colChanged != 0)
						InvalidateRect(hDlg, NULL, TRUE);
				}
			}
			return (colChanged);
		}

		case WM_PALETTECHANGED:
		{
			if (wParam != (WPARAM)hDlg && (g_hDibDefault != NULL || g_hDibThumb != NULL))
			{
				HPALETTE hpal = CreateDibPalette(g_hDibThumb != NULL ? g_hDibThumb : g_hDibDefault);
				if (hpal != NULL)
				{
					HDC hdc = GetDC(hDlg);
					HPALETTE hpalOld = SelectPalette(hdc, hpal, TRUE);
					UINT colChanged = RealizePalette(hdc);
					if (colChanged != 0)
						UpdateColors(hdc);
					if (hpalOld != NULL)
						SelectPalette(hdc, hpalOld, TRUE);
					ReleaseDC(hDlg, hdc);
					DeletePalette(hpal);
				}
			}
			return FALSE;
		}

		case WM_DPICHANGED:
		{
			HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
			if (hwndEdit == NULL || s_hfontEditBox == NULL)
				return FALSE;

			LOGFONT lf = { 0 };
			GetObject(s_hfontEditBox, sizeof(LOGFONT), (LPVOID)&lf);

			int nDpiNew = HIWORD(wParam);
			lf.lfHeight = MulDiv(lf.lfHeight, nDpiNew, s_nDpi);
			s_nDpi = nDpiNew;

			HFONT hfontScaled = CreateFontIndirect(&lf);
			if (hfontScaled == NULL)
				return FALSE;

			DeleteFont(s_hfontEditBox);
			SetWindowFont(hwndEdit, hfontScaled, TRUE);
			s_hfontEditBox = hfontScaled;

			return FALSE;
		}

		case WM_MOUSEWHEEL:
		{
			if ((GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) == 0)
				return FALSE;

			HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
			if (hwndEdit == NULL || s_hfontEditBox == NULL)
				return FALSE;

			static int nDelta = 0;
			nDelta += GET_WHEEL_DELTA_WPARAM(wParam);
			if (abs(nDelta) < WHEEL_DELTA)
				return FALSE;

			int nHeightUnits = nDelta / WHEEL_DELTA;
			nDelta = 0;

			LOGFONT lf = { 0 };
			GetObject(s_hfontEditBox, sizeof(LOGFONT), (LPVOID)&lf);

			LONG lfHeight = abs(lf.lfHeight) + nHeightUnits;
			lfHeight = min(max(6, lfHeight), 72);
			lf.lfHeight = lf.lfHeight < 0 ? -lfHeight : lfHeight;

			HFONT hfontScaled = CreateFontIndirect(&lf);
			if (hfontScaled == NULL)
				return FALSE;

			DeleteFont(s_hfontEditBox);
			SetWindowFont(hwndEdit, hfontScaled, TRUE);
			s_hfontEditBox = hfontScaled;

			return FALSE;
		}

		case WM_GETMINMAXINFO:
		{
			LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
			lpmmi->ptMinTrackSize = s_ptMinTrackSize;
			return FALSE;
		}

		case WM_SIZE:
		{ // Processing of the child windows of a resizable dialog box
			if (wParam == SIZE_MINIMIZED || wParam == SIZE_MAXHIDE || wParam == SIZE_MAXSHOW)
				return FALSE;

			// Determine the new dimensions
			RECT rc, rcInit;
			if (!GetClientRect(hDlg, &rc))
				return FALSE;
			RetrieveWindowRect(hDlg, &rcInit);

			float xMult = (float)(rc.right - rc.left) / (rcInit.right - rcInit.left);
			float yMult = (float)(rc.bottom - rc.top) / (rcInit.bottom - rcInit.top);

			// Fill the LOGFONT structure with the old font
			LOGFONT lf = { 0 };
			if (s_hfontDlgOrig != NULL)
				GetObject(s_hfontDlgOrig, sizeof(LOGFONT), (LPVOID)&lf);

			// Scale the font
			lf.lfHeight = (int)((yMult * lf.lfHeight) + ((lf.lfHeight < 0) ? (-0.5) : (0.5)));
			if (lf.lfWidth != 0) lf.lfWidth = (int)((xMult * lf.lfWidth) + 0.5);

			// Delete the old font and create the new one
			if (s_hfontDlgCurr != NULL)
				DeleteFont(s_hfontDlgCurr);

			s_hfontDlgCurr = CreateFontIndirect(&lf);

			// Assign the new font to the dialog window
			if (s_hfontDlgCurr != NULL)
				SetWindowFont(hDlg, s_hfontDlgCurr, FALSE);

			LONG lThumbnailMax = 0;
			HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
			HWND hwndThumb = GetDlgItem(hDlg, IDC_THUMB);
			HWND hwndWrapBtn = GetDlgItem(hDlg, IDC_WORDWRAP);

			// Count all child windows for DeferWindowPos
			int nNumWindows = 0;
			HWND hwndChild = GetWindow(hDlg, GW_CHILD);
			while (hwndChild)
			{
				nNumWindows++;
				hwndChild = GetNextSibling(hwndChild);
			}

			// Reposition all child windows
			HDWP hWinPosInfo = BeginDeferWindowPos(nNumWindows);
			if (hWinPosInfo != NULL)
			{
				// Start with the first child window and run through all controls
				hwndChild = GetWindow(hDlg, GW_CHILD);
				while (hwndChild)
				{
					// Assign the new font to the control.
					// The Edit window keeps the globally defined font size.
					if (s_hfontDlgCurr != NULL && hwndChild != hwndEdit)
						SetWindowFont(hwndChild, s_hfontDlgCurr, FALSE);

					// Determine the initial size of the control
					RetrieveWindowRect(hwndChild, &rc);
					// Calculate the new window size accordingly
					rc.top    = (int)((yMult * rc.top)    + 0.5);
					rc.bottom = (int)((yMult * rc.bottom) + 0.5);
					rc.left   = (int)((xMult * rc.left)   + 0.5);
					rc.right  = (int)((xMult * rc.right)  + 0.5);

					// Maximum height for the thumbnail
					if (hwndChild == hwndWrapBtn)
						lThumbnailMax = rc.bottom;

					// Adjust the size of the child window. The thumbnail
					// window and the size box require special handling.
					if (hwndChild == hwndThumb)
						hWinPosInfo = DeferWindowPos(hWinPosInfo, hwndChild, NULL, rc.left,
							max(lThumbnailMax, rc.bottom - (rc.right - rc.left)), rc.right - rc.left,
							min(rc.bottom - lThumbnailMax, rc.right - rc.left), SWP_NOZORDER | SWP_NOACTIVATE);
					else if (hwndChild == s_hwndSizeBox)
					{
						int cx = GetSystemMetrics(SM_CXVSCROLL);
						int cy = GetSystemMetrics(SM_CYHSCROLL);
						if (wParam == SIZE_MAXIMIZED)
							ShowWindow(hwndChild, SW_HIDE);
						else if (!IsWindowVisible(hwndChild))
							ShowWindow(hwndChild, SW_SHOW);
						hWinPosInfo = DeferWindowPos(hWinPosInfo, hwndChild, NULL,
							rc.right - cx, rc.bottom - cy, cx, cy, SWP_NOZORDER | SWP_NOACTIVATE);
					}
					else
						hWinPosInfo = DeferWindowPos(hWinPosInfo, hwndChild, NULL, rc.left, rc.top,
							(rc.right - rc.left), (rc.bottom - rc.top), SWP_NOZORDER | SWP_NOACTIVATE);

					// Determine the next child window
					hwndChild = GetNextSibling(hwndChild);
				}

				if (hWinPosInfo != NULL)
					EndDeferWindowPos(hWinPosInfo);
			}

			return FALSE;
		}

		case WM_CLIPBOARDUPDATE:
		{
			EnableButton(hDlg, IDC_PASTE, IDC_OPEN,
				IsClipboardFormatAvailable(CF_DIB) ||
				IsClipboardFormatAvailable(CF_DIBV5) ||
				IsClipboardFormatAvailable(CF_HDROP));
			return FALSE;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
				case IDCANCEL:
				{
					SaveSettings(hDlg, s_hfontEditBox);

					if (s_IsListener)
						s_IsListener = !RemoveClipboardFormatListener(hDlg);

					DeleteWindowRect(hDlg);
					HWND hwndChild = GetWindow(hDlg, GW_CHILD);
					while (hwndChild)
					{
						DeleteWindowRect(hwndChild);
						hwndChild = GetNextSibling(hwndChild);
					}

					if (s_hwndSizeBox != NULL)
					{
						DestroyWindow(s_hwndSizeBox);
						s_hwndSizeBox = NULL;
					}

					HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
					if (hwndEdit != NULL && g_lpPrevOutputWndProc != NULL)
						SubclassWindow(hwndEdit, g_lpPrevOutputWndProc);

					if (s_hfontEditBox != NULL)
					{
						DeleteFont(s_hfontEditBox);
						s_hfontEditBox = NULL;
					}

					if (s_hfontDlgCurr != NULL)
					{
						DeleteFont(s_hfontDlgCurr);
						s_hfontDlgCurr = NULL;
					}

					if (s_hbrBkgnd != NULL)
					{
						DeleteBrush(s_hbrBkgnd);
						s_hbrBkgnd = NULL;
					}

					EndDialog(hDlg, LOWORD(wParam));
				}
				return FALSE;

				case IDC_OPEN:
				{
					if (GetFileName(hDlg, s_szFileName, _countof(s_szFileName), &s_dwFilterIndex))
					{
						HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
						HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);

						ClearOutputWindow(hwndEdit);

						if (ParseFile(hDlg, s_szFileName) && GetFocus() != hwndEdit)
						{
							SetFocus(hwndEdit);
							int nLen = Edit_GetTextLength(hwndEdit);
							Edit_SetSel(hwndEdit, nLen, nLen);
						}

						SetCursor(hOldCursor);
					}
				}
				return FALSE;

				case IDC_COPY:
				{ // Copy text from the edit control to the clipboard
					HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
					DWORD dwSelection = Edit_GetSel(hwndEdit);
					if (HIWORD(dwSelection) != LOWORD(dwSelection))
						SendMessage(hwndEdit, WM_COPY, 0, 0);
					else
					{
						Edit_SetSel(hwndEdit, 0, -1);
						SendMessage(hwndEdit, WM_COPY, 0, 0);
					}
				}
				return FALSE;

				case IDC_PASTE:
				{
					if (IsClipboardFormatAvailable(CF_HDROP))
					{ // Open up to 100 files of a list pasted from the clipboard
						if (!OpenClipboard(hDlg))
							return FALSE;

						HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
						if (hDrop == NULL)
						{
							CloseClipboard();
							return FALSE;
						}

						LPVOID lpDropFiles = GlobalLock(hDrop);
						if (lpDropFiles == NULL)
						{
							CloseClipboard();
							return FALSE;
						}

						UINT nFiles = DragQueryFile(hDrop, (UINT)-1, NULL, 0);
						if (nFiles > 100)
							nFiles = 100;

						if (nFiles == 0)
						{
							GlobalUnlock(hDrop);
							CloseClipboard();
							return FALSE;
						}

						HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
						HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);

						ClearOutputWindow(hwndEdit);

						BOOL bSuccess = FALSE;
						for (UINT iFile = 0; iFile < nFiles; iFile++)
						{
							DragQueryFile(hDrop, iFile, s_szFileName, _countof(s_szFileName));

							if (iFile > 0)
								Edit_ReplaceSel(hwndEdit, TEXT("\r\n"));

							bSuccess = ParseFile(hDlg, s_szFileName) || bSuccess;
						}

						GlobalUnlock(hDrop);
						CloseClipboard();

						if (bSuccess && GetFocus() != hwndEdit)
						{
							SetFocus(hwndEdit);
							int nLen = Edit_GetTextLength(hwndEdit);
							Edit_SetSel(hwndEdit, nLen, nLen);
						}

						SetCursor(hOldCursor);
					}
					else if (IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5))
					{ // Paste a DIBv3 or a DIBv5 from the clipboard
						if (!OpenClipboard(hDlg))
							return FALSE;

						UINT uFormat = 0;
						// Determine the most descriptive clipboard format
						while ((uFormat = EnumClipboardFormats(uFormat)))
						{
							if (uFormat == CF_DIB || uFormat == CF_DIBV5)
								break;
						}

						HANDLE hDib = GetClipboardData(uFormat);
						if (hDib == NULL)
						{ // You cannot paste a DIB section from another application
							CloseClipboard();
							TCHAR szText[512];
							if (LoadString(g_hInstance, IDP_CLIPBOARD, szText, _countof(szText)) > 0)
								MessageBox(hDlg, szText, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
							return FALSE;
						}

						HANDLE hNewDib = CreateDibFromClipboardDib(hDib);
						if (hNewDib == NULL)
						{
							CloseClipboard();
							return FALSE;
						}

						CloseClipboard();

						// Release old thumbnail
						ResetThumbnail(GetDlgItem(hDlg, IDC_THUMB));
						LoadString(g_hInstance, IDS_UNNAMED, s_szFileName, _countof(s_szFileName));

						HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
						HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);

						ClearOutputWindow(hwndEdit);

						OutputText(hwndEdit, TEXT("Format:\t\t"));
						if (uFormat == CF_DIB)
							OutputText(hwndEdit, TEXT("DIB"));
						else if (uFormat == CF_DIBV5)
							OutputText(hwndEdit, TEXT("DIBV5"));
						else
							OutputTextFmt(hwndEdit, TEXT("%u"), uFormat);
						OutputText(hwndEdit, TEXT("\r\n"));

						SetLastError(ERROR_SUCCESS);
						BOOL bSuccess = ParseDIBitmap(hDlg, hNewDib);
						if (bSuccess)
							ReplaceThumbnail(GetDlgItem(hDlg, IDC_THUMB), hNewDib);
						else
						{
							DWORD dwError = GetLastError();
							GlobalFree(hNewDib);
							if (dwError != ERROR_SUCCESS)
								OutputErrorMessage(hwndEdit, dwError);
						}

						OutputText(hwndEdit, g_szSepThick);
						EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, bSuccess);

						if (bSuccess && GetFocus() != hwndEdit)
						{
							SetFocus(hwndEdit);
							int nLen = Edit_GetTextLength(hwndEdit);
							Edit_SetSel(hwndEdit, nLen, nLen);
						}

						SetCursor(hOldCursor);
					}
				}
				return FALSE;

				case IDC_WORDWRAP:
				{
					HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
					if (s_bWordWrap)
					{
						if (SetWordWrap(hDlg, FALSE))
							s_bWordWrap = FALSE;
					}
					else
					{
						if (SetWordWrap(hDlg, TRUE))
							s_bWordWrap = TRUE;
					}
					SetCursor(hOldCursor);
				}
				return FALSE;

				case IDC_THUMB_ICM:
				{
					ColorMatchUI(hDlg);
				}
				return FALSE;

				case IDC_THUMB_COPY:
				{ // Copy the current thumbnail to the clipboard
					HANDLE hDib = g_hDibThumb ? g_hDibThumb : g_hDibDefault;
					if (hDib == NULL)
						return FALSE;

					UINT uFormat = CF_DIB;
					HANDLE hNewDib = CreateClipboardDib(hDib, &uFormat);

					if (!OpenClipboard(hDlg))
					{
						GlobalFree(hNewDib);
						return FALSE;
					}

					HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
					if (!EmptyClipboard())
					{
						CloseClipboard();
						SetCursor(hOldCursor);
						GlobalFree(hNewDib);
						return FALSE;
					}

					if (SetClipboardData(uFormat, hNewDib) == NULL)
						GlobalFree(hNewDib);

					CloseClipboard();
					SetCursor(hOldCursor);
				}
				return FALSE;

				case IDC_THUMB_SAVE:
				{ // Save the current thumbnail
					DWORD dwFilterIndex = 1;
					TCHAR szFileName[MY_OFN_MAX_PATH];
					MyStrNCpy(szFileName, s_szFileName, _countof(szFileName));

					if (g_hDibThumb != NULL)
					{
						LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)GlobalLock(g_hDibThumb);
						if (lpbiv5 != NULL)
						{
							if (IS_WIN50_DIB(lpbiv5) && lpbiv5->bV5CSType == PROFILE_EMBEDDED &&
								lpbiv5->bV5ProfileData != 0 && lpbiv5->bV5ProfileSize != 0)
								dwFilterIndex = 2;	// Add the option to save the ICC profile
							GlobalUnlock(g_hDibThumb);
						}
					}

					if (GetFileName(hDlg, szFileName, _countof(szFileName), &dwFilterIndex, TRUE))
					{
						BOOL bSuccess = FALSE;
						HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
						if (dwFilterIndex == 2)
							bSuccess = SaveProfile(szFileName, g_hDibThumb);
						else
							bSuccess = SaveBitmap(szFileName, g_hDibThumb ? g_hDibThumb : g_hDibDefault);
						SetCursor(hOldCursor);

						if (!bSuccess)
						{
							TCHAR szText[512];
							if (LoadString(g_hInstance, IDP_WRITEFILE, szText, _countof(szText)) > 0)
								MessageBox(hDlg, szText, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
						}
					}
				}
				return FALSE;
			}
			return FALSE;

		case WMU_FILEOPEN:
		{ // Open the BMP file passed by program argument
			HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
			HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);

			if (ParseFile(hDlg, (LPCTSTR)lParam))
			{
				SetFocus(hwndEdit);
				int nLen = Edit_GetTextLength(hwndEdit);
				Edit_SetSel(hwndEdit, nLen, nLen);
			}

			if ((GetWindowStyle(hwndEdit) & ES_READONLY) == 0)
				Edit_SetReadOnly(hwndEdit, TRUE);

			SetCursor(hOldCursor);
		}
		return TRUE;

		case WM_DROPFILES:
		{ // Open up to 100 BMP files transferred via drag-and-drop
			HDROP hDrop = (HDROP)wParam;
			if (hDrop == NULL)
				return FALSE;

			if (IsMinimized(hDlg))
				ShowWindow(hDlg, SW_SHOWNORMAL);
			SetForegroundWindow(hDlg);

			UINT nFiles = DragQueryFile(hDrop, (UINT)-1, NULL, 0);
			if (nFiles > 100)
				nFiles = 100;

			if (nFiles == 0)
			{
				DragFinish(hDrop);
				return FALSE;
			}

			HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
			HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);

			ClearOutputWindow(hwndEdit);

			BOOL bSuccess = FALSE;
			for (UINT iFile = 0; iFile < nFiles; iFile++)
			{
				DragQueryFile(hDrop, iFile, s_szFileName, _countof(s_szFileName));

				if (iFile > 0)
					Edit_ReplaceSel(hwndEdit, TEXT("\r\n"));

				bSuccess = ParseFile(hDlg, s_szFileName) || bSuccess;
			}

			DragFinish(hDrop);

			if (bSuccess && GetFocus() != hwndEdit)
			{
				SetFocus(hwndEdit);
				int nLen = Edit_GetTextLength(hwndEdit);
				Edit_SetSel(hwndEdit, nLen, nLen);
			}

			SetCursor(hOldCursor);
		}
		return TRUE;

		case WM_CONTEXTMENU:
		{ // Display the context menu for the dialog box or the thumbnail
			HWND hwndSender = (HWND)wParam;
			HWND hwndThumb = GetDlgItem(hDlg, IDC_THUMB);

			if (hwndSender == hDlg || hwndSender == hwndThumb)
			{
				int x = GET_X_LPARAM(lParam);
				int y = GET_Y_LPARAM(lParam);

				if (x == -1 || y == -1)
				{
					RECT rc;
					GetWindowRect(hwndSender, &rc);
					x = (rc.left + rc.right) / 2;
					y = (rc.top + rc.bottom) / 2;
				}

				LPTSTR lpszMenuName = MAKEINTRESOURCE(IDR_POPUP_DIALOG);
				if (hwndSender == hwndThumb)
					lpszMenuName = MAKEINTRESOURCE(IDR_POPUP_THUMB);

				HMENU hmenuPopup = LoadMenu(g_hInstance, lpszMenuName);
				if (hmenuPopup == NULL)
					return FALSE;

				HMENU hmenuTrackPopup = GetSubMenu(hmenuPopup, 0);
				if (hmenuTrackPopup == NULL)
				{
					DestroyMenu(hmenuPopup);
					return FALSE;
				}

				if (hwndSender == hDlg)
				{
					if (!IsClipboardFormatAvailable(CF_DIB) &&
						!IsClipboardFormatAvailable(CF_DIBV5) &&
						!IsClipboardFormatAvailable(CF_HDROP))
						EnableMenuItem(hmenuTrackPopup, IDC_PASTE, MF_BYCOMMAND | MF_GRAYED);
				}
				else if (hwndSender == hwndThumb)
				{
					if (g_hDibThumb == NULL)
					{
						EnableMenuItem(hmenuTrackPopup, IDC_THUMB_COPY, MF_BYCOMMAND | MF_GRAYED);
						EnableMenuItem(hmenuTrackPopup, IDC_THUMB_SAVE, MF_BYCOMMAND | MF_GRAYED);
					}
				}

				MENUINFO mi = { 0 };
				mi.cbSize = sizeof(mi);
				mi.fMask = MIM_STYLE;
				mi.dwStyle = MNS_NOCHECK;
				SetMenuInfo(hmenuTrackPopup, &mi);

				BOOL bSuccess = TrackPopupMenu(hmenuTrackPopup,
					TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON, x, y, 0, hDlg, NULL);

				DestroyMenu(hmenuPopup);

				return bSuccess;
			}
		}
		return FALSE;

		case WM_HELP:
		{
			static BOOL bAboutBox = FALSE;
			if (!bAboutBox)
			{
				MSGBOXPARAMS mbp = { 0 };
				mbp.cbSize       = sizeof(MSGBOXPARAMS);
				mbp.hwndOwner    = hDlg;
				mbp.hInstance    = g_hInstance;
				mbp.lpszText     = g_szAbout;
				mbp.lpszCaption  = g_szTitle;
				mbp.dwStyle      = MB_OK | MB_USERICON;
				mbp.lpszIcon     = MAKEINTRESOURCE(IDI_HEADERVIEWER);
				mbp.dwLanguageId = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

				bAboutBox = TRUE;
				MessageBoxIndirect(&mbp);
				bAboutBox = FALSE;
			}
		}
		return TRUE;

		case WM_SETTINGCHANGE:
		case WM_THEMECHANGED:
		{
			if (message == WM_SETTINGCHANGE &&
				lParam != NULL && _tcscmp((LPCTSTR)lParam, TEXT("ImmersiveColorSet")) != 0)
				return FALSE;

			BOOL bShouldUseDarkMode = (IsAppThemed() && ShouldAppsUseDarkMode() && !IsHighContrast());
			if ((g_bUseDarkMode && !bShouldUseDarkMode) || (!g_bUseDarkMode && bShouldUseDarkMode))
			{
				g_bUseDarkMode = !g_bUseDarkMode;

				UseImmersiveDarkMode(hDlg, g_bUseDarkMode);
				AllowDarkModeForWindow(hDlg, g_bUseDarkMode);
			}
		}
		return FALSE;

		case WM_INITDIALOG:
		{
			s_bWordWrap = FALSE;
			SetWindowText(hDlg, g_szTitle);

			// Load settings
			LONG lFontHeight = 0;
			WINDOWPLACEMENT wpl = { 0 };
			if (!IsKeyDown(VK_SHIFT))
				LoadSettings(&wpl, &lFontHeight);

			// Determine logical DPI
			HDC hdc = GetDC(hDlg);
			if (hdc != NULL)
			{
				s_nDpi = GetDeviceCaps(hdc, LOGPIXELSY);
				ReleaseDC(hDlg, hdc);
			}

			// Determine dialog box font (see WM_SIZE)
			s_hfontDlgOrig = GetWindowFont(hDlg);
			if (s_hfontDlgOrig == NULL)
				s_hfontDlgOrig = GetStockFont(DEFAULT_GUI_FONT);

			RECT rc;
			// Position the thumbnail window
			HWND hwndThumb = GetDlgItem(hDlg, IDC_THUMB);
			GetWindowRect(hwndThumb, &rc);
			ScreenToClient(hDlg, (LPPOINT)&rc.left);
			ScreenToClient(hDlg, (LPPOINT)&rc.right);
			MoveWindow(hwndThumb, rc.left, rc.bottom - (rc.right - rc.left),
				rc.right - rc.left, rc.right - rc.left, FALSE);

			// Set the title for the default thumbnail
			SetWindowText(hwndThumb, TEXT(""));

			// Set the minimum size of the dialog box
			GetWindowRect(hDlg, &rc);
			s_ptMinTrackSize.x = (rc.right - rc.left) * 3 / 4;
			s_ptMinTrackSize.y = (rc.bottom - rc.top) * 3 / 4;

			// Save size of main window as property
			GetClientRect(hDlg, &rc);
			StoreWindowRect(hDlg, &rc);

			// Create size box bottom right
			int cx = GetSystemMetrics(SM_CXVSCROLL);
			int cy = GetSystemMetrics(SM_CYHSCROLL);
			DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_GROUP |
				SBS_SIZEBOX | SBS_SIZEBOXBOTTOMRIGHTALIGN | SBS_SIZEGRIP;
			s_hwndSizeBox = CreateWindow(TEXT("Scrollbar"), NULL, dwStyle,
				rc.right - cx, rc.bottom - cy, cx, cy, hDlg, (HMENU)-1, g_hInstance, NULL);

			// Save size of all child windows as property
			HWND hwndChild = GetWindow(hDlg, GW_CHILD);
			while (hwndChild)
			{
				GetWindowRect(hwndChild, &rc);
				ScreenToClient(hDlg, (LPPOINT)&rc.left);
				ScreenToClient(hDlg, (LPPOINT)&rc.right);
				StoreWindowRect(hwndChild, &rc);
				hwndChild = GetNextSibling(hwndChild);
			}

			// Subclass the text output window
			HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
			g_lpPrevOutputWndProc = SubclassWindow(hwndEdit, OutputWndProc);

			// Set the memory limit of the text output window
			Edit_LimitText(hwndEdit, 0);

			// Set the font of the text output window dpi-aware
			LOGFONT lf = { 0 };
			lf.lfHeight = -11 * s_nDpi / 72;
			lf.lfWeight = FW_NORMAL;
			lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
			MyStrNCpy(lf.lfFaceName, g_szFontEdit, _countof(lf.lfFaceName));

			BOOL bShowFontScalingInfo = FALSE;
			if (lFontHeight != 0)
			{
				// Check if the font size stored in the registry
				// is too different from the default value
				if (abs(lFontHeight - lf.lfHeight) > 6)
					bShowFontScalingInfo = TRUE;
				// Use the saved font height anyway
				lf.lfHeight = lFontHeight;
			}

			if (s_hfontEditBox != NULL)
				DeleteFont(s_hfontEditBox);
			s_hfontEditBox = CreateFontIndirect(&lf);
			if (s_hfontEditBox != NULL)
				SetWindowFont(hwndEdit, s_hfontEditBox, FALSE);

			// Windows 10 1809+ dark mode
			if (g_bUseDarkMode)
			{
				UseImmersiveDarkMode(hDlg, g_bUseDarkMode);
				AllowDarkModeForWindow(hDlg, g_bUseDarkMode);
			}

			// Set the dialog box position and size
			MySetWindowPlacement(hDlg, &wpl, FALSE);

			// Register this dialog box to receive WM_CLIPBOARDUPDATE.
			// We need this message to enable/disable the Paste button.
			s_IsListener = AddClipboardFormatListener(hDlg);
			if (s_IsListener)
				PostMessage(hDlg, WM_CLIPBOARDUPDATE, 0, 0);

			// Disable the Copy bitmap button
			EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, FALSE);

			if ((LPCTSTR)lParam != NULL && ((LPCTSTR)lParam)[0] != TEXT('\0'))
			{ // Open a BMP file passed by program argument
				ShortenPath((LPCTSTR)lParam, s_szFileName, _countof(s_szFileName));
				PostMessage(hDlg, WMU_FILEOPEN, 0, lParam);
			}
			else
			{
				LoadString(g_hInstance, IDS_UNNAMED, s_szFileName, _countof(s_szFileName));
				// Display the standard info text
				Edit_ReplaceSel(hwndEdit, g_szAbout);
				// Show a hint to scale the font if needed
				TCHAR szText[OUTPUT_LEN];
				if (bShowFontScalingInfo && LoadString(g_hInstance,
					IDS_FONTSIZEINFO, szText, _countof(szText)) > 0)
					Edit_ReplaceSel(hwndEdit, szText);
				// Ensure that the edit control is set to read-only
				if ((GetWindowStyle(hwndEdit) & ES_READONLY) == 0)
					Edit_SetReadOnly(hwndEdit, TRUE);
			}

			// Allow drag-and-drop
			DragAcceptFiles(hDlg, TRUE);
		}
		return TRUE;

		case WM_CLOSE:
		{
			SaveSettings(hDlg, s_hfontEditBox);

			if (s_IsListener)
				s_IsListener = !RemoveClipboardFormatListener(hDlg);

			DeleteWindowRect(hDlg);
			HWND hwndChild = GetWindow(hDlg, GW_CHILD);
			while (hwndChild)
			{
				DeleteWindowRect(hwndChild);
				hwndChild = GetNextSibling(hwndChild);
			}

			if (s_hwndSizeBox != NULL)
			{
				DestroyWindow(s_hwndSizeBox);
				s_hwndSizeBox = NULL;
			}

			HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
			if (hwndEdit != NULL && g_lpPrevOutputWndProc != NULL)
				SubclassWindow(hwndEdit, g_lpPrevOutputWndProc);

			if (s_hfontEditBox != NULL)
			{
				DeleteFont(s_hfontEditBox);
				s_hfontEditBox = NULL;
			}

			if (s_hfontDlgCurr != NULL)
			{
				DeleteFont(s_hfontDlgCurr);
				s_hfontDlgCurr = NULL;
			}

			if (s_hbrBkgnd != NULL)
			{
				DeleteBrush(s_hbrBkgnd);
				s_hbrBkgnd = NULL;
			}

			EndDialog(hDlg, 0);
		}
		return FALSE;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK OutputWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Improved text selection
	if (uMsg == WM_LBUTTONDBLCLK && SelectText(hwnd) != 0)
		return FALSE;

	if (uMsg == WM_KEYDOWN)
	{
		// CTRL+A to select the entire text
		if (wParam == 'A' && IsKeyDown(VK_CONTROL))
		{
			Edit_SetSel(hwnd, 0, -1);
			return FALSE;
		}

		// CTRL+V and SHIFT+INS to paste a DIB or a list of files from the clipboard
		if ((wParam == 'V' && IsKeyDown(VK_CONTROL)) ||
			(wParam == VK_INSERT && IsKeyDown(VK_SHIFT)))
		{
			PostMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_PASTE, 1), 0);
			return FALSE;
		}
	}

	return CallWindowProc(g_lpPrevOutputWndProc, hwnd, uMsg, wParam, lParam);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL ParseFile(HWND hDlg, LPCTSTR lpszFileName)
{
	if (hDlg == NULL || lpszFileName == NULL || lpszFileName[0] == TEXT('\0'))
		return FALSE;

	HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
	if (hwndEdit == NULL)
		return FALSE;

	// Release old thumbnail
	ResetThumbnail(GetDlgItem(hDlg, IDC_THUMB));

	// Output file name
	LPCTSTR lpsz = _tcsrchr(lpszFileName, TEXT('\\'));
	if (lpsz == NULL)
		lpsz = _tcsrchr(lpszFileName, TEXT('/'));
	OutputTextFmt(hwndEdit, TEXT("File Name:\t%s\r\n"),
		lpsz != NULL && *(lpsz + 1) != TEXT('\0') ? lpsz + 1 : lpszFileName);

	// Obtain attribute information about the file
	WIN32_FILE_ATTRIBUTE_DATA wfad = { 0 };
	if (!GetFileAttributesEx(lpszFileName, GetFileExInfoStandard, &wfad))
	{
		OutputErrorMessage(hwndEdit, GetLastError());
		OutputText(hwndEdit, g_szSepThick);
		// Don't deactivate the Copy button except in the event
		// of an error, to avoid a brief flashing of the button
		EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, FALSE);
		return FALSE;
	}

	// Output file size
	if ((wfad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		TCHAR szOutput[OUTPUT_LEN];
		OutputText(hwndEdit, TEXT("File Size:\t"));
		if (wfad.nFileSizeHigh > 0)
			OutputText(hwndEdit, TEXT("> 4 GB"));
		else if (FormatByteSize(wfad.nFileSizeLow, szOutput, _countof(szOutput)))
			OutputText(hwndEdit, szOutput);
		OutputText(hwndEdit, TEXT("\r\n"));
	}

	// Open the file
	HANDLE hFile = CreateFile(lpszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		OutputErrorMessage(hwndEdit, GetLastError());
		OutputText(hwndEdit, g_szSepThick);
		EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, FALSE);
		return FALSE;
	}

	// Read file signature
	BYTE achMagic[2] = { 0 };
	if (!MyReadFile(hFile, (LPVOID)&achMagic, sizeof(achMagic)))
	{
		OutputTextFromID(hwndEdit, IDS_MAGIC);
		OutputText(hwndEdit, g_szSepThick);
		EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, FALSE);
		CloseHandle(hFile);
		return FALSE;
	}

	BOOL bSuccess = FALSE;
	if (memcmp(achMagic, "BM", 2) == 0 || memcmp(achMagic, "BA", 2) == 0)
	{ // Bitmap or Bitmap Array
		SetLastError(ERROR_SUCCESS);
		bSuccess = ParseBitmap(hDlg, hFile, wfad.nFileSizeLow);
		if (!bSuccess)
		{
			DWORD dwError = GetLastError();
			if (dwError != ERROR_SUCCESS)
				OutputErrorMessage(hwndEdit, dwError);
		}
		EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, bSuccess);
	}
	else if (achMagic[0] == 0xFF && achMagic[1] == 0xD8)
	{ // JPEG
		bSuccess = ReadJpeg(hDlg, hFile, wfad.nFileSizeLow);
		EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, bSuccess);
	}
	else
	{ // Unsupported file format
		OutputTextFromID(hwndEdit, IDS_MAGIC);
		EnableButton(hDlg, IDC_THUMB_COPY, IDC_OPEN, FALSE);
		bSuccess = HexDump(hwndEdit, hFile, wfad.nFileSizeLow);
	}

	OutputText(hwndEdit, g_szSepThick);
	CloseHandle(hFile);

	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL ReadJpeg(HWND hDlg, HANDLE hFile, DWORD dwFileSize)
{
	if (hDlg == NULL || hFile == NULL || dwFileSize == 0)
		return FALSE;

	HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
	if (hwndEdit == NULL)
		return FALSE;

	HWND hwndThumb = GetDlgItem(hDlg, IDC_THUMB);
	if (hwndThumb == NULL)
		return FALSE;

	// Jump to the beginning of the file
	if (!FileSeekBegin(hFile, 0))
		return FALSE;

	LPVOID lpData = MyGlobalAllocPtr(GHND, dwFileSize);
	if (lpData == NULL)
		return FALSE;

	// Read the file
	if (!MyReadFile(hFile, lpData, dwFileSize))
	{
		MyGlobalFreePtr(lpData);
		return FALSE;
	}

	OutputText(hwndEdit, g_szSepThin);

	HANDLE hDib = NULL;
	HCURSOR hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));

	// Decode the JPEG image
	__try { hDib = JpegToDib(lpData, dwFileSize, 1); }
	__except (EXCEPTION_EXECUTE_HANDLER) { hDib = NULL; }

	SetCursor(hOldCursor);

	if (hDib == NULL)
	{
		MyGlobalFreePtr(lpData);
		SetThumbnailText(hwndThumb, IDS_UNSUPPORTED);
		return FALSE;
	}

	MyGlobalFreePtr(lpData);
	ReplaceThumbnail(hwndThumb, hDib);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL HexDump(HWND hwndEdit, HANDLE hFile, SIZE_T cbLen)
{
	const int COLUMNS = 16;
	const int FORMAT_LEN = 256;

	TCHAR szFormat[FORMAT_LEN];
	TCHAR szOutput[OUTPUT_LEN];

	if (hwndEdit == NULL || hFile == NULL)
		return FALSE;

	if (cbLen > 1024)
		cbLen = 1024;

	PBYTE pData = (PBYTE)MyGlobalAllocPtr(GHND, cbLen);
	if (pData == NULL)
		return FALSE;

	if (!FileSeekBegin(hFile, 0) || !MyReadFile(hFile, pData, cbLen))
	{
		MyGlobalFreePtr((LPVOID)pData);
		return FALSE;
	}

	OutputText(hwndEdit, g_szSepThin);
	OutputTextFmtCount(hwndEdit, IDS_HEXDUMP, cbLen);

	PBYTE pByte;
	SIZE_T i, j, c;
	for (i = 0; i < cbLen; i += COLUMNS)
	{
		c = ((cbLen - i) > COLUMNS) ? COLUMNS : cbLen - i;

		// Address
		_sntprintf(szOutput, _countof(szOutput), TEXT("%04IX| "), i);
		szOutput[OUTPUT_LEN - 1] = TEXT('\0');

		// Hex dump
		for (j = c, pByte = pData + i; j--; pByte++)
		{
			_sntprintf(szFormat, _countof(szFormat), TEXT("%02X "), *pByte);
			szFormat[FORMAT_LEN - 1] = TEXT('\0');
			_tcsncat(szOutput, szFormat, _countof(szOutput) - _tcslen(szOutput) - 4);
		}

		for (j = COLUMNS - c; j--; )
			_tcsncat(szOutput, TEXT("   "), _countof(szOutput) - _tcslen(szOutput) - 4);

		_tcsncat(szOutput, TEXT("|"), _countof(szOutput) - _tcslen(szOutput) - 4);

		// ASCII dump
		for (j = c, pByte = pData + i; j--; pByte++)
		{
			_sntprintf(szFormat, _countof(szFormat), TEXT("%hc"), isprint(*pByte) ? *pByte : '.');
			szFormat[FORMAT_LEN - 1] = TEXT('\0');
			_tcsncat(szOutput, szFormat, _countof(szOutput) - _tcslen(szOutput) - 4);
		}

		for (j = COLUMNS - c; j--; )
			_tcsncat(szOutput, TEXT(" "), _countof(szOutput) - _tcslen(szOutput) - 4);

		_tcsncat(szOutput, TEXT("|\r\n"), _countof(szOutput) - _tcslen(szOutput) - 1);

		OutputText(hwndEdit, szOutput);
	}

	MyGlobalFreePtr((LPVOID)pData);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL OnDrawItem(HWND hDlg, const LPDRAWITEMSTRUCT lpDrawItem)
{
	if (lpDrawItem == NULL || lpDrawItem->CtlID != IDC_THUMB)
		return FALSE;

	HDC hdc = lpDrawItem->hDC;
	RECT rc = lpDrawItem->rcItem;

	BOOL bSuccess = TRUE;
	BOOL bDrawText = TRUE;
	HANDLE hDib = g_hDibDefault;
	if (g_hDibThumb != NULL)
	{
		bDrawText = FALSE;
		hDib = g_hDibThumb;
	}

	if (hDib == NULL)
		bSuccess = FALSE;
	else
	{
		// If necessary, create and select a color palette
		HPALETTE hpal = NULL;
		HPALETTE hpalOld = NULL;
		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			hpal = CreateDibPalette(hDib);
		if (hpal != NULL)
		{
			hpalOld = SelectPalette(hdc, hpal, FALSE);
			if (hpalOld != NULL)
				RealizePalette(hdc);
		}

		// Output the DIB
		LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDib);
		if (lpbi == NULL)
			bSuccess = FALSE;
		else
		{
			int nSrcX = 0;
			int nSrcY = 0;
			int nSrcWidth  = abs(lpbi->biWidth);
			int nSrcHeight = abs(lpbi->biHeight);

			if (IS_OS2PM_DIB(lpbi))
			{
				nSrcWidth  = ((LPBITMAPCOREHEADER)lpbi)->bcWidth;
				nSrcHeight = ((LPBITMAPCOREHEADER)lpbi)->bcHeight;
			}

			if (lpDrawItem->itemState & ODS_SELECTED)
			{
				nSrcX = nSrcWidth  / 20;
				nSrcY = nSrcHeight / 20;
				nSrcWidth  -= 2 * nSrcX;
				nSrcHeight -= 2 * nSrcY;
			}

			if (g_hBitmapThumb != NULL && g_nIcmMode != ICM_ON)
				bSuccess = AlphaBlendBitmap(hdc, g_hBitmapThumb, rc.left, rc.top,
					rc.right - rc.left, rc.bottom - rc.top, nSrcX, nSrcY, nSrcWidth, nSrcHeight);
			else
				bSuccess = DrawDib(hdc, lpbi, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
					nSrcX, nSrcY, nSrcWidth, nSrcHeight);

			GlobalUnlock(hDib);
		}

		if (hpalOld != NULL)
			SelectPalette(hdc, hpalOld, FALSE);
		if (hpal != NULL)
			DeletePalette(hpal);
	}

	if (!bSuccess)
	{ // Draw a 45-degree crosshatch
		HBRUSH hbr = CreateHatchBrush(HS_DIAGCROSS, RGB(0, 0, 0));
		FillRect(hdc, &rc, hbr);
		DeleteBrush(hbr);
	}

	if (bDrawText)
	{ // Draw the title text of the default thumbnail
		TCHAR szText[256];
		if (GetWindowText(lpDrawItem->hwndItem, szText, _countof(szText)) > 0)
			DrawThumbnailText(hdc, &rc, szText,
				bSuccess ? RGB_THUMBNAIL_TEXTCOLOR : RGB_DARKMODE_BKCOLOR,
				bSuccess ? TRANSPARENT : OPAQUE);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DrawDib(HDC hdc, LPBITMAPINFOHEADER lpbi, int xDest, int yDest, int wDest, int hDest, int xSrc, int ySrc, int wSrc, int hSrc)
{
	if (hdc == NULL || lpbi == NULL)
		return FALSE;

	SetICMMode(hdc, g_nIcmMode);

	POINT pt = { 0 };
	GetBrushOrgEx(hdc, &pt);
	int nBltModeOld = SetStretchBltMode(hdc, HALFTONE);
	SetBrushOrgEx(hdc, pt.x, pt.y, NULL);

	int nRet = StretchDIBits(hdc, xDest, yDest, wDest, hDest, xSrc, ySrc, wSrc, hSrc,
		FindDibBits((LPCSTR)lpbi), (LPBITMAPINFO)lpbi, DIB_RGB_COLORS, SRCCOPY);

	BOOL bSuccess = (nRet != 0 && nRet != GDI_ERROR);
	if (!bSuccess)
	{
		// Is it possibly a video compressed DIB?
		if (DibIsCustomFormat((LPCSTR)lpbi))
		{ // Try to output the DIB using Video for Windows DrawDib API
			if (g_hDrawDib == NULL)
				g_hDrawDib = DrawDibOpen();

			if (g_hDrawDib != NULL)
				bSuccess = DrawDibDraw(g_hDrawDib, hdc, xDest, yDest, wDest, hDest,
					lpbi, FindDibBits((LPCSTR)lpbi), xSrc, ySrc, wSrc, hSrc, 0);
		}
	}

	if (nBltModeOld != 0)
	{
		if (nBltModeOld == HALFTONE)
			GetBrushOrgEx(hdc, &pt);
		SetStretchBltMode(hdc, nBltModeOld);
		if (nBltModeOld == HALFTONE)
			SetBrushOrgEx(hdc, pt.x, pt.y, NULL);
	}

	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL AlphaBlendBitmap(HDC hdc, HBITMAP hbm, int xDest, int yDest, int wDest, int hDest, int xSrc, int ySrc, int wSrc, int hSrc)
{
	if (hdc == NULL || hbm == NULL)
		return FALSE;

	// Create an offscreen surface
	HDC hdcMem = CreateCompatibleDC(hdc);
	if (hdcMem == NULL)
		return FALSE;

	HBITMAP hbmpSurface = CreateCompatibleBitmap(hdc, wDest, hDest);
	if (hbmpSurface == NULL)
	{
		DeleteDC(hdcMem);
		return FALSE;
	}

	HBITMAP hbmpSurfaceOld = SelectBitmap(hdcMem, hbmpSurface);
	if (hbmpSurfaceOld == NULL)
	{
		DeleteBitmap(hbmpSurface);
		DeleteDC(hdcMem);
		return FALSE;
	}

	static WORD wCheckPat[8] =
	{
		0x000F, 0x000F, 0x000F, 0x000F,
		0x00F0, 0x00F0, 0x00F0, 0x00F0
	};

	// Draw a checkerboard pattern as background
	HBITMAP hbmpBrush = CreateBitmap(8, 8, 1, 1, wCheckPat);
	if (hbmpBrush != NULL)
	{
		HBRUSH hbr = CreatePatternBrush(hbmpBrush);
		if (hbr != NULL)
		{
			HBRUSH hbrOld = SelectBrush(hdcMem, hbr);
			COLORREF rgbTextOld = SetTextColor(hdcMem, RGB(204, 204, 204));
			COLORREF rgbBkOld = SetBkColor(hdcMem, RGB(255, 255, 255));

			PatBlt(hdcMem, xDest, yDest, wDest, hDest, PATCOPY);

			if (rgbBkOld != CLR_INVALID)
				SetBkColor(hdcMem, rgbBkOld);
			if (rgbTextOld != CLR_INVALID)
				SetTextColor(hdcMem, rgbTextOld);
			if (hbrOld != NULL)
				SelectBrush(hdcMem, hbrOld);

			DeleteBrush(hbr);
		}

		DeleteBitmap(hbmpBrush);
	}

	HDC hdcAlpha = CreateCompatibleDC(hdc);
	if (hdcAlpha != NULL)
	{
		int nBltModeOld = SetStretchBltMode(hdc, COLORONCOLOR);
		HBITMAP hbmpThumbOld = SelectBitmap(hdcAlpha, hbm);

		// Blend the thumbnail with the checkerboard pattern
		BLENDFUNCTION pixelblend = { AC_SRC_OVER, 0, 0xFF, AC_SRC_ALPHA };
		if (!AlphaBlend(hdcMem, xDest, yDest, wDest, hDest, hdcAlpha, xSrc, ySrc, wSrc, hSrc, pixelblend))
			StretchBlt(hdcMem, xDest, yDest, wDest, hDest, hdcAlpha, xSrc, ySrc, wSrc, hSrc, SRCCOPY);

		if (hbmpThumbOld != NULL)
			SelectBitmap(hdcAlpha, hbmpThumbOld);
		if (nBltModeOld != 0)
		{
			POINT pt = { 0 };
			if (nBltModeOld == HALFTONE)
				GetBrushOrgEx(hdc, &pt);
			SetStretchBltMode(hdc, nBltModeOld);
			if (nBltModeOld == HALFTONE)
				SetBrushOrgEx(hdc, pt.x, pt.y, NULL);
		}

		DeleteDC(hdcAlpha);
	}

	// Transfer the offscreen surface to the screen
	BOOL bSuccess = BitBlt(hdc, xDest, yDest, wDest, hDest, hdcMem, xDest, yDest, SRCCOPY);

	SelectBitmap(hdcMem, hbmpSurfaceOld);
	DeleteBitmap(hbmpSurface);
	DeleteDC(hdcMem);

	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DrawThumbnailText(HDC hdc, LPRECT lpRect, LPCTSTR lpszText, COLORREF color, int mode)
{
	if (hdc == NULL || lpRect == NULL || lpszText == NULL)
		return FALSE;

	COLORREF rgbTextOld = SetTextColor(hdc, color);
	int nBkModeOld = SetBkMode(hdc, mode);

	HFONT hfontOld = NULL;
	HFONT hfont = CreateScaledFont(hdc, lpRect, lpszText);
	if (hfont != NULL)
		hfontOld = SelectFont(hdc, hfont);

	BOOL bSuccess = DrawText(hdc, lpszText, -1, lpRect,
		DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX) != 0;

	if (hfontOld != NULL)
		SelectFont(hdc, hfontOld);
	if (hfont != NULL)
		DeleteFont(hfont);

	if (nBkModeOld != 0)
		SetBkMode(hdc, nBkModeOld);
	if (rgbTextOld != CLR_INVALID)
		SetTextColor(hdc, rgbTextOld);

	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HFONT CreateScaledFont(HDC hdc, LPCRECT lpRect, LPCTSTR lpszText)
{
	if (hdc == NULL || lpRect == NULL || lpszText == NULL)
		return NULL;

	SIZE_T cchTextLen = _tcslen(lpszText);
	if (cchTextLen == 0)
		return NULL;

	// Define the maximum size of the rectangle in the center
	// of the thumbnail in which the text must be positioned
	SIZE sizeTextMax = { 0 };
	sizeTextMax.cx = ((lpRect->right - lpRect->left)) * 3 / 4;
	sizeTextMax.cy = ((lpRect->bottom - lpRect->top)) / 6;

	LOGFONT lf = { 0 };
	lf.lfHeight = -sizeTextMax.cy; // Set the max practicable font size
	lf.lfWeight = FW_BOLD;
	lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
	MyStrNCpy(lf.lfFaceName, g_szFontThumb, _countof(lf.lfFaceName));

	HFONT hFont = CreateFontIndirect(&lf);
	if (hFont == NULL)
		return NULL;

	HFONT hFontOld = SelectFont(hdc, hFont);

	SIZE sizeTextRatio = { 0 };
	GetTextExtentPoint32(hdc, lpszText, (int)cchTextLen, &sizeTextRatio);

	if (hFontOld != NULL)
		SelectFont(hdc, hFontOld);

	__try
	{
		// If the text has become too wide due to the maximum
		// font height, reduce the font size until it fits in
		lf.lfHeight = 0;
		if (((sizeTextMax.cy * sizeTextRatio.cx) / sizeTextRatio.cy) > sizeTextMax.cx)
			lf.lfHeight = -((sizeTextMax.cx * sizeTextRatio.cy) / sizeTextRatio.cx);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { ; }

	if (lf.lfHeight != 0)
	{
		DeleteFont(hFont);
		hFont = CreateFontIndirect(&lf);
	}

	return hFont;
}

/////////////////////////////////////////////////////////////////////////////

int SelectText(HWND hwndEdit)
{
	if (hwndEdit == NULL || Edit_GetTextLength(hwndEdit) == 0)
		return 0;

	INT_PTR iStartChar = 0;
	INT_PTR iEndChar = 0;
	SendMessage(hwndEdit, EM_GETSEL, (WPARAM)&iStartChar, (LPARAM)&iEndChar);

	int nSelLen = (int)(iEndChar - iStartChar);

	if (nSelLen != 0)
		return nSelLen;

	int iLine = Edit_LineFromChar(hwndEdit, -1);
	int nLineLen = Edit_LineLength(hwndEdit, -1);

	if (nLineLen > 1024)
		nLineLen = 1024;

	TCHAR szLine[1024] = { 0 };
	nLineLen = Edit_GetLine(hwndEdit, iLine, szLine, nLineLen);
	szLine[nLineLen] = TEXT('\0');

	if (nLineLen == 0)
		return 0;

	int iChar = Edit_LineIndex(hwndEdit, iLine);
	int iCharInLine = (int)(iStartChar - iChar);

	// Spaces and parentheses are valid characters for URLs,
	// but are required for the selection of regular words
	const TCHAR* pszCutOffChars = TEXT(" \t\"<>()[]{}|#");

	TCHAR* pszLine = szLine;
	pszLine += iCharInLine;

	TCHAR* pszChar = _tcspbrk(pszLine, pszCutOffChars);
	if (pszChar == NULL)
		iEndChar = (INT_PTR)iChar + nLineLen;
	else
		iEndChar = (INT_PTR)iChar + iCharInLine + (pszChar - pszLine);

	szLine[iCharInLine] = TEXT('\0');
	pszLine = _tcsrev(szLine);

	pszChar = _tcspbrk(pszLine, pszCutOffChars);
	if (pszChar == NULL)
		iStartChar = iChar;
	else
		iStartChar = (INT_PTR)iChar + iCharInLine - (pszChar - pszLine);

	nSelLen = (int)(iEndChar - iStartChar);

	if (nSelLen != 0)
		Edit_SetSel(hwndEdit, iStartChar, iEndChar);

	return nSelLen;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SetWordWrap(HWND hDlg, BOOL bWordWrap)
{
	if (hDlg == NULL)
		return FALSE;

	HWND hwndEditOld = GetDlgItem(hDlg, IDC_OUTPUT);
	if (hwndEditOld == NULL)
		return FALSE;

	// Save text and status of the edit control
	int nLen = Edit_GetTextLength(hwndEditOld);
	LPTSTR pszSaveText = (LPTSTR)MyGlobalAllocPtr(GHND, ((SIZE_T)nLen + 1) * sizeof(TCHAR));
	if (pszSaveText == NULL)
		return FALSE;

	Edit_GetText(hwndEditOld, pszSaveText, nLen + 1);

	RECT rcProp;
	RetrieveWindowRect(hwndEditOld, &rcProp);
	HFONT hFont = GetWindowFont(hwndEditOld);
	BOOL bHasFocus = (GetFocus() == hwndEditOld);
	DWORD dwSelStart = (DWORD)nLen;
	DWORD dwSelEnd = (DWORD)nLen;
	SendMessage(hwndEditOld, EM_GETSEL, (WPARAM)&dwSelStart, (LPARAM)&dwSelEnd);

	// Remove subclassing
	if (g_lpPrevOutputWndProc != NULL)
		SubclassWindow(hwndEditOld, g_lpPrevOutputWndProc);

	// Create a new edit control in the desired style
	DWORD dwExStyle = GetWindowExStyle(hwndEditOld);
	DWORD dwStyle = GetWindowStyle(hwndEditOld);
	if (dwExStyle == 0)
		dwExStyle = WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE;
	if (dwStyle == 0)
		dwStyle = WS_CHILD | WS_TABSTOP | WS_GROUP | WS_VSCROLL |
		ES_MULTILINE | ES_NOHIDESEL | ES_READONLY;
	if (bWordWrap)
		dwStyle = dwStyle & ~WS_HSCROLL;
	else
		dwStyle |= WS_HSCROLL;

	RECT rcSize;
	GetWindowRect(hwndEditOld, &rcSize);
	ScreenToClient(hDlg, (LPPOINT)&rcSize);
	ScreenToClient(hDlg, ((LPPOINT)&rcSize) + 1);
	UINT nID = GetWindowID(hwndEditOld);

	HWND hwndEditNew = CreateWindowEx(dwExStyle, TEXT("Edit"), NULL, dwStyle,
		rcSize.left, rcSize.top, rcSize.right - rcSize.left, rcSize.bottom - rcSize.top,
		hDlg, (HMENU)(UINT_PTR)nID, g_hInstance, NULL);
	if (hwndEditNew == NULL)
	{
		MyGlobalFreePtr((LPVOID)pszSaveText);
		return FALSE;
	}

	// To be safe, clear the content of the old edit control
	Edit_SetText(hwndEditOld, NULL);

	StoreWindowRect(hwndEditNew, &rcProp);

	// Restore all visible styles
	Edit_SetText(hwndEditNew, pszSaveText);
	MyGlobalFreePtr((LPVOID)pszSaveText);

	if (hFont != NULL)
		SetWindowFont(hwndEditNew, hFont, FALSE);

	if (g_bUseDarkMode)
		SetWindowTheme(hwndEditNew, L"DarkMode_Explorer", NULL);

	SetWindowLongPtr(hwndEditOld, GWLP_ID, (LONG_PTR)nID + 2000);
	SetWindowPos(hwndEditNew, HWND_TOP, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
	SetWindowPos(hwndEditNew, HWND_TOP, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_DRAWFRAME);

	// Destroy the old edit control
	SetWindowPos(hwndEditOld, HWND_TOP, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW | SWP_HIDEWINDOW);
	DeleteWindowRect(hwndEditOld);
	DestroyWindow(hwndEditOld);

	UpdateWindow(hwndEditNew);

	// Restore the remaining states
	Edit_LimitText(hwndEditNew, 0);
	if (bHasFocus)
		SetFocus(hwndEditNew);
	Edit_SetSel(hwndEditNew, dwSelStart, dwSelEnd);

	// Restore subclassing
	g_lpPrevOutputWndProc = SubclassWindow(hwndEditNew, OutputWndProc);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL LoadSettings(LPWINDOWPLACEMENT lpWindowPlacement, LPLONG lpFontHeight)
{
	if (lpWindowPlacement == NULL || lpFontHeight == NULL)
		return FALSE;

	HKEY hKey = NULL;
	LONG lStatus = RegOpenKeyEx(HKEY_CURRENT_USER, g_szRegPath, 0, KEY_READ, &hKey);
	if (lStatus != ERROR_SUCCESS || hKey == NULL)
		return FALSE;

	BOOL bSuccess = TRUE;
	DWORD dwValue = (DWORD)*lpFontHeight;
	DWORD dwSize = sizeof(dwValue);
	DWORD dwType = REG_DWORD;
	lStatus = RegQueryValueEx(hKey, g_szFntHeight, NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
	if (lStatus != ERROR_SUCCESS || ((LONG)dwValue < -127 || (LONG)dwValue > 127))
		bSuccess = bSuccess && FALSE;
	else
		*lpFontHeight = (LONG)dwValue;

	WINDOWPLACEMENT wpl = { 0 };
	dwSize = sizeof(wpl);
	dwType = REG_BINARY;
	lStatus = RegQueryValueEx(hKey, g_szPlacement, NULL, &dwType, (LPBYTE)&wpl, &dwSize);
	if (lStatus != ERROR_SUCCESS || dwSize != sizeof(wpl) || wpl.length != sizeof(wpl))
		bSuccess = bSuccess && FALSE;
	else
		CopyMemory(lpWindowPlacement, &wpl, sizeof(WINDOWPLACEMENT));

	RegCloseKey(hKey);
	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void SaveSettings(HWND hDlg, HFONT hFont)
{
	WINDOWPLACEMENT wpl = { 0 };
	wpl.length = sizeof(wpl);
	if (hDlg != NULL)
		GetWindowPlacement(hDlg, &wpl);

	LOGFONT lf = { 0 };
	if (hFont != NULL)
		GetObject(hFont, sizeof(LOGFONT), (LPVOID)&lf);

	HKEY hKey = NULL;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, g_szRegPath, 0, NULL, 0,
		KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS || hKey == NULL)
		return;

	RegSetValueEx(hKey, g_szFntHeight, 0, REG_DWORD, (LPBYTE)&lf.lfHeight, sizeof(DWORD));
	RegSetValueEx(hKey, g_szPlacement, 0, REG_BINARY, (LPBYTE)&wpl, sizeof(wpl));

	RegCloseKey(hKey);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL MySetWindowPlacement(HWND hWnd, const LPWINDOWPLACEMENT lpwndpl, BOOL bUseShowWindow)
{
	if (hWnd == NULL || lpwndpl == NULL || lpwndpl->length != sizeof(WINDOWPLACEMENT))
		return FALSE;

	RECT rc;
	CopyRect(&rc, &lpwndpl->rcNormalPosition);

	HMONITOR hMonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONULL);
	if (hMonitor == NULL)
		return FALSE;

	MONITORINFO mi = { 0 };
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfo(hMonitor, &mi))
		return FALSE;

	OffsetRect(&rc, mi.rcWork.left - mi.rcMonitor.left, mi.rcWork.top - mi.rcMonitor.top);
	SetWindowPos(hWnd, HWND_TOP, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
		SWP_NOZORDER | SWP_NOACTIVATE);

	if (bUseShowWindow)
		ShowWindow(hWnd, lpwndpl->showCmd);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void StoreWindowRect(HWND hwnd, LPRECT lprc)
{
	if (hwnd != NULL && lprc != NULL)
	{
		SetProp(hwnd, g_szWndTop,    (HANDLE)(LONG_PTR)lprc->top);
		SetProp(hwnd, g_szWndBottom, (HANDLE)(LONG_PTR)lprc->bottom);
		SetProp(hwnd, g_szWndLeft,   (HANDLE)(LONG_PTR)lprc->left);
		SetProp(hwnd, g_szWndRight,  (HANDLE)(LONG_PTR)lprc->right);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void RetrieveWindowRect(HWND hwnd, LPRECT lprc)
{
	if (lprc != NULL)
	{
		if (hwnd == NULL)
			SetRectEmpty(lprc);
		else
		{
			lprc->top    = (LONG)(LONG_PTR)GetProp(hwnd, g_szWndTop);
			lprc->bottom = (LONG)(LONG_PTR)GetProp(hwnd, g_szWndBottom);
			lprc->left   = (LONG)(LONG_PTR)GetProp(hwnd, g_szWndLeft);
			lprc->right  = (LONG)(LONG_PTR)GetProp(hwnd, g_szWndRight);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void DeleteWindowRect(HWND hwnd)
{
	if (hwnd != NULL)
	{
		RemoveProp(hwnd, g_szWndTop);
		RemoveProp(hwnd, g_szWndBottom);
		RemoveProp(hwnd, g_szWndLeft);
		RemoveProp(hwnd, g_szWndRight);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL IsHighContrast()
{
	HIGHCONTRAST hc = { sizeof(hc) };
	if (!SystemParametersInfo(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, 0))
		return FALSE;

	return (hc.dwFlags & HCF_HIGHCONTRASTON);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL ShouldAppsUseDarkMode()
{
	HKEY hKey = NULL;
	LONG lStatus = RegOpenKeyEx(HKEY_CURRENT_USER,
		TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"), 0, KEY_READ, &hKey);
	if (lStatus != ERROR_SUCCESS || hKey == NULL)
		return FALSE;

	DWORD dwValue = 1;
	DWORD dwSize = sizeof(dwValue);
	DWORD dwType = REG_DWORD;
	lStatus = RegQueryValueEx(hKey, TEXT("AppsUseLightTheme"), NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
	if (lStatus != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		return FALSE;
	}

	RegCloseKey(hKey);
	return (dwValue == 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT UseImmersiveDarkMode(HWND hwndMain, BOOL bUse)
{
	HRESULT hr = S_FALSE;

	if (hwndMain == NULL)
		return E_INVALIDARG;

	BOOL bUseDarkMode = !!bUse;
	if (FAILED(hr = DwmSetWindowAttribute(hwndMain,
		DWMWA_USE_IMMERSIVE_DARK_MODE, &bUseDarkMode, sizeof(bUseDarkMode))))
		hr = DwmSetWindowAttribute(hwndMain, 19, &bUseDarkMode, sizeof(bUseDarkMode));

	return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Use a dark mode theme for controls that support it
//
// "DarkMode_CFD": Edit, ComboBox
// "DarkMode_Explorer": Button, Edit (multiline), ScrollBar, TreeView
// "DarkMode_ItemsView": ListView, Header
//
BOOL AllowDarkModeForWindow(HWND hwndParent, BOOL bAllow)
{
	if (hwndParent == NULL)
		return FALSE;

	HWND hwndChild = GetWindow(hwndParent, GW_CHILD);
	while (hwndChild)
	{
		// We use only buttons, a multiline edit control and the size box (scroll bar)
		SetWindowTheme(hwndChild, bAllow ? L"DarkMode_Explorer" : NULL, NULL);
		hwndChild = GetNextSibling(hwndChild);
	}

	RedrawWindow(hwndParent, NULL, NULL,
		RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
