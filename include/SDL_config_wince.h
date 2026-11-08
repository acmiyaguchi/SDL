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

#ifndef SDL_config_wince_h_
#define SDL_config_wince_h_
#define SDL_config_h_

#include "SDL_platform.h"

/* Windows CE 6 (cegcc arm-mingw32ce). Static library only.
   M0: every subsystem uses its dummy/stub backend; later milestones switch
   them on here. C library functions are SDL's own (no HAVE_* for the CRT),
   since mingw32ce's runtime is incomplete. */

#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_LIMITS_H 1
#define HAVE_FLOAT_H 1

/* No GCC __sync atomics: ARMv5 has no ldrex/strex and libgcc has no
   __sync_*_4 helpers for this target. SDL_spinlock.c uses swp instead. */

#define SDL_AUDIO_DRIVER_DUMMY 1
#define SDL_JOYSTICK_DISABLED 1
#define SDL_HAPTIC_DISABLED 1
#define SDL_HIDAPI_DISABLED 1
#define SDL_SENSOR_DISABLED 1
#define SDL_LOADSO_DISABLED 1
#define SDL_THREADS_DISABLED 1
#define SDL_TIMERS_DISABLED 1
#define SDL_VIDEO_DRIVER_DUMMY 1
#define SDL_FILESYSTEM_DUMMY 1

#endif /* SDL_config_wince_h_ */
