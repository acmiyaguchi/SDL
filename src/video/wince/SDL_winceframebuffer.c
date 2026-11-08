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

/* Framebuffer: a top-down 565 DIB section. Presented by copying the dirty
   rects into the locked DirectDraw primary when the window is in front and
   the primary is the 800x480 565 GWES surface; otherwise BitBlt per rect.
   SDL_WINCE_DDRAW=0 forces BitBlt.

   CE 6's ddraw.dll does not match the CE 4 SDK headers, so the interfaces are
   called through vtable slots read from the PW-SH4 ROM (cerf-meta
   docs/notes/sharp-brain-pw-sh4-display.md, same as BrainPlayer's
   apps/brainplayer/ddraw_nim.nim): IDirectDraw CreateSurface 5,
   GetGDISurface 12, SetCooperativeLevel 17; IDirectDrawSurface Lock 19,
   Restore 21, Unlock 26; DDSURFACEDESC is 0x6C bytes. Writes through Lock
   mark nothing dirty in the display driver, so each present ends with 1x1
   GDI self-copies at the corners of the updated box. */

#include "SDL_wincevideo.h"
#include "SDL_timer.h"

#ifndef BI_BITFIELDS
#define BI_BITFIELDS 3
#endif

typedef struct
{
    BITMAPINFOHEADER header;
    DWORD masks[3];
} WINCE_BitmapInfo565;

typedef struct
{
    DWORD size, flags, height, width;
    LONG pitch, xpitch;
    DWORD back_buffer_count, refresh_rate;
    void *surface;
    DWORD color_keys[8];
    DWORD pixel_format[8];
    DWORD caps, surface_size;
} WINCE_DDSurfaceDesc;

SDL_COMPILE_TIME_ASSERT(wince_ddsd_size, sizeof(WINCE_DDSurfaceDesc) == 0x6C);

typedef struct { void **vtbl; } WINCE_ComObj;
typedef HRESULT (WINAPI *WINCE_DDCreate)(void *guid, void **dd, void *outer);
typedef ULONG (WINAPI *WINCE_Release)(void *self);
typedef HRESULT (WINAPI *WINCE_GetGDISurface)(void *self, void **surface);
typedef HRESULT (WINAPI *WINCE_SetCoop)(void *self, HWND hwnd, DWORD flags);
typedef HRESULT (WINAPI *WINCE_CreateSurface)(void *self, WINCE_DDSurfaceDesc *desc, void **surface, void *outer);
typedef HRESULT (WINAPI *WINCE_Lock)(void *self, RECT *rect, WINCE_DDSurfaceDesc *desc, DWORD flags, HANDLE event);
typedef HRESULT (WINAPI *WINCE_Unlock)(void *self, RECT *rect);
typedef HRESULT (WINAPI *WINCE_Restore)(void *self);

#define WINCE_VT(obj, slot, T) ((T)(((WINCE_ComObj *)(obj))->vtbl[slot]))
#define DD_SLOT_RELEASE 2
#define DD_SLOT_CREATESURFACE 5
#define DD_SLOT_GETGDISURFACE 12
#define DD_SLOT_SETCOOPERATIVELEVEL 17
#define DD_SLOT_LOCK 19
#define DD_SLOT_RESTORE 21
#define DD_SLOT_UNLOCK 26
#define WINCE_DDSD_CAPS 1
#define WINCE_DDSCAPS_PRIMARYSURFACE 0x40
#define WINCE_DDSCL_NORMAL 8

static void WINCE_DD_Close(SDL_WindowData *data)
{
    if (data->dd_primary) {
        WINCE_VT(data->dd_primary, DD_SLOT_RELEASE, WINCE_Release)(data->dd_primary);
        data->dd_primary = NULL;
    }
    if (data->dd) {
        WINCE_VT(data->dd, DD_SLOT_RELEASE, WINCE_Release)(data->dd);
        data->dd = NULL;
    }
    if (data->dd_library) {
        FreeLibrary(data->dd_library);
        data->dd_library = NULL;
    }
}

