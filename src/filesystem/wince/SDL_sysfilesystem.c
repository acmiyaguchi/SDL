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

#ifdef SDL_FILESYSTEM_WINCE

/* Windows CE filesystem paths. CE has no current directory, so the base
   path (the exe's folder) is what relative paths should be resolved from. */

#include "../../core/wince/SDL_wince.h"
#include "SDL_error.h"
#include "SDL_filesystem.h"

#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif

char *SDL_GetBasePath(void)
{
    WCHAR path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, path, SDL_arraysize(path));
    int i;

    if (len == 0 || len >= SDL_arraysize(path)) {
        WIN_SetError("Couldn't locate our .exe");
        return NULL;
    }
    for (i = (int)len - 1; i > 0; i--) {
        if (path[i] == L'\\') {
            break;
        }
    }
    path[i + 1] = 0; /* chop off filename, keep the trailing backslash */
    return WINCE_WideToUTF8(path);
}

static int WINCE_AppendDir(WCHAR *path, size_t size, const char *name)
{
    WCHAR *wname = WINCE_UTF8ToWide(name);
    if (!wname) {
        return SDL_OutOfMemory();
    }
    if (SDL_wcslen(path) + SDL_wcslen(wname) + 2 > size) {
        SDL_free(wname);
        return SDL_SetError("Path too long.");
    }
    SDL_wcslcat(path, L"\\", size);
    SDL_wcslcat(path, wname, size);
    SDL_free(wname);
    if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        return WIN_SetError("Couldn't create a prefpath.");
    }
    return 0;
}

char *SDL_GetPrefPath(const char *org, const char *app)
{
    WCHAR path[MAX_PATH];

    if (!app) {
        SDL_InvalidParamError("app");
        return NULL;
    }
    if (!org) {
        org = "";
    }
    /* Usually "\\Application Data"; fall back to "\\My Documents". */
    path[0] = 0;
    if (!SHGetSpecialFolderPath(NULL, path, CSIDL_APPDATA, TRUE) || !path[0]) {
        SDL_wcslcpy(path, L"\\My Documents", SDL_arraysize(path));
        CreateDirectoryW(path, NULL);
    }
    if (*org && WINCE_AppendDir(path, SDL_arraysize(path), org) < 0) {
        return NULL;
    }
    if (WINCE_AppendDir(path, SDL_arraysize(path), app) < 0) {
        return NULL;
    }
    SDL_wcslcat(path, L"\\", SDL_arraysize(path));
    return WINCE_WideToUTF8(path);
}

#endif /* SDL_FILESYSTEM_WINCE */
