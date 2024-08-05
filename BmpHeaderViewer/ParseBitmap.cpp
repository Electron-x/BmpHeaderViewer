////////////////////////////////////////////////////////////////////////////////////////////////
// ParseBitmap.cpp - Copyright (c) 2024 by W. Rolke.
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

// Outputs an ICC profile signature given in big-endian format
void PrintProfileSignature(HWND hwndEdit, LPCTSTR lpszName, DWORD dwSignature, BOOL bAddCrLf = TRUE);
// Outputs ICC profile tag data. Only simple structures that can be displayed in one line are supported.
void PrintProfileTagData(HWND hwndEdit, LPCTSTR lpszName, LPCSTR lpData, DWORD dwSize, BOOL bAddCrLf = FALSE);

// Returns the color name if the color passed is one of the 20 static system palette colors
BOOL GetColorName(COLORREF rgbColor, LPCTSTR* lpszName);

// Converts a half-precision floating-point number to a single-precision float.
// Special values like subnormals, infinities, NaN's and -0 are not supported.
float Half2Float(WORD h);

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL ParseBitmap(HWND hDlg, HANDLE hFile, DWORD dwFileSize)
{
	if (hDlg == NULL || hFile == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
	if (hwndEdit == NULL)
		return FALSE;

	HWND hwndThumb = GetDlgItem(hDlg, IDC_THUMB);
	if (hwndThumb == NULL)
		return FALSE;

	BITMAPFILEHEADER bfh;
	DWORD dwFileHeaderSize = sizeof(bfh);

	if (dwFileSize < dwFileHeaderSize)
	{
		OutputTextFromID(hwndEdit, IDS_CORRUPTED);
		return FALSE;
	}

	// Jump to the beginning of the file
	if (!FileSeekBegin(hFile, 0))
		return FALSE;

	// Read the file header
	ZeroMemory(&bfh, sizeof(bfh));
	if (!MyReadFile(hFile, (LPVOID)&bfh, sizeof(bfh)))
		return FALSE;

	if (bfh.bfType == BFT_BITMAPARRAY)
	{ // OS/2 Bitmap Array
		LPBITMAPARRAYFILEHEADER lpbafh = (LPBITMAPARRAYFILEHEADER)&bfh;

		OutputText(hwndEdit, g_szSepThin);
		OutputTextFmt(hwndEdit, TEXT("Type:\t\t%hc%hc\r\n"), LOBYTE(lpbafh->usType), HIBYTE(lpbafh->usType));
		OutputTextFmt(hwndEdit, TEXT("Size:\t\t%u bytes\r\n"), lpbafh->cbSize);
		OutputTextFmt(hwndEdit, TEXT("OffNext:\t%u bytes\r\n"), lpbafh->offNext);
		OutputTextFmt(hwndEdit, TEXT("CxDisplay:\t%u\r\n"), lpbafh->cxDisplay);
		OutputTextFmt(hwndEdit, TEXT("CyDisplay:\t%u\r\n"), lpbafh->cyDisplay);

		// Proceed only if the array contains only one bitmap
		if (lpbafh->offNext != 0)
		{
			OutputTextFromID(hwndEdit, IDS_BITMAPARRAY);
			SetThumbnailText(hwndThumb, IDS_UNSUPPORTED);
			return FALSE;
		}

		dwFileHeaderSize += dwFileHeaderSize;
		if (dwFileSize < dwFileHeaderSize)
		{
			OutputTextFromID(hwndEdit, IDS_CORRUPTED);
			return FALSE;
		}

		// Read the file header of the first bitmap
		ZeroMemory(&bfh, sizeof(bfh));
		if (!MyReadFile(hFile, (LPVOID)&bfh, sizeof(bfh)))
			return FALSE;

		if (bfh.bfType != BFT_BMAP)
		{ // No support for icons and pointers
			OutputTextFromID(hwndEdit, IDS_ICON_POINTER);
			SetThumbnailText(hwndThumb, IDS_UNSUPPORTED);
			return FALSE;
		}
	}

	// Output the file header (we have already checked bfType in ParseFile)
	OutputText(hwndEdit, g_szSepThin);
	OutputTextFmt(hwndEdit, TEXT("Type:\t\t%hc%hc\r\n"), LOBYTE(bfh.bfType), HIBYTE(bfh.bfType));
	OutputTextFmt(hwndEdit, TEXT("Size:\t\t%u bytes\r\n"), bfh.bfSize);
	OutputTextFmt(hwndEdit, TEXT("Reserved1:\t%u\r\n"), bfh.bfReserved1);
	OutputTextFmt(hwndEdit, TEXT("Reserved2:\t%u\r\n"), bfh.bfReserved2);
	OutputTextFmt(hwndEdit, TEXT("OffBits:\t%u bytes\r\n"), bfh.bfOffBits);

	DWORD dwDibSize = dwFileSize - dwFileHeaderSize;
	if (dwDibSize < sizeof(BITMAPCOREHEADER))
	{
		OutputTextFromID(hwndEdit, IDS_CORRUPTED);
		return FALSE;
	}

	HANDLE hDib = GlobalAlloc(GHND, dwDibSize);
	if (hDib == NULL)
		return FALSE;

	LPSTR lpbi = (LPSTR)GlobalLock(hDib);
	if (lpbi == NULL)
	{
		DWORD dwError = GetLastError();
		GlobalFree(hDib);
		SetLastError(dwError);
		return FALSE;
	}

	// Read in the entire DIB
	if (!MyReadFile(hFile, lpbi, dwDibSize))
	{
		DWORD dwError = GetLastError();
		GlobalUnlock(hDib);
		GlobalFree(hDib);
		SetLastError(dwError);
		return FALSE;
	}

	GlobalUnlock(hDib);

	// Calculate the offset from the start of the DIB to the bitmap bits
	DWORD dwOffBits = 0;
	if (bfh.bfOffBits > dwFileHeaderSize)
		dwOffBits = bfh.bfOffBits - dwFileHeaderSize;

	if (!ParseDIBitmap(hDlg, hDib, dwOffBits))
	{
		DWORD dwError = GetLastError();
		GlobalFree(hDib);
		if (dwError != ERROR_SUCCESS)
			SetLastError(dwError);
		return FALSE;
	}

	ReplaceThumbnail(hwndThumb, hDib);

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL ParseDIBitmap(HWND hDlg, HANDLE hDib, DWORD dwOffBits)
{
	if (hDlg == NULL || hDib == NULL)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
	if (hwndEdit == NULL)
		return FALSE;

	HWND hwndThumb = GetDlgItem(hDlg, IDC_THUMB);
	if (hwndThumb == NULL)
		return FALSE;

	DWORD dwDibSize = (DWORD)GlobalSize(hDib);
	if (dwDibSize == 0)
		return FALSE;

	LPSTR lpbi = (LPSTR)GlobalLock(hDib);
	if (lpbi == NULL)
		return FALSE;

	DWORD dwDibHeaderSize = *(LPDWORD)lpbi;
	LPBITMAPV5HEADER lpbih = (LPBITMAPV5HEADER)lpbi;

	OutputText(hwndEdit, g_szSepThin);
	OutputTextFmt(hwndEdit, TEXT("Size:\t\t%u bytes\r\n"), dwDibHeaderSize);

	if (dwDibHeaderSize > dwDibSize)
	{
		GlobalUnlock(hDib);
		OutputTextFromID(hwndEdit, IDS_CORRUPTED);
		return FALSE;
	}

	// Verify that the size of the header matches a supported header size
	if (dwDibHeaderSize != sizeof(BITMAPCOREHEADER) &&
		dwDibHeaderSize != sizeof(BITMAPINFOHEADER) &&
		dwDibHeaderSize != sizeof(BITMAPV2INFOHEADER) &&
		dwDibHeaderSize != sizeof(BITMAPV3INFOHEADER) &&
		dwDibHeaderSize != sizeof(BITMAPINFOHEADER2) &&
		dwDibHeaderSize != sizeof(BITMAPV4HEADER) &&
		dwDibHeaderSize != sizeof(BITMAPV5HEADER))
	{
		// Can be a EXBMINFOHEADER DIB with biSize > 40
		// or a BITMAPINFOHEADER2 DIB with 15 < cbFix < 64
		GlobalUnlock(hDib);

		if (dwDibHeaderSize < 16)
		{
			OutputTextFromID(hwndEdit, IDS_CORRUPTED);
			return FALSE;
		}

		OutputTextFromID(hwndEdit, IDS_HEADERSIZE);
		SetThumbnailText(hwndThumb, IDS_UNSUPPORTED);

		return FALSE;
	}

	// Since we don't perform format conversions here, all formats that the
	// display driver cannot directly display are marked as not displayable
	BOOL bIsDibDisplayable = TRUE;
	// Mark formats that cannot be displayed by a display driver but may be
	// supported by a printer driver
	BOOL bIsPassthroughImage = FALSE;

	if (dwDibHeaderSize == sizeof(BITMAPCOREHEADER))
	{ // OS/2 Version 1.1 Bitmap (DIBv2)
		LPBITMAPCOREHEADER lpbch = (LPBITMAPCOREHEADER)lpbi;

		OutputTextFmt(hwndEdit, TEXT("Width:\t\t%u pixels\r\n"), lpbch->bcWidth);
		OutputTextFmt(hwndEdit, TEXT("Height:\t\t%u pixels\r\n"), lpbch->bcHeight);
		OutputTextFmt(hwndEdit, TEXT("Planes:\t\t%u\r\n"), lpbch->bcPlanes);
		OutputTextFmt(hwndEdit, TEXT("BitCount:\t%u bpp\r\n"), lpbch->bcBitCount);
		
		// Perform some sanity checks
		UINT64 ullBitsSize = WIDTHBYTES((UINT64)lpbch->bcWidth * lpbch->bcPlanes * lpbch->bcBitCount) * lpbch->bcHeight;
		if (ullBitsSize == 0 || ullBitsSize > 0x80000000 || lpbch->bcBitCount > 32)
			bIsDibDisplayable = FALSE;
	}

	if (dwDibHeaderSize >= sizeof(BITMAPINFOHEADER))
	{ // Windows Version 3.0 Bitmap (DIBv3)
		OutputTextFmt(hwndEdit, TEXT("Width:\t\t%d pixels\r\n"), lpbih->bV5Width);
		OutputTextFmt(hwndEdit, TEXT("Height:\t\t%d pixels\r\n"), lpbih->bV5Height);
		OutputTextFmt(hwndEdit, TEXT("Planes:\t\t%u\r\n"), lpbih->bV5Planes);
		OutputTextFmt(hwndEdit, TEXT("BitCount:\t%u bpp\r\n"), lpbih->bV5BitCount);

		// Use the QUERYDIBSUPPORT escape function to determine whether the display
		// driver can draw this DIB. The Escape doesn't support core DIBs.
		bIsDibDisplayable = IsDibSupported(lpbi);
		// Perform some additional sanity checks
		if (lpbih->bV5Width < 0)
			lpbih->bV5Width = -lpbih->bV5Width;
		// We don't fix biPlanes to 1 because GDI still takes this value into account
		UINT64 ullBitsSize = WIDTHBYTES((UINT64)lpbih->bV5Width * lpbih->bV5Planes * lpbih->bV5BitCount) * abs(lpbih->bV5Height);
		// biBitCount is 0 for passthrough bitmaps (BI_JPEG, BI_PNG)
		if ((lpbih->bV5BitCount && ullBitsSize == 0) || ullBitsSize > 0x80000000L)
			bIsDibDisplayable = FALSE;

#if defined(_WIN32_WCE) && (_WIN32_WCE >= 0x501)
		lpbih->bV5Compression &= ~BI_SRCPREROTATE;
#endif
		DWORD dwCompression = lpbih->bV5Compression;
		OutputText(hwndEdit, TEXT("Compression:\t"));
		if (isprint(dwCompression & 0xff) &&
			isprint((dwCompression >> 8) & 0xff) &&
			isprint((dwCompression >> 16) & 0xff) &&
			isprint((dwCompression >> 24) & 0xff))
		{ // biCompression contains a FourCC code
			// Not supported by GDI, but may be rendered by VfW DrawDibDraw
			bIsDibDisplayable = TRUE;
			OutputTextFmt(hwndEdit, TEXT("%hc%hc%hc%hc"),
				(char)(dwCompression & 0xff),
				(char)((dwCompression >> 8) & 0xff),
				(char)((dwCompression >> 16) & 0xff),
				(char)((dwCompression >> 24) & 0xff));
		}
		else if (dwDibHeaderSize == sizeof(BITMAPINFOHEADER2))
		{ // OS/2 2.0 bitmap
			switch (dwCompression)
			{
				case BCA_UNCOMP:
					OutputText(hwndEdit, TEXT("UNCOMP"));
					break;
				case BCA_RLE8:
					OutputText(hwndEdit, TEXT("RLE8"));
					break;
				case BCA_RLE4:
					OutputText(hwndEdit, TEXT("RLE4"));
					break;
				case BCA_HUFFMAN1D:
					OutputText(hwndEdit, TEXT("HUFFMAN1D"));
					break;
				case BCA_RLE24:
					OutputText(hwndEdit, TEXT("RLE24"));
					break;
				default:
					OutputTextFmt(hwndEdit, TEXT("%u"), dwCompression);
			}
		}
		else
		{
			switch (dwCompression)
			{
				case BI_RGB:
					OutputText(hwndEdit, TEXT("RGB"));
					break;
				case BI_RLE8:
					OutputText(hwndEdit, TEXT("RLE8"));
					break;
				case BI_RLE4:
					OutputText(hwndEdit, TEXT("RLE4"));
					break;
				case BI_BITFIELDS:
					OutputText(hwndEdit, TEXT("BITFIELDS"));
					break;
				case BI_JPEG:
					bIsPassthroughImage = TRUE;
					OutputText(hwndEdit, TEXT("JPEG"));
					break;
				case BI_PNG:
					bIsPassthroughImage = TRUE;
					OutputText(hwndEdit, TEXT("PNG"));
					break;
				case BI_ALPHABITFIELDS:
					OutputText(hwndEdit, TEXT("ALPHABITFIELDS"));
					break;
				case BI_FOURCC:
					OutputText(hwndEdit, TEXT("FOURCC"));
					break;
				case BI_CMYK:
					OutputText(hwndEdit, TEXT("CMYK"));
					break;
				case BI_CMYKRLE8:
					OutputText(hwndEdit, TEXT("CMYKRLE8"));
					break;
				case BI_CMYKRLE4:
					OutputText(hwndEdit, TEXT("CMYKRLE4"));
					break;
				default:
					OutputTextFmt(hwndEdit, TEXT("%u"), dwCompression);
			}
		}
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("SizeImage:\t%u bytes"), lpbih->bV5SizeImage);
		// For uncompressed bitmaps, check whether the value of biSizeImage matches
		// the calculated size (e.g. Adobe Photoshop adds two padding bytes)
		if (lpbih->bV5SizeImage && ullBitsSize && ullBitsSize <= 0xFFFFFFFF &&
			((UINT64)lpbih->bV5SizeImage - ullBitsSize) != 0 && !DibIsCompressed(lpbi))
		{
			OutputTextFmt(hwndEdit, TEXT(" (%+lld bytes)"), (UINT64)lpbih->bV5SizeImage - ullBitsSize);
			// Adjust biSizeImage to the calculated value
			lpbih->bV5SizeImage = (DWORD)ullBitsSize;
		}
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("XPelsPerMeter:\t%d"), lpbih->bV5XPelsPerMeter);
		if (lpbih->bV5XPelsPerMeter >= 20)
			OutputTextFmt(hwndEdit, TEXT(" (%d dpi)"), MulDiv(lpbih->bV5XPelsPerMeter, 127, 5000));
		OutputText(hwndEdit, TEXT("\r\n"));
			
		OutputTextFmt(hwndEdit, TEXT("YPelsPerMeter:\t%d"), lpbih->bV5YPelsPerMeter);
		if (lpbih->bV5YPelsPerMeter >= 20)
			OutputTextFmt(hwndEdit, TEXT(" (%d dpi)"), MulDiv(lpbih->bV5YPelsPerMeter, 127, 5000));
		OutputText(hwndEdit, TEXT("\r\n"));
			
		OutputTextFmt(hwndEdit, TEXT("ClrUsed:\t%u\r\n"), lpbih->bV5ClrUsed);
		OutputTextFmt(hwndEdit, TEXT("ClrImportant:\t%u\r\n"), lpbih->bV5ClrImportant);
	}

	// The size of the OS/2 2.0 header can be reduced from its full size of
	// 64 bytes down to 16 bytes. Omitted members are assumed to have a value
	// of zero. However, we only support bitmaps with full header here.
	if (dwDibHeaderSize == sizeof(BITMAPINFOHEADER2))
	{ // OS/2 Version 2.0 Bitmap
		LPBITMAPINFOHEADER2 lpbih2 = (LPBITMAPINFOHEADER2)lpbi;

		// Extensions to BITMAPINFOHEADER
		OutputText(hwndEdit, TEXT("Units:\t\t"));
		if (lpbih2->usUnits == BRU_METRIC)
			OutputText(hwndEdit, TEXT("METRIC"));
		else
			OutputTextFmt(hwndEdit, TEXT("%u"), lpbih2->usUnits);
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("Reserved:\t%u\r\n"), lpbih2->usReserved);

		OutputText(hwndEdit, TEXT("Recording:\t"));
		if (lpbih2->usRecording == BRA_BOTTOMUP)
			OutputText(hwndEdit, TEXT("BOTTOMUP"));
		else
			OutputTextFmt(hwndEdit, TEXT("%u"), lpbih2->usRecording);
		OutputText(hwndEdit, TEXT("\r\n"));

		USHORT usRendering = lpbih2->usRendering;
		OutputText(hwndEdit, TEXT("Rendering:\t"));
		switch (usRendering)
		{
			case BRH_NOTHALFTONED:
				OutputText(hwndEdit, TEXT("NOTHALFTONED"));
				break;
			case BRH_ERRORDIFFUSION:
				OutputText(hwndEdit, TEXT("ERRORDIFFUSION"));
				break;
			case BRH_PANDA:
				OutputText(hwndEdit, TEXT("PANDA"));
				break;
			case BRH_SUPERCIRCLE:
				OutputText(hwndEdit, TEXT("SUPERCIRCLE"));
				break;
			default:
				OutputTextFmt(hwndEdit, TEXT("%u"), usRendering);
		}
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("Size1:\t\t%u"), lpbih2->cSize1);
		if (usRendering == BRH_ERRORDIFFUSION && lpbih2->cSize1 <= 100)
			OutputText(hwndEdit, TEXT("%"));
		else if (usRendering == BRH_PANDA || usRendering == BRH_SUPERCIRCLE)
			OutputText(hwndEdit, TEXT(" pels"));
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("Size2:\t\t%u"), lpbih2->cSize2);
		if (usRendering == BRH_PANDA || usRendering == BRH_SUPERCIRCLE)
			OutputText(hwndEdit, TEXT(" pels"));
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputText(hwndEdit, TEXT("ColorEncoding:\t"));
		if (lpbih2->ulColorEncoding == BCE_RGB)
			OutputText(hwndEdit, TEXT("RGB"));
		else if (lpbih2->ulColorEncoding == BCE_PALETTE)
			OutputText(hwndEdit, TEXT("PALETTE"));
		else
			OutputTextFmt(hwndEdit, TEXT("%u"), lpbih2->ulColorEncoding);
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("Identifier:\t%u\r\n"), lpbih2->ulIdentifier);
	}

	if (dwDibHeaderSize == sizeof(BITMAPINFOHEADER))
	{ // Windows Version 3.0 Bitmap with Windows NT extension
		if (lpbih->bV5Compression == BI_BITFIELDS || lpbih->bV5Compression == BI_ALPHABITFIELDS)
		{
			if ((dwDibHeaderSize + ColorMasksSize(lpbi)) > dwDibSize)
			{
				GlobalUnlock(hDib);
				OutputTextFromID(hwndEdit, IDS_CORRUPTED);
				return FALSE;
			}

			LPDWORD lpdwMasks = (LPDWORD)(lpbi + sizeof(BITMAPINFOHEADER));

			OutputText(hwndEdit, g_szSepThin);
			OutputTextFmt(hwndEdit, TEXT("RedMask:\t%08X\r\n"), lpdwMasks[0]);
			OutputTextFmt(hwndEdit, TEXT("GreenMask:\t%08X\r\n"), lpdwMasks[1]);
			OutputTextFmt(hwndEdit, TEXT("BlueMask:\t%08X\r\n"), lpdwMasks[2]);

			// Windows CE extension
			if (lpbih->bV5Compression == BI_ALPHABITFIELDS)
				OutputTextFmt(hwndEdit, TEXT("AlphaMask:\t%08X\r\n"), lpdwMasks[3]);
		}
	}

	if (dwDibHeaderSize != sizeof(BITMAPINFOHEADER2))
	{
		if (dwDibHeaderSize >= sizeof(BITMAPV2INFOHEADER))
		{ // Adobe Photoshop extension
			OutputTextFmt(hwndEdit, TEXT("RedMask:\t%08X\r\n"), lpbih->bV5RedMask);
			OutputTextFmt(hwndEdit, TEXT("GreenMask:\t%08X\r\n"), lpbih->bV5GreenMask);
			OutputTextFmt(hwndEdit, TEXT("BlueMask:\t%08X\r\n"), lpbih->bV5BlueMask);
		}

		if (dwDibHeaderSize >= sizeof(BITMAPV3INFOHEADER))
		{ // Adobe Photoshop extension
			OutputTextFmt(hwndEdit, TEXT("AlphaMask:\t%08X\r\n"), lpbih->bV5AlphaMask);
		}
	}

	if (dwDibHeaderSize >= sizeof(BITMAPV4HEADER))
	{ // Win32 Version 4.0 Bitmap (DIBv4, ICM 1.0)
		DWORD dwCSType = lpbih->bV5CSType;
		OutputText(hwndEdit, TEXT("CSType:\t\t"));
		if (isprint(dwCSType & 0xff) &&
			isprint((dwCSType >>  8) & 0xff) &&
			isprint((dwCSType >> 16) & 0xff) &&
			isprint((dwCSType >> 24) & 0xff))
		{
			OutputTextFmt(hwndEdit, TEXT("%hc%hc%hc%hc"),
				(char)((dwCSType >> 24) & 0xff),
				(char)((dwCSType >> 16) & 0xff),
				(char)((dwCSType >>  8) & 0xff),
				(char)(dwCSType & 0xff));
		}
		else
		{
			switch (dwCSType)
			{
				case LCS_CALIBRATED_RGB:
					OutputText(hwndEdit, TEXT("CALIBRATED_RGB"));
					break;
				case LCS_DEVICE_RGB:
					OutputText(hwndEdit, TEXT("DEVICE_RGB"));
					break;
				case LCS_DEVICE_CMYK:
					OutputText(hwndEdit, TEXT("DEVICE_CMYK"));
					break;
				default:
					OutputTextFmt(hwndEdit, TEXT("%u"), dwCSType);
			}
		}
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("CIEXYZRed:\t%.4f, %.4f, %.4f\r\n"),
			(double)lpbih->bV5Endpoints.ciexyzRed.ciexyzX / 0x40000000,
			(double)lpbih->bV5Endpoints.ciexyzRed.ciexyzY / 0x40000000,
			(double)lpbih->bV5Endpoints.ciexyzRed.ciexyzZ / 0x40000000);
		OutputTextFmt(hwndEdit, TEXT("CIEXYZGreen:\t%.4f, %.4f, %.4f\r\n"),
			(double)lpbih->bV5Endpoints.ciexyzGreen.ciexyzX / 0x40000000,
			(double)lpbih->bV5Endpoints.ciexyzGreen.ciexyzY / 0x40000000,
			(double)lpbih->bV5Endpoints.ciexyzGreen.ciexyzZ / 0x40000000);
		OutputTextFmt(hwndEdit, TEXT("CIEXYZBlue:\t%.4f, %.4f, %.4f\r\n"),
			(double)lpbih->bV5Endpoints.ciexyzBlue.ciexyzX / 0x40000000,
			(double)lpbih->bV5Endpoints.ciexyzBlue.ciexyzY / 0x40000000,
			(double)lpbih->bV5Endpoints.ciexyzBlue.ciexyzZ / 0x40000000);

		OutputTextFmt(hwndEdit, TEXT("GammaRed:\t%g\r\n"), (double)lpbih->bV5GammaRed / 0x10000);
		OutputTextFmt(hwndEdit, TEXT("GammaGreen:\t%g\r\n"), (double)lpbih->bV5GammaGreen / 0x10000);
		OutputTextFmt(hwndEdit, TEXT("GammaBlue:\t%g\r\n"), (double)lpbih->bV5GammaBlue / 0x10000);
	}

	if (dwDibHeaderSize >= sizeof(BITMAPV5HEADER))
	{ // Win32 Version 5.0 Bitmap (DIBv5, ICM 2.0)
		DWORD dwIntent = lpbih->bV5Intent;
		OutputText(hwndEdit, TEXT("Intent:\t\t"));
		switch (dwIntent)
		{
			case LCS_GM_BUSINESS:
				OutputText(hwndEdit, TEXT("GM_BUSINESS (Saturation)"));
				break;
			case LCS_GM_GRAPHICS:
				OutputText(hwndEdit, TEXT("GM_GRAPHICS (Relative Colorimetric)"));
				break;
			case LCS_GM_IMAGES:
				OutputText(hwndEdit, TEXT("GM_IMAGES (Perceptual)"));
				break;
			case LCS_GM_ABS_COLORIMETRIC:
				OutputText(hwndEdit, TEXT("GM_ABS_COLORIMETRIC (Absolute Colorimetric)"));
				break;
			default:
				OutputTextFmt(hwndEdit, TEXT("%u"), dwIntent);
		}
		OutputText(hwndEdit, TEXT("\r\n"));

		OutputTextFmt(hwndEdit, TEXT("ProfileData:\t%u bytes\r\n"), lpbih->bV5ProfileData);
		OutputTextFmt(hwndEdit, TEXT("ProfileSize:\t%u bytes\r\n"), lpbih->bV5ProfileSize);
		OutputTextFmt(hwndEdit, TEXT("Reserved:\t%u\r\n"), lpbih->bV5Reserved);
	}

	// Output the color table entries
	UINT uNumColors = min(DibNumColors(lpbi), 4096);
	if (uNumColors > 0)
	{
		if (DibBitsOffset(lpbi) > dwDibSize)
		{
			GlobalUnlock(hDib);
			OutputTextFromID(hwndEdit, IDS_CORRUPTED);
			return FALSE;
		}

		OutputText(hwndEdit, g_szSepThin);
		LPCTSTR lpszColorName = TEXT("");
		int nWidthDec = uNumColors > 1000 ? 4 : (uNumColors > 100 ? 3 : (uNumColors > 10 ? 2 : 1));
		int nWidthHex = uNumColors > 0x100 ? 3 : (uNumColors > 0x10 ? 2 : 1);

		if (IS_OS2PM_DIB(lpbi))
		{
			OutputTextFmt(hwndEdit, TEXT("%*c|   B   G   R |%-*c| B  G  R  |\r\n"), nWidthDec, 'I', nWidthHex, 'I');

			LPBITMAPCOREINFO lpbmc = (LPBITMAPCOREINFO)lpbi;
			for (UINT i = 0; i < uNumColors; i++)
			{
				OutputTextFmt(hwndEdit, TEXT("%*u| %3u %3u %3u |%0*X| %02X %02X %02X |"),
					nWidthDec, i,
					lpbmc->bmciColors[i].rgbtBlue,
					lpbmc->bmciColors[i].rgbtGreen,
					lpbmc->bmciColors[i].rgbtRed,
					nWidthHex, i,
					lpbmc->bmciColors[i].rgbtBlue,
					lpbmc->bmciColors[i].rgbtGreen,
					lpbmc->bmciColors[i].rgbtRed);

				if (GetColorName(RGB(
					lpbmc->bmciColors[i].rgbtRed,
					lpbmc->bmciColors[i].rgbtGreen,
					lpbmc->bmciColors[i].rgbtBlue),
					&lpszColorName))
					OutputTextFmt(hwndEdit, TEXT(" %s"), lpszColorName);

				OutputText(hwndEdit, TEXT("\r\n"));
			}
		}
		else
		{
			OutputTextFmt(hwndEdit, TEXT("%*c|   B   G   R   X |%-*c| B  G  R  X  |\r\n"), nWidthDec, 'I', nWidthHex, 'I');

			LPRGBQUAD lprgbqColors = (LPRGBQUAD)FindDibPalette(lpbi);
			for (UINT i = 0; i < uNumColors; i++)
			{
				OutputTextFmt(hwndEdit, TEXT("%*u| %3u %3u %3u %3u |%0*X| %02X %02X %02X %02X |"),
					nWidthDec, i,
					lprgbqColors[i].rgbBlue,
					lprgbqColors[i].rgbGreen,
					lprgbqColors[i].rgbRed,
					lprgbqColors[i].rgbReserved,
					nWidthHex, i,
					lprgbqColors[i].rgbBlue,
					lprgbqColors[i].rgbGreen,
					lprgbqColors[i].rgbRed,
					lprgbqColors[i].rgbReserved,
					lpszColorName);

				if (GetColorName(RGB(
					lprgbqColors[i].rgbRed,
					lprgbqColors[i].rgbGreen,
					lprgbqColors[i].rgbBlue),
					&lpszColorName))
					OutputTextFmt(hwndEdit, TEXT(" %s"), lpszColorName);

				OutputText(hwndEdit, TEXT("\r\n"));
			}
		}
	}

	// Check for a gap between color table and bitmap bits
	DWORD dwOffBitsPacked = DibBitsOffset(lpbi);

	if ((dwOffBits != 0 ? dwOffBits : dwOffBitsPacked) > dwDibSize)
	{
		GlobalUnlock(hDib);
		OutputTextFromID(hwndEdit, IDS_CORRUPTED);
		return FALSE;
	}

	if (dwOffBits > dwOffBitsPacked)
	{ // Gap between color table and bitmap bits present
		DWORD dwGap = dwOffBits - dwOffBitsPacked;

		OutputText(hwndEdit, g_szSepThin);
		OutputTextFmt(hwndEdit, TEXT("Gap to pixels:\t%u bytes\r\n"), dwGap);

		// Remove the gap to obtain a packed DIB
		dwDibSize -= dwGap;

		LPSTR lpOld = lpbi + dwOffBits;
		LPSTR lpNew = lpbi + dwOffBitsPacked;

		__try
		{
			dwOffBits = dwOffBitsPacked;
			MoveMemory(lpNew, lpOld, dwDibSize - dwOffBits);
			ZeroMemory(lpbi + dwDibSize, dwGap);
		}
		__except (EXCEPTION_EXECUTE_HANDLER) { ; }

		GlobalUnlock(hDib);
		HANDLE hTemp = GlobalReAlloc(hDib, dwDibSize, GMEM_MOVEABLE);
		if (hTemp != NULL)
			hDib = hTemp;

		lpbi = (LPSTR)GlobalLock(hDib);
		lpbih = (LPBITMAPV5HEADER)lpbi;
		if (lpbi == NULL)
			return FALSE;
	}
	else if (dwOffBits != 0 && dwOffBitsPacked > dwOffBits)
	{ // Color table overlaps bitmap bits
		DWORD dwOverlap = dwOffBitsPacked - dwOffBits;

		OutputText(hwndEdit, g_szSepThin);
		OutputTextFmt(hwndEdit, TEXT("Gap to pixels:\t-%u bytes\r\n"), dwOverlap);

		DWORD dwNumEntries = DibNumColors(lpbi);
		if (dwNumEntries > 0)
		{
			SIZE_T cbEntrySize = IS_OS2PM_DIB(lpbi) ? sizeof(RGBTRIPLE) : sizeof(RGBQUAD);
			DWORD dwOverlappedEntries = dwOverlap / (DWORD)cbEntrySize;

			if (dwDibHeaderSize >= sizeof(BITMAPINFOHEADER) && dwNumEntries > dwOverlappedEntries)
			{ // Adjust the number of color table entries
				lpbih->bV5ClrUsed = dwNumEntries - dwOverlappedEntries;
				if (lpbih->bV5ClrImportant > lpbih->bV5ClrUsed)
					lpbih->bV5ClrImportant = lpbih->bV5ClrUsed;
			}
			else
			{ // Add the missing color table entries to the DIB
				dwDibSize += dwOverlap;

				GlobalUnlock(hDib);
				HANDLE hTemp = GlobalReAlloc(hDib, dwDibSize, GHND);
				if (hTemp == NULL)
					return FALSE;

				hDib = hTemp;
				lpbi = (LPSTR)GlobalLock(hDib);
				lpbih = (LPBITMAPV5HEADER)lpbi;
				if (lpbi == NULL)
					return FALSE;

				LPSTR lpOld = lpbi + dwOffBits;
				LPSTR lpNew = lpbi + dwOffBitsPacked;

				__try
				{
					dwOffBits = dwOffBitsPacked;
					MoveMemory(lpNew, lpOld, dwDibSize - dwOffBits);
					ZeroMemory(lpOld, dwOverlap);
					// Create a grayscale palette
					for (UINT i = 0; i < dwOverlappedEntries; i++, lpOld += cbEntrySize)
						lpOld[0] = lpOld[1] = lpOld[2] = (BYTE)(i * 256 / dwOverlappedEntries);
				}
				__except (EXCEPTION_EXECUTE_HANDLER) { ; }
			}
		}
	}

	// Check whether the bitmap bits are cropped
	if (((dwOffBits != 0 ? dwOffBits : dwOffBitsPacked) + DibImageSize(lpbi)) > dwDibSize)
	{
		GlobalUnlock(hDib);
		OutputTextFromID(hwndEdit, IDS_CORRUPTED);
		return FALSE;
	}

	// Output the ICC profile data
	if (DibHasColorProfile(lpbi))
	{
		TCHAR szOutput[OUTPUT_LEN];

		if ((lpbih->bV5ProfileData + lpbih->bV5ProfileSize) > dwDibSize)
		{
			GlobalUnlock(hDib);
			OutputTextFromID(hwndEdit, IDS_CORRUPTED);
			return FALSE;
		}

		// Check for a gap between bitmap bits and profile data
		DWORD dwProfileData = DibBitsOffset(lpbi) + DibImageSize(lpbi);
		if (lpbih->bV5ProfileData > dwProfileData)
		{
			OutputText(hwndEdit, g_szSepThin);
			OutputTextFmt(hwndEdit, TEXT("Gap to profile:\t%u bytes\r\n"), lpbih->bV5ProfileData - dwProfileData);
		}

		if (lpbih->bV5CSType == PROFILE_LINKED)
		{
			char szPath[MAX_PATH];
			int nLen = min(min(lpbih->bV5ProfileSize + 1, dwDibSize - lpbih->bV5ProfileData + 1), _countof(szPath));

			// Output the file name of the ICC profile
			ZeroMemory(szPath, sizeof(szPath));
			if (MyStrNCpyA(szPath, lpbi + lpbih->bV5ProfileData, nLen) != NULL)
			{
				MultiByteToWideChar(1252, 0, szPath, -1, szOutput, _countof(szOutput) - 1);
				
				OutputText(hwndEdit, g_szSepThin);
				OutputText(hwndEdit, szOutput);
				OutputText(hwndEdit, TEXT("\r\n"));
			}
		}
		else if (lpbih->bV5CSType == PROFILE_EMBEDDED)
		{
			if (lpbih->bV5ProfileSize < sizeof(PROFILEV5HEADER))
			{
				GlobalUnlock(hDib);
				OutputTextFromID(hwndEdit, IDS_CORRUPTED);
				return FALSE;
			}

			// Output the ICC profile header
			LPPROFILEV5HEADER lpph = (LPPROFILEV5HEADER)(lpbi + lpbih->bV5ProfileData);
			DWORD dwProfileSize = _byteswap_ulong(lpph->phSize);
			DWORD dwVersion = _byteswap_ulong(lpph->phVersion);

			if (_byteswap_ulong(lpph->phSignature) != 'acsp' && dwVersion != 0x00000100)
				goto Exit;

			OutputText(hwndEdit, g_szSepThin);
			OutputTextFmt(hwndEdit, TEXT("ProfileSize:\t%u bytes\r\n"), dwProfileSize);

			if (dwProfileSize != lpbih->bV5ProfileSize)
			{
				GlobalUnlock(hDib);
				OutputTextFromID(hwndEdit, IDS_CORRUPTED);
				return FALSE;
			}

			PrintProfileSignature(hwndEdit, TEXT("CMMType:\t"), lpph->phCMMType);

			if (dwVersion == 0x00000100)
			{ // Apple ColorSync 1.0 profile
				OutputText(hwndEdit, TEXT("Version:\t1.0\r\n"));
				goto Exit; // No more identical header members
			}

			WORD wProfileVersion  = HIWORD(dwVersion);
			WORD wSubClassVersion = LOWORD(dwVersion);
			OutputTextFmt(hwndEdit, TEXT("Version:\t%u.%u.%u\r\n"), HIBYTE(wProfileVersion),
				(LOBYTE(wProfileVersion) >> 4) & 0xF, LOBYTE(wProfileVersion) & 0xF);
			if (wProfileVersion >= 0x0500 && lpph->phSubClass != 0)
				OutputTextFmt(hwndEdit, TEXT("SubVersion:\t%u.%u\r\n"),
					HIBYTE(wSubClassVersion), LOBYTE(wSubClassVersion));

			PrintProfileSignature(hwndEdit, TEXT("Class:\t\t"), lpph->phClass);
			PrintProfileSignature(hwndEdit, TEXT("ColorSpace:\t"), lpph->phDataColorSpace);
			PrintProfileSignature(hwndEdit, TEXT("PCS:\t\t"), lpph->phConnectionSpace);

			OutputTextFmt(hwndEdit, TEXT("DateTime:\t%02u-%02u-%02u %02u:%02u:%02u\r\n"),
				_byteswap_ushort(LOWORD(lpph->phDateTime[0])),
				_byteswap_ushort(HIWORD(lpph->phDateTime[0])),
				_byteswap_ushort(LOWORD(lpph->phDateTime[1])),
				_byteswap_ushort(HIWORD(lpph->phDateTime[1])),
				_byteswap_ushort(LOWORD(lpph->phDateTime[2])),
				_byteswap_ushort(HIWORD(lpph->phDateTime[2])));

			PrintProfileSignature(hwndEdit, TEXT("Signature:\t"), lpph->phSignature);
			PrintProfileSignature(hwndEdit, TEXT("Platform:\t"), lpph->phPlatform);

			DWORD dwProfileFlags = _byteswap_ulong(lpph->phProfileFlags);
			OutputTextFmt(hwndEdit, TEXT("ProfileFlags:\t%08X"), dwProfileFlags);
			if (dwProfileFlags != 0)
			{
				szOutput[0] = TEXT('\0');
				DWORD dwFlags = dwProfileFlags;

				if (dwProfileFlags & FLAG_EMBEDDEDPROFILE)
				{
					dwFlags ^= FLAG_EMBEDDEDPROFILE;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("EMBEDDEDPROFILE"));
				}

				if (dwProfileFlags & FLAG_DEPENDENTONDATA)
				{
					dwFlags ^= FLAG_DEPENDENTONDATA;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("DEPENDENTONDATA"));
				}

				if (dwProfileFlags & FLAG_MCSNEEDSSUBSET)
				{
					dwFlags ^= FLAG_MCSNEEDSSUBSET;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("MCSNEEDSSUBSET"));
				}

				if (dwProfileFlags & FLAG_EXTENDEDRANGEPCS)
				{
					dwFlags ^= FLAG_EXTENDEDRANGEPCS;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("EXTENDEDRANGEPCS"));
				}

				if (dwFlags != 0 && dwFlags != dwProfileFlags)
				{
					const int FLAGS_LEN = 32;
					TCHAR szFlags[FLAGS_LEN];
					_sntprintf(szFlags, _countof(szFlags), TEXT("%08X"), dwFlags);
					szFlags[FLAGS_LEN - 1] = TEXT('\0');
					AppendFlagName(szOutput, _countof(szOutput), szFlags);
				}

				if (szOutput[0] != TEXT('\0'))
				{
					OutputText(hwndEdit, TEXT(" ("));
					OutputText(hwndEdit, szOutput);
					OutputText(hwndEdit, TEXT(")"));
				}
			}
			OutputText(hwndEdit, TEXT("\r\n"));

			PrintProfileSignature(hwndEdit, TEXT("Manufacturer:\t"), lpph->phManufacturer);
			PrintProfileSignature(hwndEdit, TEXT("Model:\t\t"), lpph->phModel);

			UINT64 ullAttributes = _byteswap_ulong(lpph->phAttributes[0]);
			ullAttributes |= (UINT64)_byteswap_ulong(lpph->phAttributes[1]) << 32;
			OutputTextFmt(hwndEdit, TEXT("Attributes:\t%016llX"), ullAttributes);
			if (ullAttributes != 0)
			{
				szOutput[0] = TEXT('\0');
				UINT64 ullFlags = ullAttributes;

				if (ullAttributes & ATTRIB_TRANSPARENCY)
				{
					ullFlags ^= ATTRIB_TRANSPARENCY;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("TRANSPARENCY"));
				}

				if (ullAttributes & ATTRIB_MATTE)
				{
					ullFlags ^= ATTRIB_MATTE;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("MATTE"));
				}

				if (ullAttributes & ATTRIB_MEDIANEGATIVE)
				{
					ullFlags ^= ATTRIB_MEDIANEGATIVE;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("MEDIANEGATIVE"));
				}

				if (ullAttributes & ATTRIB_MEDIABLACKANDWHITE)
				{
					ullFlags ^= ATTRIB_MEDIABLACKANDWHITE;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("MEDIABLACKANDWHITE"));
				}

				if (ullAttributes & ATTRIB_NONPAPERBASED)
				{
					ullFlags ^= ATTRIB_NONPAPERBASED;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("NONPAPERBASED"));
				}

				if (ullAttributes & ATTRIB_TEXTURED)
				{
					ullFlags ^= ATTRIB_TEXTURED;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("TEXTURED"));
				}

				if (ullAttributes & ATTRIB_NONISOTROPIC)
				{
					ullFlags ^= ATTRIB_NONISOTROPIC;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("NONISOTROPIC"));
				}

				if (ullAttributes & ATTRIB_SELFLUMINOUS)
				{
					ullFlags ^= ATTRIB_SELFLUMINOUS;
					AppendFlagName(szOutput, _countof(szOutput), TEXT("SELFLUMINOUS"));
				}

				if (ullFlags != 0 && ullFlags != ullAttributes)
				{
					const int FLAGS_LEN = 32;
					TCHAR szFlags[FLAGS_LEN];
					_sntprintf(szFlags, _countof(szFlags), TEXT("%016llX"), ullFlags);
					szFlags[FLAGS_LEN - 1] = TEXT('\0');
					AppendFlagName(szOutput, _countof(szOutput), szFlags);
				}

				if (szOutput[0] != TEXT('\0'))
				{
					OutputText(hwndEdit, TEXT(" ("));
					OutputText(hwndEdit, szOutput);
					OutputText(hwndEdit, TEXT(")"));
				}
			}
			OutputText(hwndEdit, TEXT("\r\n"));

			DWORD dwhRenderingIntent = _byteswap_ulong(lpph->phRenderingIntent);
			OutputText(hwndEdit, TEXT("Intent:\t\t"));
			switch (dwhRenderingIntent)
			{
				case INTENT_PERCEPTUAL:
					OutputText(hwndEdit, TEXT("PERCEPTUAL"));
					break;
				case INTENT_RELATIVE_COLORIMETRIC:
					OutputText(hwndEdit, TEXT("RELATIVE_COLORIMETRIC"));
					break;
				case INTENT_SATURATION:
					OutputText(hwndEdit, TEXT("SATURATION"));
					break;
				case INTENT_ABSOLUTE_COLORIMETRIC:
					OutputText(hwndEdit, TEXT("ABSOLUTE_COLORIMETRIC"));
					break;
				default:
					OutputTextFmt(hwndEdit, TEXT("%u"), dwhRenderingIntent);
			}
			OutputText(hwndEdit, TEXT("\r\n"));

			OutputTextFmt(hwndEdit, TEXT("Illuminant:\t%.4f, %.4f, %.4f\r\n"),
				(double)(LONG32)_byteswap_ulong(lpph->phIlluminant.ciexyzX) / 0x10000,
				(double)(LONG32)_byteswap_ulong(lpph->phIlluminant.ciexyzY) / 0x10000,
				(double)(LONG32)_byteswap_ulong(lpph->phIlluminant.ciexyzZ) / 0x10000);

			PrintProfileSignature(hwndEdit, TEXT("Creator:\t"), lpph->phCreator);

			if (wProfileVersion >= 0x0400)
			{
				ZeroMemory(szOutput, sizeof(szOutput));
				for (SIZE_T i = 0; i < _countof(lpph->phProfileID); i++)
					_sntprintf(szOutput, _countof(szOutput), TEXT("%s%02X"), (LPTSTR)szOutput, lpph->phProfileID[i]);

				OutputText(hwndEdit, TEXT("ProfileID:\t"));
				OutputText(hwndEdit, szOutput);
				OutputText(hwndEdit, TEXT("\r\n"));
			}

			if (wProfileVersion >= 0x0500)
			{
				PrintProfileSignature(hwndEdit, TEXT("SpectralPCS:\t"), lpph->phSpectralPCS);

				OutputTextFmt(hwndEdit, TEXT("SpectralRange:\t%g nm - %g nm, %u steps\r\n"),
					Half2Float(_byteswap_ushort(lpph->phSpectralRange[0])),
					Half2Float(_byteswap_ushort(lpph->phSpectralRange[1])),
					_byteswap_ushort(lpph->phSpectralRange[2]));

				OutputTextFmt(hwndEdit, TEXT("BiSpectrRange:\t%g nm - %g nm, %u steps\r\n"),
					Half2Float(_byteswap_ushort(lpph->phBiSpectralRange[0])),
					Half2Float(_byteswap_ushort(lpph->phBiSpectralRange[1])),
					_byteswap_ushort(lpph->phBiSpectralRange[2]));

				PrintProfileSignature(hwndEdit, TEXT("MCS:\t\t"), lpph->phMCS);
				PrintProfileSignature(hwndEdit, TEXT("SubClass:\t"), lpph->phSubClass);
			}

			// Output the tag table
			LPDWORD lpdwTagTable = (LPDWORD)(lpbi + lpbih->bV5ProfileData + sizeof(PROFILEV5HEADER));
			DWORD dwTagCount = _byteswap_ulong(*lpdwTagTable);

			OutputText(hwndEdit, g_szSepThin);
			OutputTextFmt(hwndEdit, TEXT("TagCount:\t%u\r\n"), dwTagCount);

			if (dwTagCount == 0)
				goto Exit;

			DWORD dwSignature = 0;
			DWORD dwElementSize = 0;
			DWORD dwElementOffset = 0;
			LPDWORD lpdwTags = lpdwTagTable + 1;

			OutputText(hwndEdit, g_szSepThin);
			OutputText(hwndEdit, TEXT("Sig. | Element Offset | Element Size |\r\n"));

			while (dwTagCount--)
			{
				dwSignature = *lpdwTags++;
				dwElementOffset = _byteswap_ulong(*lpdwTags++);
				dwElementSize = _byteswap_ulong(*lpdwTags++);

				PrintProfileSignature(hwndEdit, NULL, dwSignature, FALSE);
				OutputTextFmt(hwndEdit, TEXT(" | %8u bytes"), dwElementOffset);
				OutputTextFmt(hwndEdit, TEXT(" | %6u bytes |"), dwElementSize);
				PrintProfileTagData(hwndEdit, TEXT(" "), (LPSTR)lpph + dwElementOffset, dwElementSize);
				OutputText(hwndEdit, TEXT("\r\n"));
			}
		}
	}

