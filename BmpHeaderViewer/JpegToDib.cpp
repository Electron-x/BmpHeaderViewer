////////////////////////////////////////////////////////////////////////////////////////////////
// JpegToDib.cpp - Copyright (c) 2024 by W. Rolke.
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
// This software includes code from the Independent JPEG Group's JPEG
// software (libjpeg). These parts of the code are subject to their own
// copyright and license terms, which can be found in the libjpeg directory.
//
////////////////////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "..\libjpeg\jpeglib.h"
#include "..\libjpeg\iccprofile.h"

////////////////////////////////////////////////////////////////////////////////////////////////
// Data Types

typedef struct jpeg_decompress_struct j_decompress;
typedef struct jpeg_error_mgr         j_error_mgr;

// JPEG decompression structure (overloads jpeg_decompress_struct)
typedef struct _JPEG_DECOMPRESS
{
	j_decompress    jInfo;          // Decompression structure
	j_error_mgr     jError;         // Error manager
	INT             nTraceLevel;    // Max. message level that will be displayed
	BOOL            bNeedDestroy;   // jInfo must be destroyed
	LPBYTE          lpProfileData;  // Pointer to ICC profile data
} JPEG_DECOMPRESS, *LPJPEG_DECOMPRESS;

// Buffer for processor status
static jmp_buf JmpBuffer;

////////////////////////////////////////////////////////////////////////////////////////////////
// Helper functions

// Performs housekeeping
void cleanup_jpeg_to_dib(LPJPEG_DECOMPRESS lpJpegDecompress, HANDLE hDib);
// Registers the callback functions for the error manager
void set_error_manager(j_common_ptr pjInfo, j_error_mgr* pjError);

////////////////////////////////////////////////////////////////////////////////////////////////
// Callback functions

// Error handling
static void my_error_exit(j_common_ptr pjInfo);
// Text output
static void my_output_message(j_common_ptr pjInfo);
// Message handling
static void my_emit_message(j_common_ptr pjInfo, int nMessageLevel);

////////////////////////////////////////////////////////////////////////////////////////////////

