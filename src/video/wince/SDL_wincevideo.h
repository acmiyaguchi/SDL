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

#ifndef SDL_wincevideo_h_
#define SDL_wincevideo_h_

#include "../../core/wince/SDL_wince.h"
#include "../SDL_sysvideo.h"
#include "SDL_touch.h"
#include "SDL_scancode.h"

/* The PW-SH4's GWES surface. GetSystemMetrics reports a legacy 480x320 to
   apps started from the stock library, so the window is created at this
   size and the real client size is read back with GetClientRect. */
#define WINCE_DEFAULT_WIDTH  800
#define WINCE_DEFAULT_HEIGHT 480

typedef struct
{
    SDL_Window *window;
    HWND hwnd;
    HDC hdc;      /* window DC */
    HDC mdc;      /* memory DC holding the framebuffer DIB */
    HBITMAP hbm;
    SDL_bool pen_down;
} SDL_WindowData;

extern SDL_TouchID WINCE_touch_id;

extern LRESULT CALLBACK WINCE_WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern void WINCE_PumpEvents(_THIS);
extern SDL_Scancode WINCE_VKToScancode(WPARAM vk);

extern int WINCE_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch);
extern int WINCE_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects);
extern void WINCE_DestroyWindowFramebuffer(_THIS, SDL_Window *window);

#endif /* SDL_wincevideo_h_ */
