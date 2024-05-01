////////////////////////////////////////////////////////////////////////////////////////////////
// BmpHeaderViewer.h - Copyright (c) 2024 by W. Rolke.
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

#pragma once

#include "resource.h"

#define WMU_FILEOPEN WM_APP + 42

// Maximum path length for the Open or Save As dialog box
#define MY_OFN_MAX_PATH 2048

// Maximum text length for a line in an edit control
#define OUTPUT_LEN  1024

#define RGB_DARKMODE_TEXTCOLOR  RGB(255, 255, 255)
#define RGB_DARKMODE_BKCOLOR    RGB(56, 56, 56)
#define RGB_THUMBNAIL_TEXTCOLOR RGB(255, 64, 64)

#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
// For the hex dump, the separator lines must be 72 characters long

const TCHAR g_szSepThin[]  = TEXT("------------------------------------------------------------------------\r\n");
const TCHAR g_szSepThick[] = TEXT("========================================================================\r\n");

////////////////////////////////////////////////////////////////////////////////////////////////

extern const TCHAR g_szTitle[];
extern HINSTANCE g_hInstance;
extern HBITMAP g_hBitmapThumb;
extern HANDLE g_hDibThumb;
extern int g_nIcmMode;

////////////////////////////////////////////////////////////////////////////////////////////////
