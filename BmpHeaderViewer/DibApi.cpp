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

	SIZE_T cbSize = GlobalSize(hDib);
	if (cbSize < sizeof(BITMAPCOREHEADER))
		return FALSE;

	LPCSTR lpbi = (LPCSTR)GlobalLock(hDib);
	if (lpbi == NULL)
		return FALSE;

	DWORD dwHeaderSize = *(LPDWORD)lpbi;
	DWORD dwMasksSize = ColorMasksSize(lpbi);
	DWORD dwPaletteSize = PaletteSize(lpbi);
	DWORD dwImageSize = DibImageSize(lpbi);
	DWORD dwProfileSize = 0;

	SIZE_T cbOffBits = (SIZE_T)dwHeaderSize + dwMasksSize + dwPaletteSize;

	// Copy the header so that we can adjust biSizeImage and bV5ProfileData.
	// For this purpose, we can use a v5 structure for all bitmap variants.
	BITMAPV5HEADER bmhv5;
	ZeroMemory(&bmhv5, sizeof(bmhv5));
	CopyMemory(&bmhv5, lpbi, min(dwHeaderSize, sizeof(bmhv5)));

	if (bmhv5.bV5SizeImage)
		bmhv5.bV5SizeImage = dwImageSize;

	BOOL bHasProfile = DibHasColorProfile(lpbi);
	if (bHasProfile)
	{
		dwProfileSize = bmhv5.bV5ProfileSize;
		bmhv5.bV5ProfileData = (DWORD)(cbOffBits + dwImageSize);
	}

	SIZE_T cbDibSize = cbOffBits + dwImageSize + dwProfileSize;
	if (cbDibSize > cbSize)
	{
		GlobalUnlock(hDib);
		return FALSE;
	}

	BITMAPFILEHEADER bfh = { 0 };
	bfh.bfType = BFT_BITMAP;
	bfh.bfSize = (DWORD)(cbDibSize + sizeof(BITMAPFILEHEADER));
	bfh.bfOffBits = (DWORD)(cbOffBits + sizeof(BITMAPFILEHEADER));

	HANDLE hFile = CreateFile(lpszFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		GlobalUnlock(hDib);
		return FALSE;
	}

	// Write file header
	DWORD dwWrite = 0;
	if (!WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &dwWrite, NULL) || dwWrite != sizeof(BITMAPFILEHEADER))
	{
		GlobalUnlock(hDib);
		CloseHandle(hFile);
		DeleteFile(lpszFileName);
		return FALSE;
	}

	// Write bitmap header
	if (!WriteFile(hFile, &bmhv5, dwHeaderSize, &dwWrite, NULL) || dwWrite != dwHeaderSize)
	{
		GlobalUnlock(hDib);
		CloseHandle(hFile);
		DeleteFile(lpszFileName);
		return FALSE;
	}

	// Write color masks
	if (dwMasksSize)
	{
		if (!WriteFile(hFile, lpbi + dwHeaderSize, dwMasksSize, &dwWrite, NULL) || dwWrite != dwMasksSize)
		{
			GlobalUnlock(hDib);
			CloseHandle(hFile);
			DeleteFile(lpszFileName);
			return FALSE;
		}
	}

	// Write color table
	if (dwPaletteSize)
	{
		if (!WriteFile(hFile, FindDibPalette(lpbi), dwPaletteSize, &dwWrite, NULL) || dwWrite != dwPaletteSize)
		{
			GlobalUnlock(hDib);
			CloseHandle(hFile);
			DeleteFile(lpszFileName);
			return FALSE;
		}
	}

	// Write bitmap bits
	if (!WriteFile(hFile, FindDibBits(lpbi), dwImageSize, &dwWrite, NULL) || dwWrite != dwImageSize)
	{
		GlobalUnlock(hDib);
		CloseHandle(hFile);
		DeleteFile(lpszFileName);
		return FALSE;
	}

	// Write color profile
	if (bHasProfile)
	{
		if (!WriteFile(hFile, lpbi + ((LPBITMAPV5HEADER)lpbi)->bV5ProfileData, dwProfileSize, &dwWrite, NULL) ||
			dwWrite != dwProfileSize)
		{
			GlobalUnlock(hDib);
			CloseHandle(hFile);
			DeleteFile(lpszFileName);
			return FALSE;
		}
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

	if (!DibHasEmbeddedProfile((LPCSTR)lpbiv5))
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

	BOOL bIsCore = IS_OS2PM_DIB(lpbi);
	WORD wBitCount = bIsCore ? ((LPBITMAPCOREHEADER)lpbi)->bcBitCount : lpbi->biBitCount;
	BOOL bIs16Bpp = (wBitCount == 16);

	if ((wBitCount != 16 && wBitCount != 32) ||
		(lpbi->biSize >= sizeof(BITMAPINFOHEADER) &&
		(lpbi->biCompression != BI_RGB && lpbi->biCompression != BI_BITFIELDS)) ||
		DibIsCMYK((LPCSTR)lpbi))
	{
		GlobalUnlock(hDib);
		return NULL;
	}

	LONG lWidth  = 0;
	LONG lHeight = 0;
	GetDIBDimensions((LPCSTR)lpbi, &lWidth, &lHeight);

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
	if (hbmpDib == NULL || lpBGRA == NULL)
	{
		GlobalUnlock(hDib);
		return NULL;
	}

	BYTE cAlpha;
	LPBYTE lpSrc, lpDest;
	DWORD dwColor, dwRedMask, dwGreenMask, dwBlueMask, dwAlphaMask;
	ULONG ulIncrement = WIDTHBYTES(lWidth * wBitCount);
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
			dwAlphaMask = lpbi->biSize >= sizeof(BITMAPV3INFOHEADER) ? lpdwColorMasks[3] : 0;
		}
		else if (bIs16Bpp)
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
				lpSrc = lpDIB + (ULONG_PTR)h * ulIncrement;
				lpDest = lpBGRA + (ULONG_PTR)h * lWidth * 4;

				for (LONG w = 0; w < lWidth; w++)
				{
					dwColor = MAKELONG(MAKEWORD(lpSrc[0], lpSrc[1]),
						bIs16Bpp ? 0 : MAKEWORD(lpSrc[2], lpSrc[3]));
					cAlpha = GetColorValue(dwColor, dwAlphaMask);

					if (cAlpha != 0x00)
						bHasVisiblePixels = TRUE;
					if (cAlpha != 0xFF)
						bHasTransparentPixels = TRUE;

					*lpDest++ = Mul8Bit(GetColorValue(dwColor, dwBlueMask), cAlpha);
					*lpDest++ = Mul8Bit(GetColorValue(dwColor, dwGreenMask), cAlpha);
					*lpDest++ = Mul8Bit(GetColorValue(dwColor, dwRedMask), cAlpha);
					*lpDest++ = cAlpha;

					lpSrc += bIs16Bpp ? 2 : 4;
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

	SIZE_T cbSize = GlobalSize(hDib);
	if (cbSize == 0)
		return NULL;

	LPBYTE lpSrc = (LPBYTE)GlobalLock(hDib);
	if (lpSrc == NULL)
		return NULL;

	HANDLE hNewDib = GlobalAlloc(GHND, cbSize);
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

	__try { CopyMemory(lpDest, lpSrc, cbSize); }
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
// CreateClipboardDib creates the most descriptive clipboard format from the passed DIB.
// The system can then generate the other formats itself. But it looks like Windows still
// has the problem of synthesizing a valid DIBv5 from an RLE-compressed DIBv3 or from a
// DIBv3 with color masks. In such a case, the second parameter of this function can be
// used to force the creation of a DIBv5. This DIB can then also be placed on the clipboard.

HANDLE CreateClipboardDib(HANDLE hDib, UINT *puFormat)
{
	if (hDib == NULL)
		return NULL;

	BOOL bForceDibV5 = FALSE;
	if (puFormat != NULL)
		bForceDibV5 = (*puFormat == CF_DIBV5);

	SIZE_T cbSize = GlobalSize(hDib);
	if (cbSize == 0)
		return NULL;

	LPBYTE lpSrc = (LPBYTE)GlobalLock(hDib);
	if (lpSrc == NULL)
		return NULL;
	
	HANDLE hNewDib = NULL;
	DWORD dwSrcHeaderSize = *(LPDWORD)lpSrc;
	DWORD dwDestHeaderSize = bForceDibV5 ? sizeof(BITMAPV5HEADER) : sizeof(BITMAPINFOHEADER);

	if (dwSrcHeaderSize == sizeof(BITMAPCOREHEADER))
	{
		// Create a new DIBv3 (or a DIBv5, if requested) from a DIBv2
		LPBITMAPCOREINFO lpbmci = (LPBITMAPCOREINFO)lpSrc;
		UINT uColors = DibNumColors((LPCSTR)lpbmci);
		UINT uImageSize = DibImageSize((LPCSTR)lpbmci);

		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT | GMEM_DDESHARE,
			dwDestHeaderSize + uColors * sizeof(RGBQUAD) + uImageSize);
		if (hNewDib == NULL)
		{
			GlobalUnlock(hDib);
			return NULL;
		}

		LPBITMAPV5HEADER lpDest = (LPBITMAPV5HEADER)GlobalLock(hNewDib);
		if (lpDest == NULL)
		{
			GlobalFree(hNewDib);
			GlobalUnlock(hDib);
			return NULL;
		}

		lpDest->bV5Size      = dwDestHeaderSize;
		lpDest->bV5Width     = lpbmci->bmciHeader.bcWidth;
		lpDest->bV5Height    = lpbmci->bmciHeader.bcHeight;
		lpDest->bV5Planes    = lpbmci->bmciHeader.bcPlanes;
		lpDest->bV5BitCount  = lpbmci->bmciHeader.bcBitCount;
		lpDest->bV5SizeImage = uImageSize;
		lpDest->bV5ClrUsed   = uColors;

		if (bForceDibV5)
		{
			lpDest->bV5Intent = LCS_GM_IMAGES;
			lpDest->bV5CSType = LCS_sRGB;
		}

		__try
		{
			if (uColors > 0)
			{
				LPRGBQUAD lprgbqColors = (LPRGBQUAD)FindDibPalette((LPCSTR)lpDest);
				for (UINT u = 0; u < uColors; u++)
				{
					lprgbqColors[u].rgbRed   = lpbmci->bmciColors[u].rgbtRed;
					lprgbqColors[u].rgbGreen = lpbmci->bmciColors[u].rgbtGreen;
					lprgbqColors[u].rgbBlue  = lpbmci->bmciColors[u].rgbtBlue;
					lprgbqColors[u].rgbReserved = 0;
				}
			}

			CopyMemory(FindDibBits((LPCSTR)lpDest), FindDibBits((LPCSTR)lpSrc), uImageSize);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}
	else if (dwSrcHeaderSize == sizeof(BITMAPINFOHEADER2))
	{
		// Create a new DIBv3 (or DIBv5) from an OS/2 2.0 DIB
		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbSize + dwDestHeaderSize - dwSrcHeaderSize);
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
			ZeroMemory(lpDest + sizeof(BITMAPINFOHEADER), dwDestHeaderSize - sizeof(BITMAPINFOHEADER));
			CopyMemory(lpDest + dwDestHeaderSize, lpSrc + dwSrcHeaderSize, cbSize - dwSrcHeaderSize);

			// Update the v3 (or v5) header with the new header size
			((LPBITMAPINFOHEADER)lpDest)->biSize = dwDestHeaderSize;

			if (bForceDibV5)
			{
				((LPBITMAPV5HEADER)lpDest)->bV5Intent = LCS_GM_IMAGES;
				((LPBITMAPV5HEADER)lpDest)->bV5CSType = LCS_sRGB;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}
	else if ((dwSrcHeaderSize > sizeof(BITMAPINFOHEADER) || bForceDibV5) && dwSrcHeaderSize < sizeof(BITMAPV5HEADER))
	{
		// Create a new DIBv5 from an extended DIBv3, a DIBv4 or, if requested, from a DIBv3
		SIZE_T cbDibSize = sizeof(BITMAPV5HEADER) + PaletteSize((LPCSTR)lpSrc) + DibImageSize((LPCSTR)lpSrc);

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
			CopyMemory(lpDest, lpSrc, FindDibPalette((LPCSTR)lpSrc) - lpSrc);
			CopyMemory(lpDest + sizeof(BITMAPV5HEADER), FindDibPalette((LPCSTR)lpSrc),
				cbDibSize - sizeof(BITMAPV5HEADER));

			// Update the v5 header with the new header size
			((LPBITMAPV5HEADER)lpDest)->bV5Size = sizeof(BITMAPV5HEADER);
			((LPBITMAPV5HEADER)lpDest)->bV5Intent = LCS_GM_IMAGES;
			if (dwSrcHeaderSize < sizeof(BITMAPV4HEADER))
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

		if (cbDibSize > cbSize)
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
		hNewDib = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbSize);
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

		__try { CopyMemory(lpDest, lpSrc, cbSize); }
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hNewDib);
	}

	GlobalUnlock(hDib);

	// Set the clipboard format according to the created DIB
	if (puFormat != NULL)
	{
		*puFormat = CF_DIB;
		if (bForceDibV5 || (dwSrcHeaderSize > sizeof(BITMAPINFOHEADER) && dwSrcHeaderSize != sizeof(BITMAPINFOHEADER2)))
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

BOOL GetDIBDimensions(LPCSTR lpbi, LPLONG lplWidth, LPLONG lplHeight, BOOL bAbsolute)
{
	if (lpbi == NULL || lplWidth == NULL || lplHeight == NULL)
		return FALSE;

	if (IS_OS2PM_DIB(lpbi))
	{
		*lplWidth = ((LPBITMAPCOREHEADER)lpbi)->bcWidth;
		*lplHeight = ((LPBITMAPCOREHEADER)lpbi)->bcHeight;
	}
	else
	{
		LPBITMAPINFOHEADER lpbih = (LPBITMAPINFOHEADER)lpbi;
		*lplWidth = bAbsolute ? abs(lpbih->biWidth) : lpbih->biWidth;
		*lplHeight = bAbsolute ? abs(lpbih->biHeight) : lpbih->biHeight;
	}

	return TRUE;
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
	// In the case of an extended BITMAPINFOHEADER structure,
	// the color masks are saved within the header
	if (lpbi == NULL || !IS_WIN30_DIB(lpbi))
		return 0;

	// BI_BITFIELDS is only valid for 16-bpp and 32-bpp bitmaps.
	// If set, however, we assume that the masks are present.
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

UINT DibImageSize(LPCSTR lpbi)
{
	if (lpbi == NULL)
		return 0;

	if (IS_OS2PM_DIB(lpbi))
	{
		LPBITMAPCOREHEADER lpbc = (LPBITMAPCOREHEADER)lpbi;
		UINT64 ullBitsSize = WIDTHBYTES((UINT64)lpbc->bcWidth * lpbc->bcPlanes * lpbc->bcBitCount) * lpbc->bcHeight;
		return (ullBitsSize <= UINT_MAX ? (UINT)ullBitsSize : 0);
	}

	LPBITMAPINFOHEADER lpbih = (LPBITMAPINFOHEADER)lpbi;

	if (!DibIsCompressed(lpbi))
	{
		UINT64 ullBitsSize = WIDTHBYTES((UINT64)abs(lpbih->biWidth) * lpbih->biPlanes * lpbih->biBitCount) * abs(lpbih->biHeight);
		return (ullBitsSize <= UINT_MAX ? (UINT)ullBitsSize : 0);
	}

	return lpbih->biSizeImage;
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

BOOL DibHasEmbeddedProfile(LPCSTR lpbi)
{
	if (lpbi == NULL || !IS_WIN50_DIB(lpbi))
		return FALSE;

	LPBITMAPV5HEADER lpbiv5 = (LPBITMAPV5HEADER)lpbi;
	return (lpbiv5->bV5ProfileData != 0 && lpbiv5->bV5ProfileSize != 0 &&
		lpbiv5->bV5CSType == PROFILE_EMBEDDED);
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

	if (IS_OS2V2_DIB(lpbi))
		return (dwCompression != BCA_UNCOMP);

	return (dwCompression != BI_RGB &&
		dwCompression != BI_BITFIELDS &&
		dwCompression != BI_ALPHABITFIELDS &&
		dwCompression != BI_CMYK);
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
	if (lpbi == NULL)
		return FALSE;

	DWORD dwSize = *(LPDWORD)lpbi;

	if ((dwSize >= sizeof(BITMAPV4HEADER) && ((LPBITMAPV4HEADER)lpbi)->bV4CSType == LCS_DEVICE_CMYK) ||
		(dwSize >= sizeof(BITMAPINFOHEADER) && ((LPBITMAPINFOHEADER)lpbi)->biCompression == BI_CMYK))
		return TRUE;

	return FALSE;
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
