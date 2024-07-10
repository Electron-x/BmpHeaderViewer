////////////////////////////////////////////////////////////////////////////////////////////////
// DibApi.cpp - Copyright (c) 2024 by W. Rolke.
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
// Forward declarations of functions included in this code module

// Determines the value of a color component using a color mask
BYTE GetColorValue(DWORD dwPixel, DWORD dwMask);

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SaveBitmap(LPCTSTR lpszFileName, HANDLE hDib)
{
	if (hDib == NULL || lpszFileName == NULL)
		return FALSE;

	LPCSTR lpbi = (LPCSTR)GlobalLock(hDib);
	if (lpbi == NULL)
		return FALSE;

	DWORD dwOffBits = DibBitsOffset(lpbi);
	DWORD dwBitsSize = DibImageSize(lpbi);
	DWORD dwDibSize = dwOffBits + dwBitsSize;

	if (DibHasColorProfile(lpbi))
	{
		LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)lpbi;
		// Check for a gap between bitmap bits and profile data
		if (lpbiv5->bV5ProfileData > dwDibSize)
			dwDibSize += lpbiv5->bV5ProfileData - dwDibSize;
		// Check whether the profile data follows the bitmap bits
		if ((lpbiv5->bV5ProfileData + lpbiv5->bV5ProfileSize) != dwOffBits)
			dwDibSize += lpbiv5->bV5ProfileSize;
	}

	BITMAPFILEHEADER bmfHdr = { 0 };
	bmfHdr.bfType = BFT_BITMAP;
	bmfHdr.bfSize = dwDibSize + sizeof(BITMAPFILEHEADER);
	bmfHdr.bfOffBits = dwOffBits + sizeof(BITMAPFILEHEADER);

	HANDLE hFile = CreateFile(lpszFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		GlobalUnlock(hDib);
		return FALSE;
	}

	DWORD dwWrite = 0;
	if (!WriteFile(hFile, &bmfHdr, sizeof(BITMAPFILEHEADER), &dwWrite, NULL) ||
		dwWrite != sizeof(BITMAPFILEHEADER))
	{
		GlobalUnlock(hDib);
		CloseHandle(hFile);
		DeleteFile(lpszFileName);
		return FALSE;
	}

	if (!WriteFile(hFile, (LPVOID)lpbi, dwDibSize, &dwWrite, NULL) || dwWrite != dwDibSize)
	{
		GlobalUnlock(hDib);
		CloseHandle(hFile);
		DeleteFile(lpszFileName);
		return FALSE;
	}

	GlobalUnlock(hDib);
	CloseHandle(hFile);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL SaveProfile(LPCTSTR lpszFileName, HANDLE hDib)
{
	if (hDib == NULL || lpszFileName == NULL)
		return FALSE;

	LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)GlobalLock(hDib);
	if (lpbiv5 == NULL)
		return FALSE;

	if (!IS_WIN50_DIB(lpbiv5) || lpbiv5->bV5CSType != PROFILE_EMBEDDED ||
		lpbiv5->bV5ProfileData == 0 || lpbiv5->bV5ProfileSize == 0)
	{
		GlobalUnlock(hDib);
		return FALSE;
	}

	HANDLE hFile = CreateFile(lpszFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		GlobalUnlock(hDib);
		return FALSE;
	}

	DWORD dwWrite = 0;
	if (!WriteFile(hFile, (LPBYTE)lpbiv5 + lpbiv5->bV5ProfileData, lpbiv5->bV5ProfileSize,
		&dwWrite, NULL) || dwWrite != lpbiv5->bV5ProfileSize)
	{
		GlobalUnlock(hDib);
		CloseHandle(hFile);
		DeleteFile(lpszFileName);
		return FALSE;
	}

	GlobalUnlock(hDib);
	CloseHandle(hFile);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HBITMAP CreatePremultipliedBitmap(HANDLE hDib)
{
	if (hDib == NULL)
		return NULL;

	LPBITMAPINFOHEADER lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDib);
	if (lpbi == NULL)
		return NULL;

	LPBITMAPCOREHEADER lpbc = (LPBITMAPCOREHEADER)lpbi;
	BOOL bIsCore = IS_OS2PM_DIB(lpbi);
	WORD wBitCount = bIsCore ? lpbc->bcBitCount : lpbi->biBitCount;
	BOOL bIs16Bit = (wBitCount == 16);

	if ((wBitCount != 16 && wBitCount != 32) ||
		(lpbi->biSize >= sizeof(BITMAPINFOHEADER) &&
		(lpbi->biCompression != BI_RGB && lpbi->biCompression != BI_BITFIELDS)) ||
		DibIsCMYK((LPCSTR)lpbi))
	{
		GlobalUnlock(hDib);
		return NULL;
	}

	LONG lWidth  = bIsCore ? lpbc->bcWidth : lpbi->biWidth;
	LONG lHeight = bIsCore ? lpbc->bcHeight : lpbi->biHeight;

	BITMAPINFO bmi = { {0} };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = lWidth;
	bmi.bmiHeader.biHeight = lHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	lWidth = abs(lWidth);
	lHeight = abs(lHeight);

	LPBYTE lpBGRA = NULL;
	LPBYTE lpDIB = FindDibBits((LPCSTR)lpbi);

	HBITMAP hbmpDib = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (PVOID*)&lpBGRA, NULL, 0);
	if (hbmpDib == NULL)
	{
		GlobalUnlock(hDib);
		return NULL;
	}

	BYTE cAlpha;
	LPBYTE lpSrc, lpDest;
	DWORD dwColor, dwRedMask, dwGreenMask, dwBlueMask, dwAlphaMask;
	DWORD dwIncrement = WIDTHBYTES(lWidth * wBitCount);
	BOOL bHasVisiblePixels = FALSE;
	BOOL bHasTransparentPixels = FALSE;

	__try
	{
		if (!bIsCore && lpbi->biCompression == BI_BITFIELDS)
		{
			LPDWORD lpdwColorMasks = (LPDWORD)&(((LPBITMAPINFO)lpbi)->bmiColors[0]);
			dwRedMask   = lpdwColorMasks[0];
			dwGreenMask = lpdwColorMasks[1];
			dwBlueMask  = lpdwColorMasks[2];
			dwAlphaMask = lpbi->biSize >= 56 ? lpdwColorMasks[3] : 0;
		}
		else if (bIs16Bit)
		{
			dwRedMask   = 0x00007C00;
			dwGreenMask = 0x000003E0;
			dwBlueMask  = 0x0000001F;
			dwAlphaMask = 0x00008000;
		}
		else
		{
			dwRedMask   = 0x00FF0000;
			dwGreenMask = 0x0000FF00;
			dwBlueMask  = 0x000000FF;
			dwAlphaMask = 0xFF000000;
		}

		if (dwAlphaMask)
		{
			for (LONG h = 0; h < lHeight; h++)
			{
				lpSrc = lpDIB + (ULONG_PTR)h * dwIncrement;
				lpDest = lpBGRA + (ULONG_PTR)h * lWidth * 4;

				for (LONG w = 0; w < lWidth; w++)
				{
					dwColor = MAKELONG(MAKEWORD(lpSrc[0], lpSrc[1]),
						bIs16Bit ? 0 : MAKEWORD(lpSrc[2], lpSrc[3]));
					cAlpha = GetColorValue(dwColor, dwAlphaMask);

					if (cAlpha != 0x00)
						bHasVisiblePixels = TRUE;
					if (cAlpha != 0xFF)
						bHasTransparentPixels = TRUE;

					*lpDest++ = Mul8Bit(GetColorValue(dwColor, dwBlueMask), cAlpha);
					*lpDest++ = Mul8Bit(GetColorValue(dwColor, dwGreenMask), cAlpha);
					*lpDest++ = Mul8Bit(GetColorValue(dwColor, dwRedMask), cAlpha);
					*lpDest++ = cAlpha;

					lpSrc += bIs16Bit ? 2 : 4;
				}
			}
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { ; }

	GlobalUnlock(hDib);

	if (!bHasVisiblePixels || !bHasTransparentPixels)
	{ // The image is completely transparent or completely opaque
		DeleteBitmap(hbmpDib);
		return NULL;
	}

	return hbmpDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HBITMAP FreeBitmap(HBITMAP hbmpDib)
{
	if (hbmpDib == NULL)
		return NULL;

	if (DeleteBitmap(hbmpDib))
		return NULL;

	return hbmpDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE FreeDib(HANDLE hDib)
{
	if (hDib == NULL)
		return NULL;

	return GlobalFree(hDib);
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE CopyDib(HANDLE hDib)
{
	if (hDib == NULL)
		return NULL;

	SIZE_T cbLen = GlobalSize(hDib);
	if (cbLen == 0)
		return NULL;

	LPBYTE lpSrc = (LPBYTE)GlobalLock(hDib);
	if (lpSrc == NULL)
		return NULL;

	HANDLE hNewDib = GlobalAlloc(GHND, cbLen);
	if (hNewDib == NULL)
	{
		GlobalUnlock(hDib);
		return NULL;
	}

	LPBYTE lpDest = (LPBYTE)GlobalLock(hNewDib);
	if (lpDest == NULL)
	{
		GlobalFree(hNewDib);
		GlobalUnlock(hDib);
		return NULL;
	}

	__try { CopyMemory(lpDest, lpSrc, cbLen); }
	__except (EXCEPTION_EXECUTE_HANDLER) { ; }

	GlobalUnlock(hNewDib);
	GlobalUnlock(hDib);

	return hNewDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE DecompressDib(HANDLE hDib)
{
	if (hDib == NULL)
		return NULL;

	HANDLE hDibNew = NULL;

#ifdef ICVERSION

	LPBITMAPINFO lpbmi = (LPBITMAPINFO)GlobalLock(hDib);
	if (lpbmi == NULL)
		return NULL;

	if (DibIsCustomFormat((LPCSTR)lpbmi))
		hDibNew = ICImageDecompress(NULL, 0, lpbmi, FindDibBits((LPCSTR)lpbmi), NULL);

	GlobalUnlock(hDib);

#endif // ICVERSION

	return hDibNew;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE ChangeDibBitDepth(HANDLE hDib, WORD wBitCount)
{
	if (hDib == NULL)
		return NULL;

	HDC hdc = GetDC(NULL);

	if (wBitCount == 0)
		wBitCount = (WORD)GetDeviceCaps(hdc, BITSPIXEL);

	HPALETTE hpalOld = NULL;
	HPALETTE hpal = CreateDibPalette(hDib);
	if (hpal != NULL)
	{
		hpalOld = SelectPalette(hdc, hpal, FALSE);
		RealizePalette(hdc);
	}

	HBITMAP hBitmap = NULL;
	LPSTR lpbi = (LPSTR)GlobalLock(hDib);
	if (lpbi != NULL)
	{
		SetICMMode(hdc, g_nIcmMode);
		// Convert the DIB to a compatible bitmap with the bit depth of the screen
		// (CBM_CREATEDIB and CreateDIBSection doesn't support RLE compressed DIBs)
		hBitmap = CreateDIBitmap(hdc, (LPBITMAPINFOHEADER)lpbi, CBM_INIT, FindDibBits(lpbi),
			(LPBITMAPINFO)lpbi, DIB_RGB_COLORS);

		GlobalUnlock(hDib);
	}

	HANDLE hNewDib = NULL;
	if (hBitmap != NULL)
		hNewDib = ConvertBitmapToDib(hBitmap, hdc, wBitCount);

	if (hBitmap != NULL)
		DeleteBitmap(hBitmap);
	if (hpalOld != NULL)
		SelectPalette(hdc, hpalOld, FALSE);
	if (hpal != NULL)
		DeletePalette(hpal);

	ReleaseDC(NULL, hdc);

	return hNewDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE ConvertBitmapToDib(HBITMAP hBitmap, HDC hdc, WORD wBitCount)
{
	if (hBitmap == NULL)
		return NULL;

	BITMAP bm = { 0 };
	if (!GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&bm))
		return NULL;

	if (wBitCount == 0)
		wBitCount = bm.bmPlanes * bm.bmBitsPixel;

	// Define the format of the destination DIB
	BITMAPINFOHEADER bi = { 0 };
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = bm.bmWidth;
	bi.biHeight = bm.bmHeight;
	bi.biPlanes = 1;
	bi.biBitCount = wBitCount;
	bi.biCompression = BI_RGB;

	// Request memory for the target DIB. The memory block must
	// be large enough to hold the data supplied by GetDIBits.
	HANDLE hDib = GlobalAlloc(GHND, (SIZE_T)DibBitsOffset((LPCSTR)&bi) + DibImageSize((LPCSTR)&bi));
	if (hDib == NULL)
		return NULL;

	LPBITMAPINFO lpbmi = (LPBITMAPINFO)GlobalLock(hDib);
	if (lpbmi == NULL)
	{
		GlobalFree(hDib);
		return NULL;
	}

	BOOL bReleaseDC = FALSE;
	if (hdc == NULL)
	{
		hdc = GetDC(NULL);
		bReleaseDC = TRUE;
	}

	// Set the information header
	lpbmi->bmiHeader = bi;
	// Retrieve the color table and the bitmap bits. GetDIBits also fills in biSizeImage.
	int nRet = GetDIBits(hdc, hBitmap, 0, (UINT)bi.biHeight, FindDibBits((LPCSTR)lpbmi), lpbmi, DIB_RGB_COLORS);

	if (bReleaseDC)
	{
		ReleaseDC(NULL, hdc);
		hdc = NULL;
	}

	GlobalUnlock(hDib);

	if (!nRet)
	{
		GlobalFree(hDib);
		hDib = NULL;
	}

	return hDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE CreateDibFromClipboardDib(HANDLE hDib)
{
	if (hDib == NULL)
		return NULL;

	SIZE_T cbLen = GlobalSize(hDib);
	if (cbLen == 0)
		return NULL;

	LPBYTE lpSrc = (LPBYTE)GlobalLock(hDib);
	if (lpSrc == NULL)
		return NULL;

	if (!DibHasColorProfile((LPCSTR)lpSrc))
	{
		GlobalUnlock(hDib);
		// Nothing to fix. Just copy the DIB.
		return CopyDib(hDib);
	}

	LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)lpSrc;
	DWORD dwInfoSize = lpbiv5->bV5Size + PaletteSize((LPCSTR)lpSrc);
	DWORD dwProfileSize = lpbiv5->bV5ProfileSize;
	SIZE_T cbImageSize = DibImageSize((LPCSTR)lpSrc);
	SIZE_T cbDibSize = cbImageSize + dwInfoSize + dwProfileSize;

	if (cbDibSize > cbLen)
	{
		GlobalUnlock(hDib);
		return NULL;
	}

	HANDLE hNewDib = GlobalAlloc(GHND, cbDibSize);
	if (hNewDib == NULL)
	{
		GlobalUnlock(hDib);
		return NULL;
	}

	LPBYTE lpDest = (LPBYTE)GlobalLock(hNewDib);
	if (lpDest == NULL)
	{
		GlobalFree(hNewDib);
		GlobalUnlock(hDib);
		return NULL;
	}

	__try
	{
		CopyMemory(lpDest, lpSrc, dwInfoSize);
		// FindDibBits must support DIBs in which the profile data is located before the bitmap bits
		CopyMemory(lpDest + dwInfoSize, FindDibBits((LPCSTR)lpSrc), cbImageSize);
		CopyMemory(lpDest + dwInfoSize + cbImageSize, lpSrc + lpbiv5->bV5ProfileData, dwProfileSize);

		// Update the header with the new position of the profile data
		((LPBITMAPV5HEADER)lpDest)->bV5ProfileData = dwInfoSize + (DWORD)cbImageSize;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) { ; }

	GlobalUnlock(hNewDib);
	GlobalUnlock(hDib);

	return hNewDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE CreateClipboardDib(HANDLE hDib, UINT *puFormat)
{
	if (hDib == NULL)
		return NULL;

	SIZE_T cbLen = GlobalSize(hDib);
	if (cbLen == 0)
		return NULL;

	LPBYTE lpSrc = (LPBYTE)GlobalLock(hDib);
	if (lpSrc == NULL)
		return NULL;
	
	HANDLE hNewDib = NULL;
	DWORD dwSrcHeaderSize = *(LPDWORD)lpSrc;

	if (dwSrcHeaderSize == sizeof(BITMAPCOREHEADER))
	{
		// Create a new DIBv3 from a DIBv2
		LPBITMAPCOREINFO lpbmci = (LPBITMAPCOREINFO)lpSrc;
		UINT uColors = DibNumColors((LPCSTR)lpbmci);
		UINT uImageSize = DibImageSize((LPCSTR)lpbmci);

		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT | GMEM_DDESHARE,
			sizeof(BITMAPINFOHEADER) + uColors * sizeof(RGBQUAD) + uImageSize);
		if (hNewDib == NULL)
		{
			GlobalUnlock(hDib);
			return NULL;
		}

		LPBYTE lpDest = (LPBYTE)GlobalLock(hNewDib);
		if (lpDest == NULL)
		{
			GlobalFree(hNewDib);
			GlobalUnlock(hDib);
			return NULL;
		}

		LPBITMAPINFO lpbmi = (LPBITMAPINFO)lpDest;
		lpbmi->bmiHeader.biSize      = sizeof(BITMAPINFOHEADER);
		lpbmi->bmiHeader.biWidth     = lpbmci->bmciHeader.bcWidth;
		lpbmi->bmiHeader.biHeight    = lpbmci->bmciHeader.bcHeight;
		lpbmi->bmiHeader.biPlanes    = lpbmci->bmciHeader.bcPlanes;
		lpbmi->bmiHeader.biBitCount  = lpbmci->bmciHeader.bcBitCount;
		lpbmi->bmiHeader.biSizeImage = uImageSize;
		lpbmi->bmiHeader.biClrUsed   = uColors;

		__try
		{
			if (uColors > 0)
			{
				RGBQUAD rgbQuad = { 0 };
				for (UINT u = 0; u < uColors; u++)
				{
					rgbQuad.rgbRed   = lpbmci->bmciColors[u].rgbtRed;
					rgbQuad.rgbGreen = lpbmci->bmciColors[u].rgbtGreen;
					rgbQuad.rgbBlue  = lpbmci->bmciColors[u].rgbtBlue;
					rgbQuad.rgbReserved = 0;
					lpbmi->bmiColors[u] = rgbQuad;
				}
			}

			CopyMemory(FindDibBits((LPCSTR)lpDest), FindDibBits((LPCSTR)lpSrc), uImageSize);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}
	else if (dwSrcHeaderSize == 64)
	{
		// Create a new DIBv3 from an OS/2 2.0 DIB
		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
			cbLen + sizeof(BITMAPINFOHEADER) - dwSrcHeaderSize);
		if (hNewDib == NULL)
		{
			GlobalUnlock(hDib);
			return NULL;
		}

		LPBYTE lpDest = (LPBYTE)GlobalLock(hNewDib);
		if (lpDest == NULL)
		{
			GlobalFree(hNewDib);
			GlobalUnlock(hDib);
			return NULL;
		}

		__try
		{
			CopyMemory(lpDest, lpSrc, sizeof(BITMAPINFOHEADER));
			CopyMemory(lpDest + sizeof(BITMAPINFOHEADER), lpSrc + dwSrcHeaderSize, cbLen - dwSrcHeaderSize);

			// Update the v3 header with the new header size
			((LPBITMAPINFOHEADER)lpDest)->biSize = sizeof(BITMAPINFOHEADER);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}
	else if (dwSrcHeaderSize == 52 || dwSrcHeaderSize == 56 || dwSrcHeaderSize == sizeof(BITMAPV4HEADER))
	{
		// Create a new DIBv5 from an extended DIBv3 or from a DIBv4
		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT | GMEM_DDESHARE,
			cbLen + sizeof(BITMAPV5HEADER) - dwSrcHeaderSize);
		if (hNewDib == NULL)
		{
			GlobalUnlock(hDib);
			return NULL;
		}

		LPBYTE lpDest = (LPBYTE)GlobalLock(hNewDib);
		if (lpDest == NULL)
		{
			GlobalFree(hNewDib);
			GlobalUnlock(hDib);
			return NULL;
		}

		__try
		{
			CopyMemory(lpDest, lpSrc, dwSrcHeaderSize);
			CopyMemory(lpDest + sizeof(BITMAPV5HEADER), lpSrc + dwSrcHeaderSize, cbLen - dwSrcHeaderSize);

			// Update the v5 header with the new header size
			((LPBITMAPV5HEADER)lpDest)->bV5Size = sizeof(BITMAPV5HEADER);
			((LPBITMAPV5HEADER)lpDest)->bV5Intent = LCS_GM_IMAGES;
			if (dwSrcHeaderSize == 52 || dwSrcHeaderSize == 56)
				((LPBITMAPV5HEADER)lpDest)->bV5CSType = LCS_sRGB;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}
	else if (DibHasColorProfile((LPCSTR)lpSrc))
	{
		// The ICC profile data of a CF_DIBV5 DIB must follow the color table
		LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)lpSrc;
		DWORD dwInfoSize = lpbiv5->bV5Size + PaletteSize((LPCSTR)lpSrc);
		DWORD dwProfileSize = lpbiv5->bV5ProfileSize;
		SIZE_T cbImageSize = DibImageSize((LPCSTR)lpSrc);
		SIZE_T cbDibSize = cbImageSize + dwInfoSize + dwProfileSize;

		if (cbDibSize > cbLen)
		{
			GlobalUnlock(hDib);
			return NULL;
		}

		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT | GMEM_DDESHARE, cbDibSize);
		if (hNewDib == NULL)
		{
			GlobalUnlock(hDib);
			return NULL;
		}

		LPBYTE lpDest = (LPBYTE)GlobalLock(hNewDib);
		if (lpDest == NULL)
		{
			GlobalFree(hNewDib);
			GlobalUnlock(hDib);
			return NULL;
		}

		__try
		{
			CopyMemory(lpDest, lpSrc, dwInfoSize);
			CopyMemory(lpDest + dwInfoSize, lpSrc + lpbiv5->bV5ProfileData, dwProfileSize);
			CopyMemory(lpDest + dwInfoSize + dwProfileSize, FindDibBits((LPCSTR)lpSrc), cbImageSize);

			// Update the header with the new position of the profile data
			((LPBITMAPV5HEADER)lpDest)->bV5ProfileData = dwInfoSize;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}
	else
	{
		// Copy the packed DIB
		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbLen);
		if (hNewDib == NULL)
		{
			GlobalUnlock(hDib);
			return NULL;
		}

		LPBYTE lpDest = (LPBYTE)GlobalLock(hNewDib);
		if (lpDest == NULL)
		{
			GlobalFree(hNewDib);
			GlobalUnlock(hDib);
			return NULL;
		}

		__try { CopyMemory(lpDest, lpSrc, cbLen); }
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}

	GlobalUnlock(hDib);

	// Set the clipboard format according to the created DIB
	if (puFormat != NULL)
	{
		*puFormat = CF_DIB;
		if (dwSrcHeaderSize > sizeof(BITMAPINFOHEADER) && dwSrcHeaderSize != 64)
			*puFormat = CF_DIBV5;
	}

	return hNewDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

HPALETTE CreateDibPalette(HANDLE hDib)
{
	LPSTR lpbi = NULL;
	UINT uNumColors = 0;
	HGLOBAL hMem = NULL;
	LPLOGPALETTE lppal = NULL;
	HPALETTE hpal = NULL;

	if (hDib == NULL)
		goto CreateHalftonePal;

	// Create a palette from the colors of the DIB
	lpbi = (LPSTR)GlobalLock(hDib);
	if (lpbi == NULL)
		goto CreateHalftonePal;

	uNumColors = DibNumColors(lpbi);
	if (uNumColors == 0)
		goto CreateHalftonePal;

	hMem = GlobalAlloc(GHND, sizeof(LOGPALETTE) + sizeof(PALETTEENTRY) * uNumColors);
	if (hMem == NULL)
		goto CreateHalftonePal;

	lppal = (LPLOGPALETTE)GlobalLock(hMem);
	if (lppal == NULL)
		goto CreateHalftonePal;

	lppal->palVersion = PALVERSION;
	lppal->palNumEntries = (WORD)uNumColors;

	if (IS_OS2PM_DIB(lpbi))
	{
		LPBITMAPCOREINFO lpbmc = (LPBITMAPCOREINFO)lpbi;
		for (UINT i = 0; i < uNumColors; i++)
		{
			lppal->palPalEntry[i].peRed   = lpbmc->bmciColors[i].rgbtRed;
			lppal->palPalEntry[i].peGreen = lpbmc->bmciColors[i].rgbtGreen;
			lppal->palPalEntry[i].peBlue  = lpbmc->bmciColors[i].rgbtBlue;
			lppal->palPalEntry[i].peFlags = 0;
		}
	}
	else
	{
		LPRGBQUAD lprgbqColors = (LPRGBQUAD)FindDibPalette(lpbi);
		for (UINT i = 0; i < uNumColors; i++)
		{
			lppal->palPalEntry[i].peRed   = lprgbqColors[i].rgbRed;
			lppal->palPalEntry[i].peGreen = lprgbqColors[i].rgbGreen;
			lppal->palPalEntry[i].peBlue  = lprgbqColors[i].rgbBlue;
			lppal->palPalEntry[i].peFlags = 0;
		}
	}

	hpal = CreatePalette(lppal);

CreateHalftonePal:

	if (lppal != NULL)
		GlobalUnlock(hMem);
	if (hMem != NULL)
		GlobalFree(hMem);
	if (lpbi != NULL)
		GlobalUnlock(hDib);

	if (hpal == NULL)
	{ // Create a halftone palette
		HDC hdc = GetDC(NULL);
		hpal = CreateHalftonePalette(hdc);
		ReleaseDC(NULL, hdc);
	}

	return hpal;
}

////////////////////////////////////////////////////////////////////////////////////////////////

UINT DibNumColors(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return 0;

	WORD  wBitCount = 0;
	DWORD dwClrUsed = 0;

	if (IS_OS2PM_DIB(lpbi))
		wBitCount = ((LPBITMAPCOREHEADER)lpbi)->bcBitCount;
	else
	{
		wBitCount = ((LPBITMAPINFOHEADER)lpbi)->biBitCount;
		dwClrUsed = ((LPBITMAPINFOHEADER)lpbi)->biClrUsed;
		// Allow up to 4096 color table entries
		if (dwClrUsed > 0 && dwClrUsed <= (1U << 12U))
			return dwClrUsed;
	}

	if (wBitCount > 0 && wBitCount <= 12)
		return (1U << wBitCount);
	else
		return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////

UINT ColorMasksSize(LPCSTR lpbi)
{
	if (lpbi == NULL || !IS_WIN30_DIB(lpbi))
		return 0;

	if (((LPBITMAPINFOHEADER)lpbi)->biCompression == BI_BITFIELDS)
		return 3 * sizeof(DWORD);
	else if (((LPBITMAPINFOHEADER)lpbi)->biCompression == BI_ALPHABITFIELDS)
		return 4 * sizeof(DWORD);

	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////

UINT PaletteSize(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return 0;

	return DibNumColors(lpbi) * (IS_OS2PM_DIB(lpbi) ? sizeof(RGBTRIPLE) : sizeof(RGBQUAD));
}

////////////////////////////////////////////////////////////////////////////////////////////////

UINT DibImageSize(LPCSTR lpbi, BOOL bCalculate)
{
	if (lpbi == NULL)
		return 0;

	if (IS_OS2PM_DIB(lpbi))
	{
		LPBITMAPCOREHEADER lpbc = (LPBITMAPCOREHEADER)lpbi;
		return WIDTHBYTES(lpbc->bcWidth * lpbc->bcPlanes * lpbc->bcBitCount) * lpbc->bcHeight;
	}

	LPBITMAPINFOHEADER lpbih = (LPBITMAPINFOHEADER)lpbi;

	if (lpbih->biSizeImage && (!bCalculate || DibIsCompressed(lpbi)))
		return lpbih->biSizeImage;

	return WIDTHBYTES(abs(lpbih->biWidth) * lpbih->biPlanes * lpbih->biBitCount) * abs(lpbih->biHeight);
}

////////////////////////////////////////////////////////////////////////////////////////////////

UINT DibBitsOffset(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return 0;

	UINT uOffBits = *(LPDWORD)lpbi + ColorMasksSize(lpbi) + PaletteSize(lpbi);

	// Check whether the profile data is placed before or after the bitmap bits
	LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)lpbi;
	if (!IS_WIN50_DIB(lpbi) || lpbiv5->bV5ProfileData != uOffBits)
		return uOffBits;

	return uOffBits + lpbiv5->bV5ProfileSize;
}

////////////////////////////////////////////////////////////////////////////////////////////////

LPBYTE FindDibPalette(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return NULL;

	return (LPBYTE)lpbi + *(LPDWORD)lpbi + ColorMasksSize(lpbi);
}

////////////////////////////////////////////////////////////////////////////////////////////////

LPBYTE FindDibBits(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return NULL;

	return (LPBYTE)lpbi + DibBitsOffset(lpbi);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DibHasColorProfile(LPCSTR lpbi)
{
	if (lpbi == NULL || !IS_WIN50_DIB(lpbi))
		return FALSE;

	LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)lpbi;
	return (lpbiv5->bV5ProfileData != 0 && lpbiv5->bV5ProfileSize != 0 &&
		(lpbiv5->bV5CSType == PROFILE_LINKED || lpbiv5->bV5CSType == PROFILE_EMBEDDED));
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DibHasColorSpaceData(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return FALSE;

	LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)lpbi;
	DWORD dwSize = lpbiv5->bV5Size;

	// We ignore linked profiles because they don't work with ICM inside DC
	return ((dwSize >= sizeof(BITMAPV4HEADER) && lpbiv5->bV5CSType == LCS_CALIBRATED_RGB) ||
		(dwSize >= sizeof(BITMAPV5HEADER) && lpbiv5->bV5CSType == PROFILE_EMBEDDED));
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DibIsCompressed(LPCSTR lpbi)
{
	if (lpbi == NULL || IS_OS2PM_DIB(lpbi))
		return FALSE;

	DWORD dwCompression = ((LPBITMAPINFOHEADER)lpbi)->biCompression;

	return (dwCompression != BI_RGB &&
		dwCompression != BI_BITFIELDS &&
		dwCompression != BI_ALPHABITFIELDS);
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DibIsCustomFormat(LPCSTR lpbi)
{
	if (lpbi == NULL || !IS_WIN30_DIB(lpbi))
		return FALSE;

	DWORD dwCompression = ((LPBITMAPINFOHEADER)lpbi)->biCompression;

	return (isprint(dwCompression & 0xff) &&
		isprint((dwCompression >> 8) & 0xff) &&
		isprint((dwCompression >> 16) & 0xff) &&
		isprint((dwCompression >> 24) & 0xff));
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DibIsCMYK(LPCSTR lpbi)
{
	if (lpbi == NULL || *(LPDWORD)lpbi < sizeof(BITMAPV4HEADER))
		return FALSE;

	return (((LPBITMAPV4HEADER)lpbi)->bV4CSType == LCS_DEVICE_CMYK);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// IsDibSupported: Determines whether a display driver can display a specific DIB.
// This function can also be used to check whether a video driver supports inverted DIBs.
// Returns TRUE even if the info could not be retrieved (e.g. for a core DIB).

BOOL IsDibSupported(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return FALSE;

	return (QueryDibSupport(lpbi) & QDI_DIBTOSCREEN) != 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// The QUERYDIBSUPPORT escape function determines whether a display driver supports a specific
// DIB. It checks if biCompression contains either BI_RGB, BI_RLE4, BI_RLE8 or BI_BITFIELDS
// and if the value of biBitCount matches the format. Core DIBs are therefore not supported.

DWORD QueryDibSupport(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return (DWORD)-1;

	HDC hdc = GetDC(NULL);
	if (hdc == NULL)
		return (DWORD)-1;

	DWORD dwFlags = 0;
	if (Escape(hdc, QUERYDIBSUPPORT, *(LPDWORD)lpbi, lpbi, (LPVOID)&dwFlags) <= 0)
		dwFlags = (DWORD)-1;

	ReleaseDC(NULL, hdc);

	return dwFlags;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static BYTE GetColorValue(DWORD dwPixel, DWORD dwMask)
{
	if (dwMask == 0)
		return 0;

	DWORD dwColor = dwPixel & dwMask;

	while ((dwMask & 0x80000000) == 0)
	{
		dwColor = dwColor << 1;
		dwMask  = dwMask  << 1;
	}

	return HIBYTE(HIWORD(dwColor));
}

////////////////////////////////////////////////////////////////////////////////////////////////
