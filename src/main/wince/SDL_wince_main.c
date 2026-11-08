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

/* WinMain for Windows CE: CE passes only the arguments (no program name) as
   UTF-16 and has no CommandLineToArgvW. argv[0] is the module path; the rest
   is split with the usual Windows quoting rules (quotes group, backslashes
   before a quote escape it). Strings are UTF-8. */

#include "SDL_config.h"

#ifdef __WINCE__

#include <windows.h>

#include "SDL.h"
#include "SDL_main.h"

#ifdef main
#undef main
#endif

static char *WINCE_ToUTF8(const WCHAR *w)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s;
    if (n <= 0) {
        n = 1;
    }
    s = (char *)SDL_malloc(n);
    if (s) {
        s[0] = '\0';
        WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
        s[n - 1] = '\0';
    }
    return s;
}

/* Split cmd into args, writing the strings into buf (at least as long as
   cmd). When args is NULL, only counts. Returns the argument count. */
static int WINCE_SplitArgs(const WCHAR *cmd, WCHAR *buf, WCHAR **args)
{
    int count = 0;
    while (*cmd) {
        int quoted = 0;
        while (*cmd == L' ' || *cmd == L'\t') {
            cmd++;
        }
        if (!*cmd) {
            break;
        }
        if (args) {
            args[count] = buf;
        }
        while (*cmd && (quoted || (*cmd != L' ' && *cmd != L'\t'))) {
            int slashes = 0, i;
            while (*cmd == L'\\') {
                slashes++;
                cmd++;
            }
            if (*cmd == L'"') {
                for (i = 0; i < slashes / 2; i++) {
                    *buf++ = L'\\';
                }
                if (slashes % 2) {
                    *buf++ = L'"';
                } else {
                    quoted = !quoted;
                }
                cmd++;
            } else {
                for (i = 0; i < slashes; i++) {
                    *buf++ = L'\\';
                }
                if (*cmd && (quoted || (*cmd != L' ' && *cmd != L'\t'))) {
                    *buf++ = *cmd++;
                }
            }
        }
        *buf++ = 0;
        count++;
    }
    return count;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR szCmdLine, int sw)
{
    WCHAR modpath[MAX_PATH];
    const WCHAR *cmd = szCmdLine ? szCmdLine : L"";
    WCHAR *buf, **wargs;
    char **argv;
    int argc, n, i, result;

    (void)hInst;
    (void)hPrev;
    (void)sw;

    buf = (WCHAR *)SDL_malloc((wcslen(cmd) + 1) * sizeof(WCHAR));
    if (!buf) {
        return -1;
    }
    n = WINCE_SplitArgs(cmd, buf, NULL);
    wargs = (WCHAR **)SDL_malloc((n + 1) * sizeof(WCHAR *));
    argv = (char **)SDL_malloc((n + 2) * sizeof(char *));
    if (!wargs || !argv) {
        return -1;
    }
    WINCE_SplitArgs(cmd, buf, wargs);

    modpath[0] = 0;
    GetModuleFileNameW(NULL, modpath, MAX_PATH);
    argv[0] = WINCE_ToUTF8(modpath);
    for (i = 0; i < n; i++) {
        argv[i + 1] = WINCE_ToUTF8(wargs[i]);
    }
    argc = n + 1;
    argv[argc] = NULL;

    SDL_SetMainReady();
    result = SDL_main(argc, argv);

    for (i = 0; i < argc; i++) {
        SDL_free(argv[i]);
    }
    SDL_free(argv);
    SDL_free(wargs);
    SDL_free(buf);
    return result;
}

#endif /* __WINCE__ */
