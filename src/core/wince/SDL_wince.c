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
#include "../../SDL_internal.h"

#ifdef __WINCE__

#include "SDL_wince.h"
#include "SDL_error.h"

WCHAR *WINCE_UTF8ToWide(const char *s)
{
    int n;
    WCHAR *w;
    if (!s) {
        return NULL;
    }
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) {
        return NULL;
    }
    w = (WCHAR *)SDL_malloc(n * sizeof(WCHAR));
    if (w) {
        MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    }
    return w;
}

char *WINCE_WideToUTF8(const WCHAR *w)
{
    int n;
    char *s;
    if (!w) {
        return NULL;
    }
    n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) {
        return NULL;
    }
    s = (char *)SDL_malloc(n);
    if (s) {
        WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
    }
    return s;
}

int WIN_SetError(const char *prefix)
{
    WCHAR buffer[256];
    char *message;
    DWORD err = GetLastError();
    if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err, 0,
                        buffer, SDL_arraysize(buffer), NULL)) {
        return SDL_SetError("%s%serror 0x%lx", prefix ? prefix : "", prefix ? ": " : "", (unsigned long)err);
    }
    message = WINCE_WideToUTF8(buffer);
    SDL_SetError("%s%s%s", prefix ? prefix : "", prefix ? ": " : "", message ? message : "");
    SDL_free(message);
    return -1;
}

void WIN_RECTToRect(const RECT *winrect, SDL_Rect *sdlrect)
{
    sdlrect->x = winrect->left;
    sdlrect->w = (winrect->right - winrect->left) + 1;
    sdlrect->y = winrect->top;
    sdlrect->h = (winrect->bottom - winrect->top) + 1;
}

void WIN_RectToRECT(const SDL_Rect *sdlrect, RECT *winrect)
{
    winrect->left = sdlrect->x;
    winrect->right = sdlrect->x + sdlrect->w - 1;
    winrect->top = sdlrect->y;
    winrect->bottom = sdlrect->y + sdlrect->h - 1;
}

BOOL WIN_IsRectEmpty(const RECT *rect)
{
    return (rect->right <= rect->left) || (rect->bottom <= rect->top);
}

#endif /* __WINCE__ */
