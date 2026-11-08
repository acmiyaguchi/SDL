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

#ifdef SDL_TIMER_WINCE

/* Windows CE ticks: QueryPerformanceCounter when the OAL provides it (the
   PW-SH4 reports 12 MHz), otherwise GetTickCount. No timeBeginPeriod. */

#include "../../core/wince/SDL_wince.h"
#include "SDL_timer.h"

static SDL_bool ticks_started = SDL_FALSE;
static SDL_bool use_qpc = SDL_FALSE;
static LARGE_INTEGER start_ticks;
static LARGE_INTEGER ticks_per_second;
static DWORD start_tickcount;
/* GetTickCount wraps after ~49.7 days; extend it to 64 bits. */
static DWORD last_tickcount;
static Uint64 tickcount_high;

static Uint64 WINCE_TickCount64(void)
{
    DWORD now = GetTickCount();
    if (now < last_tickcount) {
        tickcount_high += ((Uint64)1) << 32;
    }
    last_tickcount = now;
    return tickcount_high | now;
}

void SDL_TicksInit(void)
{
    if (ticks_started) {
        return;
    }
    ticks_started = SDL_TRUE;
    use_qpc = (QueryPerformanceFrequency(&ticks_per_second) && ticks_per_second.QuadPart > 1000 &&
               QueryPerformanceCounter(&start_ticks)) ? SDL_TRUE : SDL_FALSE;
    if (!use_qpc) {
        ticks_per_second.QuadPart = 1000;
    }
    last_tickcount = 0;
    tickcount_high = 0;
    start_tickcount = GetTickCount();
    last_tickcount = start_tickcount;
}

void SDL_TicksQuit(void)
{
    ticks_started = SDL_FALSE;
}

Uint64 SDL_GetTicks64(void)
{
    if (!ticks_started) {
        SDL_TicksInit();
    }
    if (use_qpc) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (Uint64)(((now.QuadPart - start_ticks.QuadPart) * 1000) / ticks_per_second.QuadPart);
    }
    return WINCE_TickCount64() - start_tickcount;
}

Uint64 SDL_GetPerformanceCounter(void)
{
    if (!ticks_started) {
        SDL_TicksInit();
    }
    if (use_qpc) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return (Uint64)counter.QuadPart;
    }
    return WINCE_TickCount64();
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    if (!ticks_started) {
        SDL_TicksInit();
    }
    return (Uint64)ticks_per_second.QuadPart;
}

void SDL_Delay(Uint32 ms)
{
    Sleep(ms);
}

#endif /* SDL_TIMER_WINCE */