static SDL_bool WINCE_DD_Fail(SDL_WindowData *data, const char *step, HRESULT hr)
{
    SDL_Log("wince: DirectDraw %s failed (hr=0x%08lx); using BitBlt", step, (unsigned long)hr);
    WINCE_DD_Close(data);
    return SDL_FALSE;
}

static HRESULT WINCE_DD_Lock(void *surface, WINCE_DDSurfaceDesc *desc)
{
    HRESULT hr;
    SDL_zerop(desc);
    desc->size = sizeof(*desc);
    hr = WINCE_VT(surface, DD_SLOT_LOCK, WINCE_Lock)(surface, NULL, desc, 0, NULL);
    if (FAILED(hr) && SUCCEEDED(WINCE_VT(surface, DD_SLOT_RESTORE, WINCE_Restore)(surface))) {
        SDL_zerop(desc);
        desc->size = sizeof(*desc);
        hr = WINCE_VT(surface, DD_SLOT_LOCK, WINCE_Lock)(surface, NULL, desc, 0, NULL);
    }
    return hr;
}

/* Opens the primary once per framebuffer; keeps it only when it is the
   whole 800x480 RGB565 screen with a 1600-byte pitch. */
static SDL_bool WINCE_DD_Open(SDL_WindowData *data)
{
    WINCE_DDCreate create;
    WINCE_DDSurfaceDesc desc;
    HRESULT hr;

    data->dd_tried = SDL_TRUE;
    if (!SDL_GetHintBoolean("SDL_WINCE_DDRAW", SDL_TRUE)) {
        return SDL_FALSE;
    }
    data->dd_library = LoadLibraryW(L"ddraw.dll");
    if (!data->dd_library) {
        return WINCE_DD_Fail(data, "load", 0);
    }
    create = (WINCE_DDCreate)GetProcAddressW(data->dd_library, L"DirectDrawCreate");
    if (!create) {
        return WINCE_DD_Fail(data, "proc", 0);
    }
    hr = create(NULL, &data->dd, NULL);
    if (FAILED(hr) || !data->dd) {
        return WINCE_DD_Fail(data, "create", hr);
    }
    hr = WINCE_VT(data->dd, DD_SLOT_GETGDISURFACE, WINCE_GetGDISurface)(data->dd, &data->dd_primary);
    if (FAILED(hr) || !data->dd_primary) {
        data->dd_primary = NULL;
        hr = WINCE_VT(data->dd, DD_SLOT_SETCOOPERATIVELEVEL, WINCE_SetCoop)(data->dd, data->hwnd, WINCE_DDSCL_NORMAL);
        if (FAILED(hr)) {
            return WINCE_DD_Fail(data, "cooperative level", hr);
        }
        SDL_zero(desc);
        desc.size = sizeof(desc);
        desc.flags = WINCE_DDSD_CAPS;
        desc.caps = WINCE_DDSCAPS_PRIMARYSURFACE;
        hr = WINCE_VT(data->dd, DD_SLOT_CREATESURFACE, WINCE_CreateSurface)(data->dd, &desc, &data->dd_primary, NULL);
        if (FAILED(hr) || !data->dd_primary) {
            data->dd_primary = NULL;
            return WINCE_DD_Fail(data, "primary", hr);
        }
    }
    hr = WINCE_DD_Lock(data->dd_primary, &desc);
    if (FAILED(hr)) {
        return WINCE_DD_Fail(data, "probe lock", hr);
    }
    WINCE_VT(data->dd_primary, DD_SLOT_UNLOCK, WINCE_Unlock)(data->dd_primary, NULL);
    if (desc.width != WINCE_DEFAULT_WIDTH || desc.height != WINCE_DEFAULT_HEIGHT ||
        desc.pitch != WINCE_DEFAULT_WIDTH * 2 || !desc.surface) {
        SDL_Log("wince: DirectDraw primary %lux%lu pitch %ld; using BitBlt",
                (unsigned long)desc.width, (unsigned long)desc.height, (long)desc.pitch);
        WINCE_DD_Close(data);
        return SDL_FALSE;
    }
    SDL_Log("wince: presenting through the DirectDraw primary");
    return SDL_TRUE;
}