HANDLE JpegToDib(LPVOID lpJpegData, DWORD dwLenData, INT nTraceLevel)
{
	HANDLE hDib = NULL;

	// Initialize the JPEG decompression object
	JPEG_DECOMPRESS JpegDecompress = { {0} };
	j_decompress_ptr pjInfo = &JpegDecompress.jInfo;
	JpegDecompress.lpProfileData = NULL;
	JpegDecompress.nTraceLevel = nTraceLevel;
	set_error_manager((j_common_ptr)pjInfo, &JpegDecompress.jError);

	// Save processor status for error handling
	if (setjmp(JmpBuffer))
	{
		cleanup_jpeg_to_dib(&JpegDecompress, hDib);
		return NULL;
	}

	jpeg_create_decompress(pjInfo);
	JpegDecompress.bNeedDestroy = TRUE;

	// Prepare for reading an ICC profile
	setup_read_icc_profile(pjInfo);

	// Determine data source
	jpeg_mem_src(pjInfo, (LPBYTE)lpJpegData, dwLenData);

	// Determine image information
	jpeg_read_header(pjInfo, TRUE);

	// Read an existing ICC profile. In the case of a CMYK output, however,
	// we rudimentarily convert CMYK to RGB and discard the color profile.
	UINT uProfileLen = 0;
	BOOL bHasProfile = FALSE;
	if (pjInfo->out_color_space != JCS_CMYK)
		bHasProfile = read_icc_profile(pjInfo, &JpegDecompress.lpProfileData, &uProfileLen);

	// Image resolution
	LONG lXPelsPerMeter = 0;
	LONG lYPelsPerMeter = 0;
	if (pjInfo->saw_JFIF_marker)
	{
		if (pjInfo->density_unit == 1)
		{ // Pixel per inch
			lXPelsPerMeter = pjInfo->X_density * 5000 / 127;
			lYPelsPerMeter = pjInfo->Y_density * 5000 / 127;
		}
		else if (pjInfo->density_unit == 2)
		{ // Pixel per cm
			lXPelsPerMeter = pjInfo->X_density * 100;
			lYPelsPerMeter = pjInfo->Y_density * 100;
		}
	}

	// Start decompression in the JPEG library
	jpeg_start_decompress(pjInfo);

	// Determine output image format
	UINT uNumColors = 0;
	WORD wBitDepth = pjInfo->output_components << 3;
	if (wBitDepth == 8)
		uNumColors = pjInfo->out_color_space == JCS_GRAYSCALE ? 256 : pjInfo->actual_number_of_colors;
	UINT uIncrement = WIDTHBYTES(pjInfo->output_width * wBitDepth);
	DWORD dwImageSize = uIncrement * pjInfo->output_height;
	DWORD dwHeaderSize = bHasProfile ? sizeof(BITMAPV5HEADER) : sizeof(BITMAPINFOHEADER);

	// Allocate memory for the DIB
	hDib = GlobalAlloc(GHND, dwHeaderSize + uNumColors * sizeof(RGBQUAD) + uProfileLen + dwImageSize);
	if (hDib == NULL)
	{
		cleanup_jpeg_to_dib(&JpegDecompress, hDib);
		return NULL;
	}

	// Fill bitmap information block
	LPBITMAPV5HEADER lpBIV5 = (LPBITMAPV5HEADER)GlobalLock(hDib);
	if (lpBIV5 == NULL)
	{
		cleanup_jpeg_to_dib(&JpegDecompress, hDib);
		return NULL;
	}

	lpBIV5->bV5Size = dwHeaderSize;
	lpBIV5->bV5Width = pjInfo->output_width;
	lpBIV5->bV5Height = pjInfo->output_height;
	lpBIV5->bV5Planes = 1;
	lpBIV5->bV5BitCount = wBitDepth;
	lpBIV5->bV5Compression = BI_RGB;
	lpBIV5->bV5XPelsPerMeter = lXPelsPerMeter;
	lpBIV5->bV5YPelsPerMeter = lYPelsPerMeter;
	lpBIV5->bV5ClrUsed = uNumColors;

	// Create color table
	if (wBitDepth == 8)
	{
		LPRGBQUAD lprgbqColors = (LPRGBQUAD)FindDibPalette((LPCSTR)lpBIV5);
		if (pjInfo->out_color_space == JCS_GRAYSCALE)
		{ // Grayscale image
			for (UINT u = 0; u < uNumColors; u++)
			{  // Create grayscale
				lprgbqColors[u].rgbRed      = (BYTE)u;
				lprgbqColors[u].rgbGreen    = (BYTE)u;
				lprgbqColors[u].rgbBlue     = (BYTE)u;
				lprgbqColors[u].rgbReserved = (BYTE)0;
			}
		}
		else
		{ // Palette image
			for (UINT u = 0; u < uNumColors; u++)
			{  // Copy color table
				lprgbqColors[u].rgbRed   = pjInfo->colormap[0][u];
				lprgbqColors[u].rgbGreen = pjInfo->colormap[1][u];
				lprgbqColors[u].rgbBlue  = pjInfo->colormap[2][u];
				lprgbqColors[u].rgbReserved = 0;
			}
		}
	}

	// Embed an existing ICC profile into the DIB
	if (bHasProfile)
	{
		lpBIV5->bV5CSType = PROFILE_EMBEDDED;
		lpBIV5->bV5Intent = LCS_GM_IMAGES;
		lpBIV5->bV5ProfileSize = uProfileLen;
		lpBIV5->bV5ProfileData = dwHeaderSize + uNumColors * sizeof(RGBQUAD);

		CopyMemory((LPBYTE)lpBIV5 + lpBIV5->bV5ProfileData, JpegDecompress.lpProfileData, uProfileLen);
	}

	// Determine pointer to start of image data
	LPBYTE lpDIB = FindDibBits((LPCSTR)lpBIV5);
	LPBYTE lpBits = NULL;            // Pointer to a DIB image row
	JSAMPROW lpScanlines[1] = { 0 }; // Pointer to a scanline
	JDIMENSION uScanline = 0;        // Row index
	BYTE cKey, cRed, cGreen, cBlue;  // Color components
	// Consider that Adobe Photoshop writes inverted CMYK data
	BYTE cInv = pjInfo->saw_Adobe_marker ? 0x00 : 0xFF;

	// Copy image rows (scanlines). The arrangement of the color
	// components must be changed in jmorecfg.h from RGB to BGR.
	while (pjInfo->output_scanline < pjInfo->output_height)
	{
		uScanline = pjInfo->output_height-1 - pjInfo->output_scanline;
		lpBits = lpDIB + (UINT_PTR)uScanline * uIncrement;
		lpScanlines[0] = lpBits;

		// Decompress one line
		jpeg_read_scanlines(pjInfo, lpScanlines, 1);

		if (pjInfo->out_color_space == JCS_CMYK)
		{ // Convert from CMYK to RGB
			for (UINT u = 0; u < (pjInfo->output_width * 4); u += 4)
			{
				cKey   = lpBits[u + 3] ^ cInv;
				cBlue  = Mul8Bit(lpBits[u + 2] ^ cInv, cKey);
				cGreen = Mul8Bit(lpBits[u + 1] ^ cInv, cKey);
				cRed   = Mul8Bit(lpBits[u + 0] ^ cInv, cKey);
				lpBits[u + 0] = cBlue;
				lpBits[u + 1] = cGreen;
				lpBits[u + 2] = cRed;
				lpBits[u + 3] = 0xFF;
			}
		}
	}

	// Finish decompression
	GlobalUnlock(hDib);
	jpeg_finish_decompress(pjInfo);

	// Free all requested memory, but keep the DIB we just created
	cleanup_jpeg_to_dib(&JpegDecompress, NULL);
	JpegDecompress.bNeedDestroy = FALSE;

	return hDib;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void cleanup_jpeg_to_dib(LPJPEG_DECOMPRESS lpJpegDecompress, HANDLE hDib)
{
	if (lpJpegDecompress != NULL)
	{
		// Release the ICC profile data
		if (lpJpegDecompress->lpProfileData != NULL)
		{
			free(lpJpegDecompress->lpProfileData);
			lpJpegDecompress->lpProfileData = NULL;
		}

		// Destroy the JPEG decompress object
		if (lpJpegDecompress->bNeedDestroy)
			jpeg_destroy_decompress(&lpJpegDecompress->jInfo);
	}

	// Release the DIB
	if (hDib != NULL)
	{
		GlobalUnlock(hDib);
		GlobalFree(hDib);
		hDib = NULL;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

void set_error_manager(j_common_ptr pjInfo, j_error_mgr* pjError)
{
	// Activate the default error manager
	jpeg_std_error(pjError);

	// Set callback functions
	pjError->error_exit = my_error_exit;
	pjError->output_message = my_output_message;
	pjError->emit_message = my_emit_message;

	// Set trace level
	pjError->trace_level = ((LPJPEG_DECOMPRESS)pjInfo)->nTraceLevel;

	// Assign the error manager structure to the JPEG object
	pjInfo->err = pjError;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// If an error occurs in libjpeg, my_error_exit is called. In this case, a
// precise error message should be displayed on the screen. Then my_error_exit
// deletes the JPEG object and returns to the JpegToDib function using throw.

void my_error_exit(j_common_ptr pjInfo)
{
	// Display error message on screen
	(*pjInfo->err->output_message)(pjInfo);

	// Delete JPEG object (e.g. delete temporary files, memory, etc.)
	jpeg_destroy(pjInfo);
	((LPJPEG_DECOMPRESS)pjInfo)->bNeedDestroy = FALSE;

	longjmp(JmpBuffer, 1);  // Return to setjmp in JpegToDib
}

////////////////////////////////////////////////////////////////////////////////////////////////
// This callback function formats a message and displays it on the screen

void my_output_message(j_common_ptr pjInfo)
{
	char szBuffer[JMSG_LENGTH_MAX];
	TCHAR szMessage[JMSG_LENGTH_MAX];

	// Format text
	(*pjInfo->err->format_message)(pjInfo, szBuffer);

	// Output text
	mbstowcs(szMessage, szBuffer, JMSG_LENGTH_MAX - 1);

	HWND hDlg = GetActiveWindow();
	HWND hwndEdit = GetDlgItem(hDlg, IDC_OUTPUT);
	if (hwndEdit == NULL)
		MessageBox(hDlg, szMessage, g_szTitle, MB_OK | MB_ICONEXCLAMATION);
	else
	{
		OutputText(hwndEdit, szMessage);
		OutputText(hwndEdit, TEXT("\r\n"));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
// This function handles all messages (trace, debug or error printouts) generated by libjpeg.
// my_emit_message determines whether the message is suppressed or displayed on the screen:
//  -1: Warning
//   0: Important message (e.g. error)
// 1-3: Trace information

void my_emit_message(j_common_ptr pjInfo, int nMessageLevel)
{
	if (nMessageLevel < 0)
	{ // Warning -> Display only if trace_level >= 1
		if ((pjInfo->err->num_warnings == 0 && pjInfo->err->trace_level >= 1) ||
			pjInfo->err->trace_level >= 3)
			(*pjInfo->err->output_message)(pjInfo);
		// Increase warning counter
		pjInfo->err->num_warnings++;
	}
	else if (nMessageLevel == 0)
	{ // Important message -> Display on screen
		(*pjInfo->err->output_message)(pjInfo);
	}
	else if (pjInfo->err->trace_level >= nMessageLevel)
	{ // Trace information -> Display if trace_level >= msg_level
		(*pjInfo->err->output_message)(pjInfo);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
