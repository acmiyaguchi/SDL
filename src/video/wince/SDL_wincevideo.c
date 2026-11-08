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

/* Windows CE video driver: one top-level popup window per SDL window,
   pinned to (0,0) and covering the display, drawn through a 16-bit 565 DIB
   section (the native GWES format on the PW-SH4). No OpenGL. */

#include "SDL_video.h"
#include "SDL_mouse.h"
#include "SDL_syswm.h"
#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_touch_c.h"
#include "SDL_wincevideo.h"
#include "SDL_timer.h"

#define WINCE_CLASS_NAME L"SDL_app"

SDL_TouchID WINCE_touch_id = 1;
static ATOM wince_class;
static int display_w = WINCE_DEFAULT_WIDTH;
static int display_h = WINCE_DEFAULT_HEIGHT;

static int WINCE_VideoInit(_THIS)
{
    SDL_DisplayMode mode;
    WNDCLASSW wc;

    SDL_zero(wc);
    wc.lpfnWndProc = WINCE_WindowProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = WINCE_CLASS_NAME;
    wc.hbrBackground = NULL;
    wince_class = RegisterClassW(&wc);
    if (!wince_class) {
        return WIN_SetError("Couldn't register window class");
    }

    SDL_zero(mode);
    mode.format = SDL_PIXELFORMAT_RGB565;
    mode.w = display_w;
    mode.h = display_h;
    mode.refresh_rate = 60;
    if (SDL_AddBasicVideoDisplay(&mode) < 0) {
        return -1;
    }
    SDL_AddTouch(WINCE_touch_id, SDL_TOUCH_DEVICE_DIRECT, "touchscreen");
    return 0;
}

static void WINCE_VideoQuit(_THIS)
{
    if (wince_class) {
        UnregisterClassW(WINCE_CLASS_NAME, GetModuleHandleW(NULL));
        wince_class = 0;
    }
}

static int WINCE_GetDisplayBounds(_THIS, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    rect->x = 0;
    rect->y = 0;
    rect->w = display->current_mode.w;
    rect->h = display->current_mode.h;
    return 0;
}

static void WINCE_GetDisplayModes(_THIS, SDL_VideoDisplay *display)
{
    SDL_DisplayMode mode = display->current_mode;
    SDL_AddDisplayMode(display, &mode);
}

static int WINCE_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0; /* only one mode */
}

/* Resize our view of the display when the client area tells us otherwise. */
static void WINCE_UpdateDisplaySize(int w, int h)
{
    SDL_VideoDisplay *display = SDL_GetDisplay(0);
    if (display && (display->current_mode.w != w || display->current_mode.h != h)) {
        display->current_mode.w = w;
        display->current_mode.h = h;
        display->desktop_mode.w = w;
        display->desktop_mode.h = h;
        display_w = w;
        display_h = h;
    }
}

static int WINCE_CreateWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data;
    WCHAR *title;
    RECT rc;
    int w, h;

    data = (SDL_WindowData *)SDL_calloc(1, sizeof(*data));
    if (!data) {
        return SDL_OutOfMemory();
    }
    data->window = window;
    window->driverdata = data;

    title = WINCE_UTF8ToWide(window->title ? window->title : "");
    /* Always the whole display: CE apps are full screen. */
    data->hwnd = CreateWindowExW(0, WINCE_CLASS_NAME, title ? title : L"", WS_POPUP,
                                 0, 0, display_w, display_h, NULL, NULL, GetModuleHandleW(NULL), NULL);
    SDL_free(title);
    if (!data->hwnd) {
        SDL_free(data);
        window->driverdata = NULL;
        return WIN_SetError("Couldn't create window");
    }
    SetWindowLongW(data->hwnd, GWL_USERDATA, (LONG)data);
    data->hdc = GetDC(data->hwnd);

    GetClientRect(data->hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (w > 0 && h > 0) {
        WINCE_UpdateDisplaySize(w, h);
    } else {
        w = display_w;
        h = display_h;
    }
    window->x = 0;
    window->y = 0;
    window->w = w;
    window->h = h;
    window->flags |= SDL_WINDOW_BORDERLESS;
    window->flags &= ~SDL_WINDOW_RESIZABLE;
    return 0;
}

static void WINCE_SetWindowTitle(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    WCHAR *title = WINCE_UTF8ToWide(window->title ? window->title : "");
    if (title) {
        SetWindowTextW(data->hwnd, title);
        SDL_free(title);
    }
}