#define WINCE_DD_RETRY_MS 2000
#define WINCE_DD_MAX_RETRIES 5

/* Copies the rects into the primary. SDL_FALSE means the caller must BitBlt
   (not in front, or the lock failed). A failed lock or a primary that no
   longer has the 800x480/1600 geometry drops DirectDraw; it is reopened
   (which re-checks the geometry) after WINCE_DD_RETRY_MS, up to
   WINCE_DD_MAX_RETRIES times per framebuffer. */
static SDL_bool WINCE_DD_Present(SDL_WindowData *data, const SDL_Rect *rects, int numrects)
{
    WINCE_DDSurfaceDesc desc;
    POINT origin = { 0, 0 };
    RECT client;
    int i, x0 = WINCE_DEFAULT_WIDTH, y0 = WINCE_DEFAULT_HEIGHT, x1 = 0, y1 = 0;
    HRESULT hr;

    if (!data->dd_tried && SDL_TICKS_PASSED(SDL_GetTicks(), data->dd_retry_at)) {
        WINCE_DD_Open(data);
    }
    if (!data->dd_primary || GetForegroundWindow() != data->hwnd) {
        return SDL_FALSE;
    }
    if (!ClientToScreen(data->hwnd, &origin) || !GetClientRect(data->hwnd, &client)) {
        return SDL_FALSE;
    }
    hr = WINCE_DD_Lock(data->dd_primary, &desc);
    if (SUCCEEDED(hr) && (!desc.surface || desc.width != WINCE_DEFAULT_WIDTH ||
                          desc.height != WINCE_DEFAULT_HEIGHT || desc.pitch != WINCE_DEFAULT_WIDTH * 2)) {
        /* Locked, but not the surface we validated at open (or no pixels):
           unlock before dropping it, never write with the new geometry. */
        WINCE_VT(data->dd_primary, DD_SLOT_UNLOCK, WINCE_Unlock)(data->dd_primary, NULL);
        hr = E_FAIL;
    }
    if (FAILED(hr)) {
        WINCE_DD_Fail(data, "lock", hr);
        if (data->dd_retries > 0) {
            data->dd_retries--;
            data->dd_tried = SDL_FALSE;
            data->dd_retry_at = SDL_GetTicks() + WINCE_DD_RETRY_MS;
        }
        return SDL_FALSE;
    }
    for (i = 0; i < numrects; ++i) {
        /* Clip to the client area and the screen. */
        int rx0 = SDL_max(rects[i].x, 0), ry0 = SDL_max(rects[i].y, 0);
        int rx1 = SDL_min(rects[i].x + rects[i].w, (int)client.right);
        int ry1 = SDL_min(rects[i].y + rects[i].h, (int)client.bottom);
        int y;
        rx0 = SDL_max(rx0, -origin.x);
        ry0 = SDL_max(ry0, -origin.y);
        rx1 = SDL_min(rx1, WINCE_DEFAULT_WIDTH - origin.x);
        ry1 = SDL_min(ry1, WINCE_DEFAULT_HEIGHT - origin.y);
        if (rx0 >= rx1 || ry0 >= ry1) {
            continue;
        }
        for (y = ry0; y < ry1; ++y) {
            SDL_memcpy((Uint8 *)desc.surface + (y + origin.y) * desc.pitch + (rx0 + origin.x) * 2,
                       (const Uint8 *)data->pixels + y * data->pitch + rx0 * 2,
                       (size_t)(rx1 - rx0) * 2);
        }
        x0 = SDL_min(x0, rx0 + origin.x);
        y0 = SDL_min(y0, ry0 + origin.y);
        x1 = SDL_max(x1, rx1 + origin.x);
        y1 = SDL_max(y1, ry1 + origin.y);
    }
    WINCE_VT(data->dd_primary, DD_SLOT_UNLOCK, WINCE_Unlock)(data->dd_primary, NULL);
    if (x0 < x1 && y0 < y1) {
        /* The display driver sends the bounding box of dirty tiles: touch two
           opposite corners of the box with 1x1 self-copies (no pixel moves). */
        HDC screen = GetDC(NULL);
        if (screen) {
            BitBlt(screen, x0, y0, 1, 1, screen, x0, y0, SRCCOPY);
            BitBlt(screen, x1 - 1, y1 - 1, 1, 1, screen, x1 - 1, y1 - 1, SRCCOPY);
            ReleaseDC(NULL, screen);
        }
    }
    return SDL_TRUE;
}

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
    data->pixels = (Uint16 *)*pixels;
    data->pitch = *pitch;
    data->dd_tried = SDL_FALSE;
    data->dd_retry_at = 0;
    data->dd_retries = WINCE_DD_MAX_RETRIES;
    return 0;
}

