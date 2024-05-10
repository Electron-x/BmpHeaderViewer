////////////////////////////////////////////////////////////////////////////////////////////////
// DibApi.h - Copyright (c) 2024 by W. Rolke.
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

#define BFT_BITMAP          0x4d42 // 'BM'
#define PALVERSION          0x300

#ifndef BI_ALPHABITFIELDS
#define BI_ALPHABITFIELDS   6L
#endif
#ifndef LCS_DEVICE_RGB
#define LCS_DEVICE_RGB      0x00000001L
#endif
#ifndef LCS_DEVICE_CMYK
#define LCS_DEVICE_CMYK     0x00000002L
#endif

// WIDTHBYTES returns the number of DWORD-aligned bytes needed to
// hold the bit count of a DIB scanline (biWidth * biBitCount)
#define WIDTHBYTES(bits)    (((bits) + 31) / 32 * 4)

#define IS_OS2PM_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPCOREHEADER))
#define IS_WIN30_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPINFOHEADER))
#define IS_WIN40_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPV4HEADER))
#define IS_WIN50_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPV5HEADER))

// Incorporating rounding, Mul8Bit is an approximation of a * b / 255 for values in the
// range [0...255]. For details, see the documentation of the DrvAlphaBlend function.
__inline int Mul8Bit(int a, int b)
{ int t = a * b + 128; return (t + (t >> 8)) >> 8; }

// Saves a DIB as a Windows Bitmap file
BOOL SaveBitmap(LPCTSTR lpszFileName, HANDLE hDib);

// Saves the embedded profile of a DIB in an ICC color profile file
BOOL SaveProfile(LPCTSTR lpszFileName, HANDLE hDib);

// Creates a 32-bit DIB with pre-multiplied alpha from a translucent 16/32-bit DIB
HBITMAP CreatePremultipliedBitmap(HANDLE hDib);

// Frees the memory allocated for a DIB section using DeleteObject
HBITMAP FreeBitmap(HBITMAP hbmpDib);

// Frees the memory allocated for a DIB using GlobalFree
HANDLE FreeDib(HANDLE hDib);

// Creates a copy of a DIB
HANDLE CopyDib(HANDLE hDib);

// Decompresses a video compressed DIB using Video Compression Manager
HANDLE DecompressDib(HANDLE hDib);

// Converts any DIB to a compatible bitmap and then back to a DIB with the desired bit depth
HANDLE ChangeDibBitDepth(HANDLE hDib, WORD wBitCount = 0);

// Converts a compatible bitmap to a device-independent bitmap with the desired bit depth.
// This function takes a DC instead of a logical palette (as support for ChangeDibBitDepth).
HANDLE ConvertBitmapToDib(HBITMAP hBitmap, HDC hdc = NULL, WORD wBitCount = 0);

// Creates a DIB from a clipboard DIB in which the profile data follows the bitmap bits
HANDLE CreateDibFromClipboardDib(HANDLE hDib);

// Creates a memory object from a given DIB, which can be placed on the clipboard.
// Depending on the source DIB, a CF_DIB or CF_DIBV5 is created. The system can then
// create the other formats itself. puFormat returns the format for SetClipboardData.
HANDLE CreateClipboardDib(HANDLE hDib, UINT* puFormat = NULL);

// Creates a logical color palette from a given DIB.
// If an error occurs, a halftone palette is created.
HPALETTE CreateDibPalette(HANDLE hDib);

// Calculates the number of colors in the DIBs color table
UINT DibNumColors(LPCSTR lpbi);

// Gets the size of the optional color masks of a DIBv3
UINT ColorMasksSize(LPCSTR lpbi);

// Gets the size required to store the DIBs palette
UINT PaletteSize(LPCSTR lpbi);

// Gets the size of the DIBs bitmap bits
UINT DibImageSize(LPCSTR lpbi);

// Gets the offset from the beginning of the DIB to the bitmap bits
UINT DibBitsOffset(LPCSTR lpbi);

// Returns a pointer to the DIBs color table 
LPBYTE FindDibPalette(LPCSTR lpbi);

// Returns a pointer to the pixel bits of a packed DIB
LPBYTE FindDibBits(LPCSTR lpbi);

// Checks if the DIBv5 has a linked or embedded ICC profile
BOOL DibHasColorProfile(LPCSTR lpbi);

// Checks if the DIB contains color space data
BOOL DibHasColorSpaceData(LPCSTR lpbi);

// Checks if the biCompression member of a DIBv3 struct contains a FourCC code
BOOL DibIsCustomFormat(LPCSTR lpbi);

// Checks whether the DIB uses the CMYK color model
BOOL DibIsCMYK(LPCSTR lpbi);

// Determines whether a device can draw a specific DIB
BOOL IsDibSupported(LPCSTR lpbi);

// Determines whether a device supports a specific DIB
DWORD QueryDibSupport(LPCSTR lpbi);

////////////////////////////////////////////////////////////////////////////////////////////////
