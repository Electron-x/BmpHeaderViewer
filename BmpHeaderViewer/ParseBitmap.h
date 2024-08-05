////////////////////////////////////////////////////////////////////////////////////////////////
// ParseBitmap.h - Copyright (c) 2024 by W. Rolke.
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

// Parses a Windows Bitmap file and displays its metadata
BOOL ParseBitmap(HWND hDlg, HANDLE hFile, DWORD dwFileSize);

// Parses a DIB and displays its metadata. dwOffBits is the offset from
// the start of the DIB to the bitmap bits (can be 0 for a packed DIB).
BOOL ParseDIBitmap(HWND hDlg, HANDLE hDib, DWORD dwOffBits = 0);

////////////////////////////////////////////////////////////////////////////////////////////////
// ICC profile header declarations

#define INTENT_PERCEPTUAL               0
#define INTENT_RELATIVE_COLORIMETRIC    1
#define INTENT_SATURATION               2
#define INTENT_ABSOLUTE_COLORIMETRIC    3

#define FLAG_EMBEDDEDPROFILE            0x00000001
#define FLAG_DEPENDENTONDATA            0x00000002
#define FLAG_MCSNEEDSSUBSET             0x00000004
#define FLAG_EXTENDEDRANGEPCS           0x00000008

#define ATTRIB_TRANSPARENCY             0x00000001
#define ATTRIB_MATTE                    0x00000002
#define ATTRIB_MEDIANEGATIVE            0x00000004
#define ATTRIB_MEDIABLACKANDWHITE       0x00000008
#define ATTRIB_NONPAPERBASED            0x00000010
#define ATTRIB_TEXTURED                 0x00000020
#define ATTRIB_NONISOTROPIC             0x00000040
#define ATTRIB_SELFLUMINOUS             0x00000080

typedef struct _PROFILEV5HEADER
{
    DWORD   phSize;
    DWORD   phCMMType;
    DWORD   phVersion;
    DWORD   phClass;
    DWORD   phDataColorSpace;
    DWORD   phConnectionSpace;
    DWORD   phDateTime[3];
    DWORD   phSignature;
    DWORD   phPlatform;
    DWORD   phProfileFlags;
    DWORD   phManufacturer;
    DWORD   phModel;
    DWORD   phAttributes[2];
    DWORD   phRenderingIntent;
    CIEXYZ  phIlluminant;
    DWORD   phCreator;
    BYTE    phProfileID[16];
    DWORD   phSpectralPCS;
    WORD    phSpectralRange[3];
    WORD    phBiSpectralRange[3];
    DWORD   phMCS;
    DWORD   phSubClass;
    BYTE    phReserved[4];
} PROFILEV5HEADER, FAR* LPPROFILEV5HEADER, *PPROFILEV5HEADER;

////////////////////////////////////////////////////////////////////////////////////////////////
// Names of the 20 static system palette colors

struct ColorName
{
    COLORREF rgbColor;
    LPCTSTR lpszName;
};

const ColorName g_aColorNames[] =
{
    { RGB(0x00, 0x00, 0x00), TEXT("black") },
    { RGB(0x80, 0x00, 0x00), TEXT("dark red") },
    { RGB(0x00, 0x80, 0x00), TEXT("dark green") },
    { RGB(0x80, 0x80, 0x00), TEXT("dark yellow") },
    { RGB(0x00, 0x00, 0x80), TEXT("dark blue") },
    { RGB(0x80, 0x00, 0x80), TEXT("dark magenta") },
    { RGB(0x00, 0x80, 0x80), TEXT("dark cyan") },
    { RGB(0xC0, 0xC0, 0xC0), TEXT("light gray") },
    { RGB(0xC0, 0xDC, 0xC0), TEXT("money green") },
    { RGB(0xA6, 0xCA, 0xF0), TEXT("sky blue") },
    { RGB(0xFF, 0xFB, 0xF0), TEXT("cream") },
    { RGB(0xA0, 0xA0, 0xA4), TEXT("light gray") },
    { RGB(0x80, 0x80, 0x80), TEXT("medium gray") },
    { RGB(0xFF, 0x00, 0x00), TEXT("red") },
    { RGB(0x00, 0xFF, 0x00), TEXT("green") },
    { RGB(0xFF, 0xFF, 0x00), TEXT("yellow") },
    { RGB(0x00, 0x00, 0xFF), TEXT("blue") },
    { RGB(0xFF, 0x00, 0xFF), TEXT("magenta") },
    { RGB(0x00, 0xFF, 0xFF), TEXT("cyan") },
    { RGB(0xFF, 0xFF, 0xFF), TEXT("white") }
};

////////////////////////////////////////////////////////////////////////////////////////////////