/* Frame counter: every 5 s of guest time, one log line with the presents per
   second over that window, the mean present cost and the path taken:
     wince: fps 23.9 frames 120 over 5016 ms, present 3.12 ms, path ddraw
   On by default (a line per 5 s is cheap); SDL_WINCE_FPS_LOG=0 turns it off. */
#define WINCE_FPS_WINDOW_MS 5000

static void WINCE_CountFrame(SDL_WindowData *data, Uint64 t0, Uint64 t1)
{
    const Uint64 freq = SDL_GetPerformanceFrequency();
    Uint64 elapsed_ms;

    if (!data->fps_start) {
        if (!SDL_GetHintBoolean("SDL_WINCE_FPS_LOG", SDL_TRUE)) {
            data->fps_start = (Uint64)-1; /* disabled */
            return;
        }
        data->fps_start = t0;
    }
    if (data->fps_start == (Uint64)-1) {
        return;
    }
    if (t1 < data->fps_start || t1 < t0 ||
        (t1 - data->fps_start) / freq > 4 * WINCE_FPS_WINDOW_MS / 1000) {
        /* The counter went backwards or jumped (seen once in CERF): drop the
           window instead of logging a bogus rate. */
        data->fps_start = t1;
        data->fps_frames = 0;
        data->fps_present = 0;
        return;
    }
    data->fps_frames++;
    data->fps_present += t1 - t0;
    elapsed_ms = (t1 - data->fps_start) * 1000 / freq;
    if (elapsed_ms >= WINCE_FPS_WINDOW_MS) {
        /* Fixed point: fps x10 and present ms x100, no float on CE. */
        const Uint32 fps10 = (Uint32)((Uint64)data->fps_frames * 10000 / elapsed_ms);
        const Uint32 pres100 = (Uint32)(data->fps_present * 100000 / freq / data->fps_frames);
        SDL_Log("wince: fps %u.%u frames %u over %u ms, present %u.%02u ms, path %s",
                fps10 / 10, fps10 % 10, data->fps_frames, (unsigned)elapsed_ms,
                pres100 / 100, pres100 % 100, data->fps_ddraw ? "ddraw" : "bitblt");
        data->fps_start = t1;
        data->fps_frames = 0;
        data->fps_present = 0;
    }
}

int WINCE_UpdateWindowFramebuffer(_THIS, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    const Uint64 t0 = SDL_GetPerformanceCounter();
    int i;

    data->fps_ddraw = WINCE_DD_Present(data, rects, numrects);
    if (!data->fps_ddraw) {
        for (i = 0; i < numrects; ++i) {
            BitBlt(data->hdc, rects[i].x, rects[i].y, rects[i].w, rects[i].h,
                   data->mdc, rects[i].x, rects[i].y, SRCCOPY);
        }
    }
    WINCE_CountFrame(data, t0, SDL_GetPerformanceCounter());
    return 0;
}

void WINCE_DestroyWindowFramebuffer(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return;
    }
    WINCE_DD_Close(data);
    data->dd_tried = SDL_FALSE;
    data->pixels = NULL;
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
