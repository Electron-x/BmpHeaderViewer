////////////////////////////////////////////////////////////////////////////////////////////////
// Misc.h - Copyright (c) 2024 by W. Rolke.
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

__inline DWORD GetFilePointer(HANDLE hFile)
{ return SetFilePointer(hFile, 0, NULL, FILE_CURRENT); }

__inline BOOL FileSeekBegin(HANDLE hFile, LONG lDistanceToMove)
{ return (SetFilePointer(hFile, lDistanceToMove, NULL, FILE_BEGIN) != 0xFFFFFFFF); }

__inline BOOL FileSeekCurrent(HANDLE hFile, LONG lDistanceToMove)
{ return (SetFilePointer(hFile, lDistanceToMove, NULL, FILE_CURRENT) != 0xFFFFFFFF); }

// Reads the specified number of bytes from a file and checks whether the desired
// number of bytes has been read. hFile must be a synchronous file handle.
BOOL MyReadFile(HANDLE hFile, LPVOID lpBuffer, SIZE_T cbSize);

// Replacement for GlobalAllocPtr from windowsx.h to avoid warning C28183
LPVOID MyGlobalAllocPtr(UINT uFlags, SIZE_T dwBytes);

// Replacement for GlobalFreePtr from windowsx.h to avoid warning C6387
void MyGlobalFreePtr(LPCVOID pMem);

// Wrapper for lstrcpynA to avoid warning C6031
LPSTR MyStrNCpyA(LPSTR lpString1, LPCSTR lpString2, int iMaxLength);

// Wrapper for lstrcpynW to avoid warning C6031
LPWSTR MyStrNCpyW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength);

#ifdef UNICODE
#define MyStrNCpy MyStrNCpyW
#else
#define MyStrNCpy MyStrNCpyA
#endif

// Determines whether the specified key was down at the time the input message was generated
BOOL IsKeyDown(int nVirtKey);

// Sets the title of the thumbnail
void SetThumbnailText(HWND hwndThumb, UINT uID = 0);

// Frees the thumbnail bitmaps and sets the thumbnail title
void ResetThumbnail(HWND hwndThumb, LPCTSTR lpszTitle = NULL);

// Replaces the current thumbnail with the given DIB
void ReplaceThumbnail(HWND hwndThumb, HANDLE hDib);

// Clears the text of an edit control, resets the undo flag, and clears the modification flag
void ClearOutputWindow(HWND hwndEdit);

// Inserts the passed text at the current cursor position of an edit control
void OutputText(HWND hwndEdit, LPCTSTR lpszOutput);

// Formats text with _vsntprintf and inserts it at the current cursor position of an edit control
void OutputTextFmt(HWND hwndEdit, LPCTSTR lpszFormat, ...);

// Loads a message from the resources and replaces the placeholder {COUNT} with the passed
// value (to keep the printf-style format specifiers under internal control). The text is
// then inserted with a separator line at the current cursor position of an edit control.
BOOL OutputTextFmtCount(HWND hwndEdit, UINT uID, SIZE_T uCount);

// Loads an error message from the resources and inserts it with a
// separator line at the current cursor position of an edit control
BOOL OutputTextFromID(HWND hwndEdit, UINT uID);

// Retrieves the message text for a system-defined error and inserts it
// with a separator line at the current cursor position of an edit control
BOOL OutputErrorMessage(HWND hwndEdit, DWORD dwError);

// Appends a string to another string with an OR separator. Is used to list flag names
PTCHAR AppendFlagName(LPTSTR lpszOutput, SIZE_T cchLenOutput, LPCTSTR lpszFlagName);

// Converts any byte value into a string of three digits
BOOL FormatByteSize(DWORD dwSize, LPTSTR lpszString, SIZE_T cchStringLen);

// Replaces each occurrence of a search pattern in a string with a different string.
// The memory of the returned character string must be freed using MyGlobalFreePtr.
LPTSTR AllocReplaceString(LPCTSTR lpszOriginal, LPCTSTR lpszPattern, LPCTSTR lpszReplacement);

// Creates a form of the specified path with a length smaller than cchBuffer. For this purpose
// the short path form is retrieved. If this is not successful, only the filename is returned.
SIZE_T ShortenPath(LPCTSTR lpszLongPath, LPTSTR lpszShortPath, SIZE_T cchBuffer);

// Displays the File Open or Save As dialog box
BOOL GetFileName(HWND hDlg, LPTSTR lpszFileName, SIZE_T cchStringLen, LPDWORD lpdwFilterIndex, BOOL bSave = FALSE);

// Displays the Color Management dialog box
BOOL ColorMatchUI(HWND hDlg);

////////////////////////////////////////////////////////////////////////////////////////////////