Exit:
	GlobalUnlock(hDib);

	if (!bIsDibDisplayable && !bIsPassthroughImage)
	{
		SetThumbnailText(hwndThumb, IDS_UNSUPPORTED);
		return FALSE;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void PrintProfileSignature(HWND hwndEdit, LPCTSTR lpszName, DWORD dwSignature, BOOL bAddCrLf)
{
	if (hwndEdit == NULL)
		return;

	if (lpszName != NULL)
		OutputText(hwndEdit, lpszName);

	if (dwSignature == 0)
		OutputText(hwndEdit, TEXT("0"));
	else
		if (isprint(dwSignature & 0xff) &&
			isprint((dwSignature >>  8) & 0xff) &&
			isprint((dwSignature >> 16) & 0xff) &&
			isprint((dwSignature >> 24) & 0xff))
		{ // Sequence of four characters
			OutputTextFmt(hwndEdit, TEXT("%hc%hc%hc%hc"),
				(char)(dwSignature & 0xff),
				(char)((dwSignature >>  8) & 0xff),
				(char)((dwSignature >> 16) & 0xff),
				(char)((dwSignature >> 24) & 0xff));
		}
		else
			if (isprint(dwSignature & 0xff) &&
				isprint((dwSignature >> 8) & 0xff))
			{ // Extended colour space signature
				OutputTextFmt(hwndEdit, TEXT("%hc%hc%02X%02X"),
					(char)(dwSignature & 0xff),
					(char)((dwSignature >>  8) & 0xff),
					(BYTE)((dwSignature >> 16) & 0xff),
					(BYTE)((dwSignature >> 24) & 0xff));
			}
			else
				OutputTextFmt(hwndEdit, TEXT("%08X"), _byteswap_ulong(dwSignature));

	if (bAddCrLf)
		OutputText(hwndEdit, TEXT("\r\n"));
}

////////////////////////////////////////////////////////////////////////////////////////////////

void PrintProfileTagData(HWND hwndEdit, LPCTSTR lpszName, LPCSTR lpData, DWORD dwSize, BOOL bAddCrLf)
{
	if (hwndEdit == NULL || lpData == NULL || dwSize < 4)
		return;

	// Don't show anything if the total length of the text exceeds
	// OUTPUT_LEN. Otherwise OutputTextFmt would cut off the text.
	SIZE_T cchMaxLen = OUTPUT_LEN - 39;
	if (lpszName != NULL)
		cchMaxLen -= min(cchMaxLen, _tcslen(lpszName));

	// Retrieve the tag type signature
	DWORD dwSignature = _byteswap_ulong(*(LPDWORD)lpData);

	switch (dwSignature)
	{
		case 'sig ': // signatureType
			if (dwSize == 12)
				PrintProfileSignature(hwndEdit, lpszName, *(LPDWORD)(lpData + 8), FALSE);
			break;

		case 'text': // textType
			if (dwSize > 8)
			{
				UINT cbStringLen = dwSize - 8;
				if (cbStringLen > 0 && cbStringLen <= cchMaxLen)
				{
					// From profile v4, the terminating NULL is no longer mandatory
					cbStringLen += sizeof(CHAR);
					LPSTR pszText = (LPSTR)MyGlobalAllocPtr(GHND, cbStringLen);
					if (pszText != NULL)
					{
						MyStrNCpyA(pszText, lpData + 8, cbStringLen);
						// Skip the entire element if the text contains line breaks
						if (strpbrk((PCSTR)pszText, "\r\n") == NULL)
						{
							if (lpszName != NULL)
								OutputText(hwndEdit, lpszName);
							OutputTextFmt(hwndEdit, TEXT("%S"), pszText);
						}
						MyGlobalFreePtr(pszText);
					}
				}
			}
			break;

		case 'desc': // textDescriptionType
			if (dwSize > 12)
			{ // We only support the ASCII structure
				UINT cbStringLen = _byteswap_ulong(*(LPDWORD)(lpData + 8));
				if (cbStringLen > 0 && cbStringLen <= cchMaxLen)
				{
					cbStringLen += sizeof(CHAR);
					LPSTR pszText = (LPSTR)MyGlobalAllocPtr(GHND, cbStringLen);
					if (pszText != NULL)
					{
						MyStrNCpyA(pszText, lpData + 12, cbStringLen);
						if (strpbrk((PCSTR)pszText, "\r\n") == NULL)
						{
							if (lpszName != NULL)
								OutputText(hwndEdit, lpszName);
							OutputTextFmt(hwndEdit, TEXT("%S"), pszText);
						}
						MyGlobalFreePtr(pszText);
					}
				}
			}
			break;

		case 'utf8': // utf8Type
			if (dwSize > 8)
			{
				UINT cbMultiLen = dwSize - 8;
				if (cbMultiLen > 0 && cbMultiLen <= cchMaxLen)
				{
					UINT cchWideLen = MultiByteToWideChar(CP_UTF8, 0, lpData + 8, cbMultiLen, NULL, 0);
					if (cchWideLen > 0)
					{
						LPWSTR pwszWide = (LPWSTR)MyGlobalAllocPtr(GHND, cchWideLen + sizeof(WCHAR));
						if (pwszWide != NULL)
						{
							// Convert the UTF-8 string to Unicode UTF-16
							MultiByteToWideChar(CP_UTF8, 0, lpData + 8, cbMultiLen, pwszWide, cchWideLen);

							if (wcspbrk((PCWSTR)pwszWide, L"\r\n") == NULL)
							{
								if (lpszName != NULL)
									OutputText(hwndEdit, lpszName);
								OutputText(hwndEdit, pwszWide);
							}
							MyGlobalFreePtr(pwszWide);
						}
					}
				}
			}
			break;

		case 'mluc': // multiLocalizedUnicodeType
			if (dwSize > 28 && _byteswap_ulong(*(LPDWORD)(lpData + 8)) > 0)
			{ // We only support the first record
				UINT cbStringLen = _byteswap_ulong(*(LPDWORD)(lpData + 20));
				if (cbStringLen > 0 && (cbStringLen / sizeof(WCHAR)) <= cchMaxLen)
				{
					LPCWSTR pwszSrc = (LPCWSTR)(lpData + _byteswap_ulong(*(LPDWORD)(lpData + 24)));
					LPWSTR pwszDest = (LPWSTR)MyGlobalAllocPtr(GHND, cbStringLen + sizeof(WCHAR));
					if (pwszDest != NULL)
					{
						// Convert the Unicode string from BE to LE
						UINT cchStringLen = cbStringLen / sizeof(WCHAR);
						for (UINT i = 0; i < cchStringLen; i++)
							_sntprintf(pwszDest, cchStringLen, TEXT("%s%c"), pwszDest, _byteswap_ushort(pwszSrc[i]));
						pwszDest[cchStringLen] = L'\0';

						if (wcspbrk((PCWSTR)pwszDest, L"\r\n") == NULL)
						{
							if (lpszName != NULL)
								OutputText(hwndEdit, lpszName);
							OutputText(hwndEdit, pwszDest);
						}
						MyGlobalFreePtr(pwszDest);
					}
				}
			}
			break;

		case 'curv': // curveType
			if (dwSize >= 12)
			{
				DWORD dwCount = _byteswap_ulong(*(LPDWORD)(lpData + 8));
				if (dwCount == 0 || dwCount == 1)
				{ // We only support the gamma value
					if (lpszName != NULL)
						OutputText(hwndEdit, lpszName);

					if (dwCount == 0)
						OutputText(hwndEdit, TEXT("Y = X"));
					else
						OutputTextFmt(hwndEdit, TEXT("Y = X ^ %.4f"),
							(double)(LONG32)_byteswap_ushort(*(LPWORD)(lpData + 12)) / 0x100);
				}
			}
			break;

		case 'XYZ ': // XYZType
			if (dwSize == 20)
			{ // We only support arrays with one set of values
				if (lpszName != NULL)
					OutputText(hwndEdit, lpszName);

				OutputTextFmt(hwndEdit, TEXT("X = %.4f, Y = %.4f, Z = %.4f"),
					(double)(LONG32)_byteswap_ulong(*(LPDWORD)(lpData +  8)) / 0x10000,
					(double)(LONG32)_byteswap_ulong(*(LPDWORD)(lpData + 12)) / 0x10000,
					(double)(LONG32)_byteswap_ulong(*(LPDWORD)(lpData + 16)) / 0x10000);
			}
			break;

		case 'ICCp': // embeddedProfileType
			if (dwSize > 8)
			{
				if (lpszName != NULL)
					OutputText(hwndEdit, lpszName);

				OutputText(hwndEdit, TEXT("[Embedded ICC.2 profile]"));
			}
			break;

		case 'MS10': // WcsProfilesTagType
			if (dwSize > 8)
			{
				if (lpszName != NULL)
					OutputText(hwndEdit, lpszName);

				OutputText(hwndEdit, TEXT("[Embedded WCS profiles]"));
			}
			break;
	}

	if (bAddCrLf)
		OutputText(hwndEdit, TEXT("\r\n"));
}

////////////////////////////////////////////////////////////////////////////////////////////////

BOOL GetColorName(COLORREF rgbColor, LPCTSTR* lpszName)
{
	if (lpszName == NULL)
		return FALSE;

	*lpszName = TEXT("");
	size_t count = sizeof(g_aColorNames) / sizeof(ColorName);

	for (size_t i = 0; i < count; i++)
	{
		if (g_aColorNames[i].rgbColor == rgbColor)
		{
			*lpszName = g_aColorNames[i].lpszName;
			return TRUE;
		}
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

float Half2Float(WORD h)
{
	DWORD f = h ? ((h & 0x8000) << 16) | (((h & 0x7C00) + 0x1C000) << 13) | ((h & 0x03FF) << 13) : 0;

	return *((float*)((void*)&f));
}

////////////////////////////////////////////////////////////////////////////////////////////////
