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

#define PALVERSION          0x300
#define BFT_BITMAP          0x4d42      // 'BM'

#define BI_BGRA             5L          // Windows NT 5 Beta only
#define BI_ALPHABITFIELDS   6L          // Unlike documented, this value is valid under Windows CE 3.0+
#define BI_FOURCC           7L          // Windows Mobile 5.0+, Windows CE 6.0+

#define BI_CMYK             10L         // GDI internal. The value in [MS-WMF] appears to be incorrect.
#define BI_CMYKRLE8         11L         // GDI internal. The value in [MS-WMF] appears to be incorrect.
#define BI_CMYKRLE4         12L         // GDI internal. The value in [MS-WMF] appears to be incorrect.

#define BI_SRCPREROTATE     0x8000      // Windows Mobile 5.0+, Windows CE 6.0+

#ifndef LCS_DEVICE_RGB
#define LCS_DEVICE_RGB                  0x00000001L // Obsolete
#endif
#ifndef LCS_DEVICE_CMYK
#define LCS_DEVICE_CMYK                 0x00000002L // Obsolete
#endif

#ifndef LCS_GM_ABS_COLORIMETRIC
#define LCS_GM_ABS_COLORIMETRIC         0x00000008L
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
// OS/2 Presentation Manager Bitmap type declarations

#define BFT_BITMAPARRAY     0x4142 // 'BA'
#define BFT_BMAP            0x4d42 // 'BM'

#define BCA_UNCOMP          0L
#define BCA_RLE8            1L
#define BCA_RLE4            2L
#define BCA_HUFFMAN1D       3L
#define BCA_RLE24           4L

#define BRU_METRIC          0L

#define BRA_BOTTOMUP        0L

#define BRH_NOTHALFTONED    0L
#define BRH_ERRORDIFFUSION  1L
#define BRH_PANDA           2L
#define BRH_SUPERCIRCLE     3L

#define BCE_PALETTE         (-1L)
#define BCE_RGB             0L

#pragma pack(push,1)

typedef struct _BITMAPARRAYFILEHEADER
{
    USHORT usType;
    ULONG  cbSize;
    ULONG  offNext;
    USHORT cxDisplay;
    USHORT cyDisplay;
    BITMAPFILEHEADER bfh;
} BITMAPARRAYFILEHEADER, FAR* LPBITMAPARRAYFILEHEADER, * PBITMAPARRAYFILEHEADER;

typedef struct _BITMAPINFOHEADER2
{
    ULONG  cbFix;
    ULONG  cx;
    ULONG  cy;
    USHORT cPlanes;
    USHORT cBitCount;
    ULONG  ulCompression;
    ULONG  cbImage;
    ULONG  cxResolution;
    ULONG  cyResolution;
    ULONG  cclrUsed;
    ULONG  cclrImportant;
    USHORT usUnits;
    USHORT usReserved;
    USHORT usRecording;
    USHORT usRendering;
    ULONG  cSize1;
    ULONG  cSize2;
    ULONG  ulColorEncoding;
    ULONG  ulIdentifier;
} BITMAPINFOHEADER2, FAR* LPBITMAPINFOHEADER2, * PBITMAPINFOHEADER2;

#pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////////////////////
// Adobe Photoshop bitmap header extensions

typedef struct _BITMAPV2INFOHEADER
{
    DWORD  biSize;
    LONG   biWidth;
    LONG   biHeight;
    WORD   biPlanes;
    WORD   biBitCount;
    DWORD  biCompression;
    DWORD  biSizeImage;
    LONG   biXPelsPerMeter;
    LONG   biYPelsPerMeter;
    DWORD  biClrUsed;
    DWORD  biClrImportant;
    DWORD  biRedMask;
    DWORD  biGreenMask;
    DWORD  biBlueMask;
} BITMAPV2INFOHEADER, FAR* LPBITMAPV2INFOHEADER, * PBITMAPV2INFOHEADER;

typedef struct _BITMAPV3INFOHEADER
{
    DWORD  biSize;
    LONG   biWidth;
    LONG   biHeight;
    WORD   biPlanes;
    WORD   biBitCount;
    DWORD  biCompression;
    DWORD  biSizeImage;
    LONG   biXPelsPerMeter;
    LONG   biYPelsPerMeter;
    DWORD  biClrUsed;
    DWORD  biClrImportant;
    DWORD  biRedMask;
    DWORD  biGreenMask;
    DWORD  biBlueMask;
    DWORD  biAlphaMask;
} BITMAPV3INFOHEADER, FAR* LPBITMAPV3INFOHEADER, * PBITMAPV3INFOHEADER;