/* Position and size requests are ignored: the window always covers the
   display. Report the real size back so SDL keeps the right surface. */
static void WINCE_FixWindowGeometry(SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    RECT rc;
    GetClientRect(data->hwnd, &rc);
    if (window->x != 0 || window->y != 0) {
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_MOVED, 0, 0);
    }
    if (window->w != rc.right || window->h != rc.bottom) {
        SDL_SendWindowEvent(window, SDL_WINDOWEVENT_RESIZED, rc.right, rc.bottom);
    }
}

static void WINCE_SetWindowPosition(_THIS, SDL_Window *window)
{
    WINCE_FixWindowGeometry(window);
}

static void WINCE_SetWindowSize(_THIS, SDL_Window *window)
{
    WINCE_FixWindowGeometry(window);
}

static void WINCE_ShowWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    SetWindowPos(data->hwnd, HWND_TOPMOST, 0, 0, display_w, display_h, SWP_SHOWWINDOW);
    ShowWindow(data->hwnd, SW_SHOW);
    data->shown_ticks = SDL_GetTicks();
    data->reclaimed = SDL_FALSE;
    SetForegroundWindow(data->hwnd);
    SetFocus(data->hwnd);
    WINCE_FixWindowGeometry(window);
}

static void WINCE_HideWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    ShowWindow(data->hwnd, SW_HIDE);
}

static void WINCE_RaiseWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    SetForegroundWindow(data->hwnd);
    SetFocus(data->hwnd);
}

static void WINCE_SetWindowFullscreen(_THIS, SDL_Window *window, SDL_VideoDisplay *display, SDL_bool fullscreen)
{
    WINCE_FixWindowGeometry(window); /* always full screen */
}

static void WINCE_DestroyWindow(_THIS, SDL_Window *window)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (!data) {
        return;
    }
    WINCE_DestroyWindowFramebuffer(_this, window);
    if (data->hdc) {
        ReleaseDC(data->hwnd, data->hdc);
    }
    if (data->hwnd) {
        SetWindowLongW(data->hwnd, GWL_USERDATA, 0);
        DestroyWindow(data->hwnd);
    }
    SDL_free(data);
    window->driverdata = NULL;
}

static SDL_bool WINCE_GetWindowWMInfo(_THIS, SDL_Window *window, struct SDL_SysWMinfo *info)
{
    SDL_WindowData *data = (SDL_WindowData *)window->driverdata;
    if (info->version.major > SDL_MAJOR_VERSION) {
        SDL_SetError("Application not compiled with SDL %d", SDL_MAJOR_VERSION);
        return SDL_FALSE;
    }
    info->subsystem = SDL_SYSWM_WINDOWS;
    info->info.win.window = data->hwnd;
    info->info.win.hdc = data->hdc;
    info->info.win.hinstance = GetModuleHandleW(NULL);
    return SDL_TRUE;
}

static void WINCE_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *WINCE_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }
    device->VideoInit = WINCE_VideoInit;
    device->VideoQuit = WINCE_VideoQuit;
    device->GetDisplayBounds = WINCE_GetDisplayBounds;
    device->GetDisplayUsableBounds = WINCE_GetDisplayBounds;
    device->GetDisplayModes = WINCE_GetDisplayModes;
    device->SetDisplayMode = WINCE_SetDisplayMode;
    device->PumpEvents = WINCE_PumpEvents;
    device->CreateSDLWindow = WINCE_CreateWindow;
    device->SetWindowTitle = WINCE_SetWindowTitle;
    device->SetWindowPosition = WINCE_SetWindowPosition;
    device->SetWindowSize = WINCE_SetWindowSize;
    device->ShowWindow = WINCE_ShowWindow;
    device->HideWindow = WINCE_HideWindow;
    device->RaiseWindow = WINCE_RaiseWindow;
    device->SetWindowFullscreen = WINCE_SetWindowFullscreen;
    device->DestroyWindow = WINCE_DestroyWindow;
    device->GetWindowWMInfo = WINCE_GetWindowWMInfo;
    device->CreateWindowFramebuffer = WINCE_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = WINCE_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = WINCE_DestroyWindowFramebuffer;
    device->free = WINCE_DeleteDevice;
    return device;
}

VideoBootStrap WINCE_bootstrap = {
    "wince", "SDL Windows CE video driver", WINCE_CreateDevice,
    NULL /* no ShowMessageBox implementation yet */
};

#endif /* SDL_VIDEO_DRIVER_WINCE */
