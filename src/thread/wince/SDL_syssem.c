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

/* Counting semaphores on CE kernel semaphores. The count is tracked with
   interlocked operations so SDL_SemValue works (CE has no query call). */

#include "SDL_thread.h"
#include "SDL_timer.h"
#include "../../core/wince/SDL_wince.h"

struct SDL_semaphore
{
    HANDLE id;
    LONG count;
};

SDL_sem *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_sem *sem = (SDL_sem *)SDL_malloc(sizeof(*sem));
    if (!sem) {
        SDL_OutOfMemory();
        return NULL;
    }
    sem->id = CreateSemaphoreW(NULL, (LONG)initial_value, 0x7FFFFFFF, NULL);
    sem->count = (LONG)initial_value;
    if (!sem->id) {
        SDL_SetError("Couldn't create semaphore");
        SDL_free(sem);
        return NULL;
    }
    return sem;
}

void SDL_DestroySemaphore(SDL_sem *sem)
{
    if (sem) {
        if (sem->id) {
            CloseHandle(sem->id);
        }
        SDL_free(sem);
    }
}

int SDL_SemWaitTimeout(SDL_sem *sem, Uint32 timeout)
{
    DWORD dwMilliseconds;

    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    dwMilliseconds = (timeout == SDL_MUTEX_MAXWAIT) ? INFINITE : (DWORD)timeout;
    switch (WaitForSingleObject(sem->id, dwMilliseconds)) {
    case WAIT_OBJECT_0:
        InterlockedDecrement(&sem->count);
        return 0;
    case WAIT_TIMEOUT:
        return SDL_MUTEX_TIMEDOUT;
    default:
        return SDL_SetError("WaitForSingleObject() failed");
    }
}

int SDL_SemTryWait(SDL_sem *sem)
{
    return SDL_SemWaitTimeout(sem, 0);
}

int SDL_SemWait(SDL_sem *sem)
{
    return SDL_SemWaitTimeout(sem, SDL_MUTEX_MAXWAIT);
}

Uint32 SDL_SemValue(SDL_sem *sem)
{
    if (!sem) {
        SDL_InvalidParamError("sem");
        return 0;
    }
    return (Uint32)sem->count;
}

int SDL_SemPost(SDL_sem *sem)
{
    if (!sem) {
        return SDL_InvalidParamError("sem");
    }
    /* Increase before release so a woken waiter never sees a negative count. */
    InterlockedIncrement(&sem->count);
    if (ReleaseSemaphore(sem->id, 1, NULL) == FALSE) {
        InterlockedDecrement(&sem->count);
        return SDL_SetError("ReleaseSemaphore() failed");
    }
    return 0;
}

#endif /* SDL_THREAD_WINCE */
