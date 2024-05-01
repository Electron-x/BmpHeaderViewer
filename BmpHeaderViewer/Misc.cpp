////////////////////////////////////////////////////////////////////////////////////////////////
// Misc.cpp - Copyright (c) 2024 by W. Rolke.
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

BOOL MyReadFile(HANDLE hFile, LPVOID lpBuffer, SIZE_T cbSize)
{
	if (hFile == NULL || lpBuffer == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	DWORD dwRead;
	if (!ReadFile(hFile, lpBuffer, (DWORD)cbSize, &dwRead, NULL) || dwRead != cbSize)
		return FALSE;

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

LPVOID MyGlobalAllocPtr(UINT uFlags, SIZE_T dwBytes)
{
	HGLOBAL handle = GlobalAlloc(uFlags, dwBytes);
	return ((handle == NULL) ? NULL : GlobalLock(handle));
}

////////////////////////////////////////////////////////////////////////////////////////////////

void MyGlobalFreePtr(LPCVOID pMem)
{
	if (pMem != NULL)
	{
		HGLOBAL handle = GlobalHandle(pMem);
		if (handle != NULL) { GlobalUnlock(handle); GlobalFree(handle); }
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

LPSTR MyStrNCpyA(LPSTR lpString1, LPCSTR lpString2, int iMaxLength)
{
	return lstrcpynA(lpString1, lpString2, iMaxLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////

LPWSTR MyStrNCpyW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength)
{
	return lstrcpynW(lpString1, lpString2, iMaxLength);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL IsKeyDown(int nVirtKey)
{
	return (GetKeyState(nVirtKey) < 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////

void SetThumbnailText(HWND hwndThumb, UINT uID)
{
	if (hwndThumb == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return;
	}

	// Load the desired text or the default string "Unsupported format"
	TCHAR szText[OUTPUT_LEN];
	if (!LoadString(g_hInstance, uID ? uID : IDS_UNSUPPORTED, szText, _countof(szText)))
		return;

	// Save the text as title of the owner-drawn bitmap button
	SetWindowText(hwndThumb, szText);

	// Update the control immediately
	if (InvalidateRect(hwndThumb, NULL, FALSE))
		UpdateWindow(hwndThumb);
}

////////////////////////////////////////////////////////////////////////////////////////////////

void ResetThumbnail(HWND hwndThumb, LPCTSTR lpszTitle)
{
	// Free the memory of the current thumbnail image
	g_hBitmapThumb = FreeBitmap(g_hBitmapThumb);
	g_hDibThumb = FreeDib(g_hDibThumb);

	// Deactivate ICM by default
	g_nIcmMode = ICM_OFF;

	if (hwndThumb != NULL)
	{ // Set the title of the default thumbnail
		SetWindowText(hwndThumb, lpszTitle != NULL ? lpszTitle : TEXT(""));
		InvalidateRect(hwndThumb, NULL, FALSE);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void ReplaceThumbnail(HWND hwndThumb, HANDLE hDib)
{
	// Free the memory of the current thumbnail image
	g_hBitmapThumb = FreeBitmap(g_hBitmapThumb);
	g_hDibThumb = FreeDib(g_hDibThumb);

	// Save the DIB for display using StretchDIBits or DrawDibDraw.
	// If hDib is NULL, a default thumbnail is displayed.
	g_hDibThumb = hDib;
	// Check the DIB for transparent pixels and create an additional
	// pre-multiplied bitmap for the AlphaBlend function if needed
	g_hBitmapThumb = CreatePremultipliedBitmap(hDib);

	if (hDib != NULL)
	{
		LPCSTR lpbi = (LPCSTR)GlobalLock(hDib);
		if (lpbi != NULL)
		{
			// For performance reasons, enable ICM inside DC only if the
			// DIB contains color space data and no transparent pixels
			g_nIcmMode = ICM_OFF;
			if (g_hBitmapThumb == NULL && DibHasColorSpaceData(lpbi))
				g_nIcmMode = ICM_ON;

			GlobalUnlock(hDib);
		}
	}

	if (hwndThumb != NULL)
	{ // Display the thumbnail immediately
		if (InvalidateRect(hwndThumb, NULL, FALSE))
			UpdateWindow(hwndThumb);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void ClearOutputWindow(HWND hwndEdit)
{
	if (hwndEdit != NULL)
	{
		Edit_SetText(hwndEdit, TEXT(""));
		Edit_EmptyUndoBuffer(hwndEdit);
		Edit_SetModify(hwndEdit, FALSE);

		UpdateWindow(hwndEdit);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void OutputText(HWND hwndEdit, LPCTSTR lpszOutput)
{
	if (hwndEdit != NULL && lpszOutput != NULL)
		Edit_ReplaceSel(hwndEdit, lpszOutput);
}

////////////////////////////////////////////////////////////////////////////////////////////////

void OutputTextFmt(HWND hwndEdit, LPCTSTR lpszFormat, ...)
{
	if (hwndEdit != NULL && lpszFormat != NULL)
	{
		TCHAR szOutput[OUTPUT_LEN];

		va_list arglist;
		va_start(arglist, lpszFormat);
		_vsntprintf(szOutput, OUTPUT_LEN - 1, lpszFormat, arglist);
		szOutput[OUTPUT_LEN - 1] = TEXT('\0');
		va_end(arglist);

		Edit_ReplaceSel(hwndEdit, szOutput);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL OutputTextFmtCount(HWND hwndEdit, UINT uID, SIZE_T uCount)
{
	const int FORMAT_LEN = 256;

	if (hwndEdit == NULL)
		return FALSE;

	TCHAR szOutput[OUTPUT_LEN];
	if (LoadString(g_hInstance, uID, szOutput, _countof(szOutput)) == 0)
		return FALSE;

	TCHAR szFormat[FORMAT_LEN];
	_sntprintf(szFormat, _countof(szFormat), TEXT("%Iu"), uCount);
	szFormat[FORMAT_LEN - 1] = TEXT('\0');

	LPCTSTR lpszText = AllocReplaceString(szOutput, TEXT("{COUNT}"), szFormat);
	if (lpszText == NULL)
		return FALSE;

	OutputText(hwndEdit, lpszText);
	OutputText(hwndEdit, g_szSepThin);

	MyGlobalFreePtr((LPVOID)lpszText);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL OutputTextFromID(HWND hwndEdit, UINT uID)
{
	if (hwndEdit == NULL)
		return FALSE;

	TCHAR szOutput[OUTPUT_LEN];
	if (LoadString(g_hInstance, uID, szOutput, _countof(szOutput)) == 0)
		return FALSE;

	Edit_ReplaceSel(hwndEdit, g_szSepThin);
	Edit_ReplaceSel(hwndEdit, szOutput);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL OutputErrorMessage(HWND hwndEdit, DWORD dwError)
{
	if (hwndEdit == NULL)
		return FALSE;

	LPVOID lpMsgBuf = NULL;
	TCHAR szOutput[OUTPUT_LEN];

	DWORD dwLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dwError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);

	if (dwLen > 0 && lpMsgBuf != NULL)
	{
		MyStrNCpy(szOutput, (LPTSTR)lpMsgBuf, _countof(szOutput));
		OutputText(hwndEdit, g_szSepThin);
		OutputText(hwndEdit, szOutput);
		LocalFree(lpMsgBuf);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

PTCHAR AppendFlagName(LPTSTR lpszOutput, SIZE_T cchLenOutput, LPCTSTR lpszFlagName)
{
	if (lpszOutput == NULL || lpszFlagName == NULL || cchLenOutput == 0)
		return NULL;

	if (lpszOutput[0] != TEXT('\0'))
		_tcsncat(lpszOutput, TEXT("|"), cchLenOutput - _tcslen(lpszOutput) - 1);

	return _tcsncat(lpszOutput, lpszFlagName, cchLenOutput - _tcslen(lpszOutput) - 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL FormatByteSize(DWORD dwSize, LPTSTR lpszString, SIZE_T cchStringLen)
{
	if (lpszString == NULL)
		return FALSE;

	if (dwSize < 1024)
		_sntprintf(lpszString, cchStringLen, TEXT("%u bytes"), dwSize);
	else if (dwSize < 10240)
		_sntprintf(lpszString, cchStringLen, TEXT("%.2f KB"), (double)(dwSize / 1024.0));
	else if (dwSize < 102400)
		_sntprintf(lpszString, cchStringLen, TEXT("%.1f KB"), (double)(dwSize / 1024.0));
	else if (dwSize < 1048576)
		_sntprintf(lpszString, cchStringLen, TEXT("%.0f KB"), (double)(dwSize / 1024.0));
	else if (dwSize < 10485760)
		_sntprintf(lpszString, cchStringLen, TEXT("%.2f MB"), (double)(dwSize / 1048576.0));
	else if (dwSize < 104857600)
		_sntprintf(lpszString, cchStringLen, TEXT("%.1f MB"), (double)(dwSize / 1048576.0));
	else if (dwSize < 1073741824)
		_sntprintf(lpszString, cchStringLen, TEXT("%.0f MB"), (double)(dwSize / 1048576.0));
	else
		_sntprintf(lpszString, cchStringLen, TEXT("%.2f GB"), (double)(dwSize / 1073741824.0));

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

LPTSTR AllocReplaceString(LPCTSTR lpszOriginal, LPCTSTR lpszPattern, LPCTSTR lpszReplacement)
{
	if (lpszOriginal == NULL || lpszPattern == NULL || lpszReplacement == NULL)
		return NULL;

	SIZE_T CONST cchRepLen = _tcslen(lpszReplacement);
	SIZE_T CONST cchPatLen = _tcslen(lpszPattern);
	SIZE_T CONST cchOriLen = _tcslen(lpszOriginal);

	UINT uPatCount = 0;
	LPCTSTR lpszOrigPtr;
	LPCTSTR lpszPatLoc;

	// Determine how often the pattern occurs in the original string
	for (lpszOrigPtr = lpszOriginal; (lpszPatLoc = _tcsstr(lpszOrigPtr, lpszPattern));
		lpszOrigPtr = lpszPatLoc + cchPatLen)
		uPatCount++;

	// Allocate memory for the new string
	SIZE_T CONST cchRetLen = cchOriLen + uPatCount * (cchRepLen - cchPatLen);
	LPTSTR lpszReturn = (LPTSTR)MyGlobalAllocPtr(GHND, (cchRetLen + 1) * sizeof(TCHAR));
	if (lpszReturn != NULL)
	{
		// Copy the original string and replace each occurrence of the pattern
		LPTSTR lpszRetPtr = lpszReturn;
		for (lpszOrigPtr = lpszOriginal; (lpszPatLoc = _tcsstr(lpszOrigPtr, lpszPattern));
			lpszOrigPtr = lpszPatLoc + cchPatLen)
		{
			SIZE_T CONST cchSkpLen = lpszPatLoc - lpszOrigPtr;
			// Copy the substring up to the beginning of the pattern
			_tcsncpy(lpszRetPtr, lpszOrigPtr, cchSkpLen);
			lpszRetPtr += cchSkpLen;
			// Copy the replacement string
			_tcsncpy(lpszRetPtr, lpszReplacement, cchRepLen);
			lpszRetPtr += cchRepLen;
		}
		// Copy the rest of the string
		_tcscpy(lpszRetPtr, lpszOrigPtr);
	}

	return lpszReturn;
}

////////////////////////////////////////////////////////////////////////////////////////////////

SIZE_T ShortenPath(LPCTSTR lpszLongPath, LPTSTR lpszShortPath, SIZE_T cchBuffer)
{
	if (lpszShortPath == NULL || lpszLongPath == NULL || cchBuffer == 0)
		return 0;

	SIZE_T cchPath = _tcslen(lpszLongPath);
	if (cchPath < cchBuffer)
	{
		MyStrNCpy(lpszShortPath, lpszLongPath, (int)cchBuffer);
		return cchPath;
	}

	cchPath = GetShortPathName(lpszLongPath, lpszShortPath, (DWORD)cchBuffer);
	if (cchPath > 0 && cchPath < cchBuffer)
		return cchPath;

	LPCTSTR lpszFile = _tcsrchr(lpszLongPath, TEXT('\\'));
	if (lpszFile == NULL)
		lpszFile = _tcsrchr(lpszLongPath, TEXT('/'));
	if (lpszFile != NULL)
	{
		lpszFile++;
		cchPath = _tcslen(lpszFile);
		if (cchPath > 0 && cchPath < cchBuffer)
		{
			MyStrNCpy(lpszShortPath, lpszFile, (int)cchBuffer);
			return cchPath;
		}
	}

	lpszShortPath[0] = TEXT('\0');
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOL GetFileName(HWND hDlg, LPTSTR lpszFileName, SIZE_T cchStringLen, LPDWORD lpdwFilterIndex, BOOL bSave)
{
	if (lpszFileName == NULL || lpdwFilterIndex == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	// Filter string
	TCHAR szFilter[1024] = { 0 };
	UINT uID = IDS_FILTER_BMP_OPEN;
	if (bSave)
	{
		uID = IDS_FILTER_BMP_SAVE;
		if (*lpdwFilterIndex == 2)
		{
			uID = IDS_FILTER_ICC_SAVE;
			*lpdwFilterIndex = 1;
		}
	}
	LoadString(g_hInstance, uID, szFilter, _countof(szFilter));
	TCHAR* psz = szFilter;
	while ((psz = _tcschr(psz, TEXT('|'))))
		*psz++ = TEXT('\0');

	// Initial directory
	TCHAR* pszInitialDir = NULL;
	TCHAR szInitialDir[MY_OFN_MAX_PATH] = { 0 };
	if (lpszFileName[0] != TEXT('\0'))
	{
		MyStrNCpy(szInitialDir, lpszFileName, _countof(szInitialDir));
		if ((psz = _tcsrchr(szInitialDir, TEXT('\\'))) == NULL)
			psz = _tcsrchr(szInitialDir, TEXT('/'));
		if (psz != NULL)
		{
			pszInitialDir = szInitialDir;
			szInitialDir[psz - pszInitialDir] = TEXT('\0');
		}
	}

	// File name
	TCHAR szFile[MY_OFN_MAX_PATH] = { 0 };
	if (bSave)
	{
		if (lpszFileName[0] != TEXT('\0'))
		{
			if ((psz = _tcsrchr(lpszFileName, TEXT('\\'))) == NULL)
				psz = _tcsrchr(lpszFileName, TEXT('/'));
			psz = (psz != NULL ? (*(psz + 1) != TEXT('\0') ? psz + 1 : (TCHAR*)TEXT("*")) : lpszFileName);
			MyStrNCpy(szFile, psz, _countof(szFile));
		}
		else
			_tcscpy(szFile, TEXT("*"));
		if ((psz = _tcsrchr(szFile, TEXT('.'))) != NULL)
			szFile[psz - szFile] = TEXT('\0');
		_tcsncat(szFile, *lpdwFilterIndex == 2 ? TEXT(".icc") : TEXT(".bmp"),
			_countof(szFile) - _tcslen(szFile) - 1);
	}

	OPENFILENAME of    = { 0 };
	of.lStructSize     = sizeof(OPENFILENAME);
	of.hwndOwner       = hDlg;
	of.Flags           = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
	of.lpstrFile       = szFile;
	of.nMaxFile        = _countof(szFile);
	of.lpstrFilter     = szFilter;
	of.nFilterIndex    = *lpdwFilterIndex;
	of.lpstrDefExt     = *lpdwFilterIndex == 2 ? TEXT("icc") : TEXT("bmp");
	of.lpstrInitialDir = pszInitialDir;

	BOOL bRet = FALSE;

	__try
	{
		if (bSave)
		{
			of.Flags |= OFN_OVERWRITEPROMPT;
			bRet = GetSaveFileName(&of);
		}
		else
		{
			of.Flags |= OFN_FILEMUSTEXIST;
			bRet = GetOpenFileName(&of);
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { ; }

	if (bRet)
	{
		MyStrNCpy(lpszFileName, szFile, (int)cchStringLen);
		if (lpdwFilterIndex != NULL)
			*lpdwFilterIndex = of.nFilterIndex;
	}
	else
	{
		DWORD dwErr = CommDlgExtendedError();
		if (dwErr == FNERR_INVALIDFILENAME || dwErr == FNERR_BUFFERTOOSMALL)
			MessageBeep(MB_ICONEXCLAMATION);
	}

	return bRet;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Applies the color settings made in the Color Management dialog box
//
static BOOL ApplyColorSettings(PCOLORMATCHSETUP pcms)
{
	if (pcms == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	// Activate or deactivate color management
	g_nIcmMode = (pcms->dwFlags & CMS_DISABLEICM) ? ICM_OFF : ICM_ON;

	HWND hwndThumb = GetDlgItem(pcms->hwndOwner, IDC_THUMB);
	if (hwndThumb != NULL)
	{ // Update the thumbnail immediately
		if (InvalidateRect(hwndThumb, NULL, FALSE))
			UpdateWindow(hwndThumb);
	}

	if (pcms->dwFlags & CMS_ENABLEPROOFING)
	{ // Inform the user that proofing is not supported
		TCHAR szText[512];
		if (LoadString(g_hInstance, IDP_PROOFING, szText, _countof(szText)) > 0)
			MessageBox(GetActiveWindow(), szText, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Callback function that is invoked when the Apply button of the Color Management dialog box is pressed
//
static BOOL WINAPI ColorSetupApply(PCOLORMATCHSETUP pcms, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);

	return ApplyColorSettings(pcms);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL ColorMatchUI(HWND hDlg)
{
	TCHAR szMonitorProfile[MAX_PATH + 1] = { 0 };
	TCHAR szPrinterProfile[MAX_PATH + 1] = { 0 };
	TCHAR szTargetProfile[MAX_PATH + 1] = { 0 };

	COLORMATCHSETUP cms = { 0 };
	cms.dwSize = sizeof(COLORMATCHSETUP);
	cms.dwVersion = COLOR_MATCH_VERSION;
	cms.hwndOwner = hDlg;
	cms.lpfnApplyCallback = ColorSetupApply;

	cms.pMonitorProfile  = szMonitorProfile;
	cms.ccMonitorProfile = MAX_PATH;
	cms.pPrinterProfile  = szPrinterProfile;
	cms.ccPrinterProfile = MAX_PATH;
	cms.pTargetProfile   = szTargetProfile;
	cms.ccTargetProfile  = MAX_PATH;

	cms.dwFlags = CMS_USEAPPLYCALLBACK | CMS_DISABLEINTENT;
	if (g_nIcmMode != ICM_ON)
		cms.dwFlags |= CMS_DISABLEICM;

	if (g_hDibThumb != NULL)
	{
		LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)GlobalLock(g_hDibThumb);
		if (lpbiv5 != NULL)
		{
			if (lpbiv5->bV5Size >= sizeof(BITMAPV5HEADER))
			{
				if (lpbiv5->bV5CSType == PROFILE_EMBEDDED)
					cms.pSourceName = TEXT("Embedded Profile");
				else if (lpbiv5->bV5CSType == PROFILE_LINKED)
					cms.pSourceName = TEXT("Linked Profile");
			}
			else if (lpbiv5->bV5Size >= sizeof(BITMAPV4HEADER))
			{
				if (lpbiv5->bV5CSType == LCS_CALIBRATED_RGB)
					cms.pSourceName = TEXT("Calibrated RGB");
				else if (lpbiv5->bV5CSType == LCS_DEVICE_RGB)
					cms.pSourceName = TEXT("Device RGB");
				else if (lpbiv5->bV5CSType == LCS_DEVICE_CMYK)
					cms.pSourceName = TEXT("Device CMYK");
			}

			GlobalUnlock(g_hDibThumb);
		}
	}

	SetLastError(ERROR_SUCCESS);
	// Create the Color Management dialog box
	BOOL bSuccess = SetupColorMatching(&cms);
	if (!bSuccess)
		return (GetLastError() == ERROR_SUCCESS) ? TRUE : FALSE;

	ApplyColorSettings(&cms);

	return bSuccess;
}

////////////////////////////////////////////////////////////////////////////////////////////////
