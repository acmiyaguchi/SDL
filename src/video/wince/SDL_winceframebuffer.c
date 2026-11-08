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

#ifdef SDL_VIDEO_DRIVER_WINCE

/* Framebuffer: a top-down 565 DIB section, BitBlt'ed per dirty rect. */

#include "SDL_wincevideo.h"

#ifndef BI_BITFIELDS
#define BI_BITFIELDS 3
#endif

typedef struct
{
    BITMAPINFOHEADER header;
    DWORD masks[3];
} WINCE_BitmapInfo565;

int WINCE_CreateWindowFramebuffer(_THIS, SDL_Window *window, Uint32 *format, void **pixels, int *pitch)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    WINCE_BitmapInfo565 info;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);
    WINCE_DestroyWindowFramebuffer(_this, window);

    *format = SDL_PIXELFORMAT_RGB565;
    *pitch = ((w * 2) + 3) & ~3;

    SDL_zero(info);
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = w;
    info.header.biHeight = -h; /* top-down */
    info.header.biPlanes = 1;
    info.header.biBitCount = 16;
    info.header.biCompression = BI_BITFIELDS;
    info.header.biSizeImage = (DWORD)h * (*pitch);
    info.masks[0] = 0xF800;
    info.masks[1] = 0x07E0;
    info.masks[2] = 0x001F;

    data->mdc = CreateCompatibleDC(data->hdc);
    data->hbm = CreateDIBSection(data->hdc, (BITMAPINFO *)&info, DIB_RGB_COLORS, pixels, NULL, 0);
    if (!data->mdc || !data->hbm) {
        WINCE_DestroyWindowFramebuffer(_this, window);
        return WIN_SetError("Unable to create DIB");
    }
    SelectObject(data->mdc, data->hbm);
    return 0;
}

int WINCE_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    int i;
    for (i = 0; i < numrects; ++i) {
        BitBlt(data->hdc, rects[i].x, rects[i].y, rects[i].w, rects[i].h,
               data->mdc, rects[i].x, rects[i].y, SRCCOPY);
    }
    return 0;
}

void WINCE_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return;
    }
    if (data->mdc) {
        DeleteDC(data->mdc);
        data->mdc = NULL;
    }
    if (data->hbm) {
        DeleteObject(data->hbm);
        data->hbm = NULL;
    }
}

#endif /* SDL_VIDEO_DRIVER_WINCE */
