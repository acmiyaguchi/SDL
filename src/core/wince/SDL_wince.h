/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* Windows CE helpers: string conversion, errors, and small shims for
   desktop Win32 calls that CE lacks, so shared Windows code can be reused. */

#ifndef SDL_wince_h_
#define SDL_wince_h_

#ifndef UNICODE
#define UNICODE 1
#endif
#include <windows.h>

#include "SDL_stdinc.h"
#include "SDL_rect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* UTF-8 <-> UTF-16. Results are SDL_malloc()ed; NULL on failure. */
extern WCHAR *WINCE_UTF8ToWide(const char *s);
extern char *WINCE_WideToUTF8(const WCHAR *w);

#define WIN_StringToUTF8W(S) WINCE_WideToUTF8(S)
#define WIN_UTF8ToStringW(S) WINCE_UTF8ToWide(S)
#define WIN_StringToUTF8     WINCE_WideToUTF8
#define WIN_UTF8ToString     WINCE_UTF8ToWide
#define SDL_tcslen           SDL_wcslen
#define SDL_tcsstr           SDL_wcsstr

/* Sets an error message from GetLastError(). Always returns -1. */
extern int WIN_SetError(const char *prefix);

extern void WIN_RECTToRect(const RECT *winrect, SDL_Rect *sdlrect);
extern void WIN_RectToRECT(const SDL_Rect *sdlrect, RECT *winrect);
extern BOOL WIN_IsRectEmpty(const RECT *rect);

/* --- shims for desktop-only calls --- */

#ifndef SEM_FAILCRITICALERRORS
#define SEM_FAILCRITICALERRORS 0x0001
#endif
#ifndef SEM_NOOPENFILEERRORBOX
#define SEM_NOOPENFILEERRORBOX 0x8000
#endif
/* CE shows no critical-error dialogs for file opens. */
#define SetErrorMode(mode) ((UINT)0)

/* Files on CE are below 4 GB; CE's SetFilePointer takes no high part. */
static __inline BOOL WINCE_GetFileSizeEx(HANDLE h, LARGE_INTEGER *size)
{
    DWORD lo = GetFileSize(h, NULL);
    if (lo == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
        return FALSE;
    }
    size->QuadPart = lo;
    return TRUE;
}

static __inline BOOL WINCE_SetFilePointerEx(HANDLE h, LARGE_INTEGER dist, LARGE_INTEGER *newpos, DWORD method)
{
    DWORD pos;
    if (dist.QuadPart < -0x7FFFFFFFLL || dist.QuadPart > 0x7FFFFFFFLL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    SetLastError(NO_ERROR);
    pos = SetFilePointer(h, (LONG)dist.QuadPart, NULL, method);
    if (pos == 0xFFFFFFFF && GetLastError() != NO_ERROR) {
        return FALSE;
    }
    if (newpos) {
        newpos->QuadPart = pos;
    }
    return TRUE;
}
#define GetFileSizeEx    WINCE_GetFileSizeEx
#define SetFilePointerEx WINCE_SetFilePointerEx

#ifdef __cplusplus
}
#endif

#endif /* SDL_wince_h_ */