////////////////////////////////////////////////////////////////////////////////////////////////

// WIDTHBYTES returns the number of DWORD-aligned bytes needed to
// hold the bit count of a DIB scanline (biWidth * biBitCount)
#define WIDTHBYTES(bits)    (((bits) + 31) / 32 * 4)

#define IS_OS2PM_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPCOREHEADER))
#define IS_WIN30_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPINFOHEADER))
#define IS_WIN40_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPV4HEADER))
#define IS_WIN50_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPV5HEADER))
#define IS_OS2V2_DIB(lpbi)  ((*(LPDWORD)(lpbi)) == sizeof(BITMAPINFOHEADER2))

////////////////////////////////////////////////////////////////////////////////////////////////

// Incorporating rounding, Mul8Bit is an approximation of a * b / 255 for values in the
// range [0...255]. For details, see the documentation of the DrvAlphaBlend function.
__inline int Mul8Bit(int a, int b)
{ int t = a * b + 128; return (t + (t >> 8)) >> 8; }

// Saves a packed DIB as a Windows Bitmap file
BOOL SaveBitmap(LPCTSTR lpszFileName, HANDLE hDib);

// Saves the embedded profile of a DIB in an ICC color profile file
BOOL SaveProfile(LPCTSTR lpszFileName, HANDLE hDib);

// Creates a 32-bpp DIB with pre-multiplied alpha from a translucent 16/32-bpp DIB
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

// Creates the most descriptive clipboard format from the passed DIB.
// If puFormat is NULL or contains CF_DIB, a DIBv3 or DIBv5 is generated
// depending on the source DIB. If puFormat contains CF_DIBV5, a DIBv5 is
// always generated. puFormat receives the generated format on return.
HANDLE CreateClipboardDib(HANDLE hDib, UINT* puFormat = NULL);

// Creates a logical color palette from a given DIB.
// If an error occurs, a halftone palette is created.
HPALETTE CreateDibPalette(HANDLE hDib);

// Gets the width and height of a DIB
BOOL GetDIBDimensions(LPCSTR lpbi, LPLONG lplWidth, LPLONG lplHeight, BOOL bAbsolute = FALSE);

// Calculates the number of colors in the DIBs color table
UINT DibNumColors(LPCSTR lpbi);

// Gets the size of the optional color masks of a DIBv3
UINT ColorMasksSize(LPCSTR lpbi);

// Gets the size required to store the DIBs color table
UINT PaletteSize(LPCSTR lpbi);

// Gets the size of the DIBs bitmap bits. For uncompressed bitmaps,
// this function calculates the size from the individual header entries.
// The value in biSizeImage is only returned for compressed bitmaps.
UINT DibImageSize(LPCSTR lpbi);

// Gets the offset from the beginning of the DIB to the bitmap bits
UINT DibBitsOffset(LPCSTR lpbi);

// Returns a pointer to the DIBs color table 
LPBYTE FindDibPalette(LPCSTR lpbi);

// Returns a pointer to the pixel bits of a packed DIB
LPBYTE FindDibBits(LPCSTR lpbi);

// Checks if the DIBv5 has an embedded ICC profile
BOOL DibHasEmbeddedProfile(LPCSTR lpbi);

// Checks if the DIBv5 has a linked or embedded ICC profile
BOOL DibHasColorProfile(LPCSTR lpbi);

// Checks if the DIB contains color space data
BOOL DibHasColorSpaceData(LPCSTR lpbi);

// Checks if the bitmap bits of the DIB are compressed
BOOL DibIsCompressed(LPCSTR lpbi);

// Checks if the biCompression member of a DIBv3 struct contains a FourCC code
BOOL DibIsCustomFormat(LPCSTR lpbi);

// Checks whether the DIB uses the CMYK color model
BOOL DibIsCMYK(LPCSTR lpbi);

// Determines whether the display device can draw a specific DIB
BOOL IsDibSupported(LPCSTR lpbi);

// Determines whether the display device supports a specific DIB
DWORD QueryDibSupport(LPCSTR lpbi);

////////////////////////////////////////////////////////////////////////////////////////////////
