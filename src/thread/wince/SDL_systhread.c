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

#ifdef SDL_THREAD_WINCE

/* Windows CE threads: CreateThread directly (no _beginthreadex; the CE CRT
   keeps no per-thread state that needs it). */

#include "SDL_thread.h"
#include "../SDL_thread_c.h"
#include "../SDL_systhread.h"
#include "SDL_systhread_c.h"

#ifndef STACK_SIZE_PARAM_IS_A_RESERVATION
#define STACK_SIZE_PARAM_IS_A_RESERVATION 0x00010000
#endif

static DWORD WINAPI RunThread(LPVOID data)
{
    SDL_RunThread((SDL_Thread *)data);
    return 0;
}

int SDL_SYS_CreateThread(SDL_Thread *thread)
{
    DWORD threadid = 0;
    const DWORD flags = thread->stacksize ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0;

    thread->handle = CreateThread(NULL, (DWORD)thread->stacksize, RunThread, thread, flags, &threadid);
    if (!thread->handle) {
        return SDL_SetError("Not enough resources to create thread");
    }
    return 0;
}

void SDL_SYS_SetupThread(const char *name)
{
    (void)name; /* CE has no thread names */
}

SDL_threadID SDL_ThreadID(void)
{
    return (SDL_threadID)GetCurrentThreadId();
}

int SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    int value;

    if (priority == SDL_THREAD_PRIORITY_LOW) {
        value = THREAD_PRIORITY_LOWEST;
    } else if (priority == SDL_THREAD_PRIORITY_HIGH) {
        value = THREAD_PRIORITY_HIGHEST;
    } else if (priority == SDL_THREAD_PRIORITY_TIME_CRITICAL) {
        value = THREAD_PRIORITY_TIME_CRITICAL;
    } else {
        value = THREAD_PRIORITY_NORMAL;
    }
    if (!SetThreadPriority(GetCurrentThread(), value)) {
        return WIN_SetError("SetThreadPriority()");
    }
    return 0;
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    CloseHandle(thread->handle);
}

#endif /* SDL_THREAD_WINCE */
