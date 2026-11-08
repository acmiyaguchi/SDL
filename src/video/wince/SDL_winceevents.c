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

/* Windows CE input and window messages.

   The stylus is reported as a single-finger direct touch device only;
   SDL synthesises mouse events from it (SDL_HINT_TOUCH_MOUSE_EVENTS), so
   apps that handle both touch and mouse do not see doubled clicks.
   Keys: CE virtual keys -> SDL scancodes; WM_CHAR -> text input. */

#include "SDL_wincevideo.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"
#include "../../events/SDL_windowevents_c.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

SDL_Scancode WINCE_VKToScancode(WPARAM vk)
{
    if (vk >= 'A' && vk <= 'Z') {
        return (SDL_Scancode)(SDL_SCANCODE_A + (vk - 'A'));
    }
    if (vk >= '1' && vk <= '9') {
        return (SDL_Scancode)(SDL_SCANCODE_1 + (vk - '1'));
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return (SDL_Scancode)(SDL_SCANCODE_F1 + (vk - VK_F1));
    }
    if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) {
        return (SDL_Scancode)(SDL_SCANCODE_KP_1 + (vk - VK_NUMPAD1));
    }
    switch (vk) {
    case '0': return SDL_SCANCODE_0;
    case VK_NUMPAD0: return SDL_SCANCODE_KP_0;
    case VK_RETURN: return SDL_SCANCODE_RETURN;
    case VK_ESCAPE: return SDL_SCANCODE_ESCAPE;
    case VK_BACK: return SDL_SCANCODE_BACKSPACE;
    case VK_TAB: return SDL_SCANCODE_TAB;
    case VK_SPACE: return SDL_SCANCODE_SPACE;
    case VK_CAPITAL: return SDL_SCANCODE_CAPSLOCK;
    case VK_SHIFT: return SDL_SCANCODE_LSHIFT;
    case VK_CONTROL: return SDL_SCANCODE_LCTRL;
    case VK_MENU: return SDL_SCANCODE_LALT;
    case VK_PRIOR: return SDL_SCANCODE_PAGEUP;
    case VK_NEXT: return SDL_SCANCODE_PAGEDOWN;
    case VK_END: return SDL_SCANCODE_END;
    case VK_HOME: return SDL_SCANCODE_HOME;
    case VK_LEFT: return SDL_SCANCODE_LEFT;
    case VK_UP: return SDL_SCANCODE_UP;
    case VK_RIGHT: return SDL_SCANCODE_RIGHT;
    case VK_DOWN: return SDL_SCANCODE_DOWN;
    case VK_INSERT: return SDL_SCANCODE_INSERT;
    case VK_DELETE: return SDL_SCANCODE_DELETE;
    case 0xBA: return SDL_SCANCODE_SEMICOLON;  /* VK_OEM_1 */
    case 0xBB: return SDL_SCANCODE_EQUALS;     /* VK_OEM_PLUS */
    case 0xBC: return SDL_SCANCODE_COMMA;      /* VK_OEM_COMMA */
    case 0xBD: return SDL_SCANCODE_MINUS;      /* VK_OEM_MINUS */
    case 0xBE: return SDL_SCANCODE_PERIOD;     /* VK_OEM_PERIOD */
    case 0xBF: return SDL_SCANCODE_SLASH;      /* VK_OEM_2 */
    case 0xC0: return SDL_SCANCODE_GRAVE;      /* VK_OEM_3 */
    case 0xDB: return SDL_SCANCODE_LEFTBRACKET;
    case 0xDC: return SDL_SCANCODE_BACKSLASH;
    case 0xDD: return SDL_SCANCODE_RIGHTBRACKET;
    case 0xDE: return SDL_SCANCODE_APOSTROPHE;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

static SDL_WindowData *WINCE_GetData(HWND hwnd)
{
    return (SDL_WindowData *)GetWindowLongW(hwnd, GWL_USERDATA);
}

static void WINCE_SendPen(SDL_WindowData *data, UINT msg, LPARAM lParam)
{
    SDL_Window *window = data->window;
    float x, y;
    int px = GET_X_LPARAM(lParam), py = GET_Y_LPARAM(lParam);

    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= window->w) px = window->w - 1;
    if (py >= window->h) py = window->h - 1;
    x = (window->w > 1) ? (float)px / (float)(window->w - 1) : 0.0f;
    y = (window->h > 1) ? (float)py / (float)(window->h - 1) : 0.0f;

    switch (msg) {
    case WM_LBUTTONDOWN:
        SetCapture(data->hwnd);
        data->pen_down = SDL_TRUE;
        SDL_SendTouch(WINCE_touch_id, 0, window, SDL_TRUE, x, y, 1.0f);
        break;
    case WM_MOUSEMOVE:
        if (data->pen_down) {
            SDL_SendTouchMotion(WINCE_touch_id, 0, window, x, y, 1.0f);
        }
        break;
    case WM_LBUTTONUP:
        if (data->pen_down) {
            data->pen_down = SDL_FALSE;
            ReleaseCapture();
            SDL_SendTouch(WINCE_touch_id, 0, window, SDL_FALSE, x, y, 1.0f);
        }
        break;
    }
}

LRESULT CALLBACK WINCE_WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SDL_WindowData *data = WINCE_GetData(hwnd);

    if (!data) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
        WINCE_SendPen(data, msg, lParam);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        SDL_SendKeyboardKey(SDL_PRESSED, WINCE_VKToScancode(wParam));
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        SDL_SendKeyboardKey(SDL_RELEASED, WINCE_VKToScancode(wParam));
        return 0;

    case WM_CHAR:
        if (wParam >= 0x20 && wParam != 0x7F) {
            WCHAR w[2];
            char *text;
            w[0] = (WCHAR)wParam;
            w[1] = 0;
            text = WINCE_WideToUTF8(w);
            if (text) {
                SDL_SendKeyboardText(text);
                SDL_free(text);
            }
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE) {
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_SHOWN, 0, 0);
            SDL_SetKeyboardFocus(data->window);
            SDL_SetMouseFocus(data->window);
        } else {
            if (SDL_GetKeyboardFocus() == data->window) {
                SDL_SetKeyboardFocus(NULL);
            }
            SDL_ResetKeyboard();
        }
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        if (w > 0 && h > 0) {
            SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_RESIZED, w, h);
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (data->mdc) {
            BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
                   ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
                   data->mdc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
        }
        EndPaint(hwnd, &ps);
        SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_EXPOSED, 0, 0);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        SDL_SendWindowEvent(data->window, SDL_WINDOWEVENT_CLOSE, 0, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WINCE_PumpEvents(_THIS)
{
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

#endif /* SDL_VIDEO_DRIVER_WINCE */
