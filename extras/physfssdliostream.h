/*
 * This code provides a glue layer between PhysicsFS and Simple Directmedia
 *  Layer's (SDL) IOStream i/o abstraction.
 *
 * License: this code is public domain. I make no warranty that it is useful,
 *  correct, harmless, or environmentally safe.
 *
 * This particular file may be used however you like, including copying it
 *  verbatim into a closed-source project, exploiting it commercially, and
 *  removing any trace of my name from the source (although I hope you won't
 *  do that). I welcome enhancements and corrections to this file, but I do
 *  not require you to send me patches if you make changes. This code has
 *  NO WARRANTY.
 *
 * Unless otherwise stated, the rest of PhysicsFS falls under the zlib license.
 *  Please see LICENSE.txt in the root of the source tree.
 *
 * SDL 3 is zlib, like PhysicsFS.
 *  You can get SDL at https://www.libsdl.org/
 *
 *  This file was originally written by Ryan C. Gordon. (icculus@icculus.org)
 *  and then ported from SDL1/2's SDL_RWops to SDL3's SDL_IOStream.
 */

#ifndef _INCLUDE_PHYSFSSDLIOSTREAM_H_
#define _INCLUDE_PHYSFSSDLIOSTREAM_H_

#include "physfs.h"
#include "SDL3/SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open a platform-independent filename for reading, and make it accessible
 *  via an SDL_IOStream structure. The file will be closed in PhysicsFS when the
 *  IOStream is closed. PhysicsFS should be configured to your liking before
 *  opening files through this method.
 *
 *   @param filename File to open in platform-independent notation.
 *  @return A valid SDL_IOStream structure on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDLIOSTREAM_openRead(const char *fname);

/**
 * Open a platform-independent filename for writing, and make it accessible
 *  via an SDL_IOStream structure. The file will be closed in PhysicsFS when the
 *  IOStream is closed. PhysicsFS should be configured to your liking before
 *  opening files through this method.
 *
 *   @param filename File to open in platform-independent notation.
 *  @return A valid SDL_IOStream structure on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDLIOSTREAM_openWrite(const char *fname);

/**
 * Open a platform-independent filename for appending, and make it accessible
 *  via an SDL_IOStream structure. The file will be closed in PhysicsFS when the
 *  IOStream is closed. PhysicsFS should be configured to your liking before
 *  opening files through this method.
 *
 *   @param filename File to open in platform-independent notation.
 *  @return A valid SDL_IOStream structure on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDLIOSTREAM_openAppend(const char *fname);

/**
 * Make a SDL_IOStream from an existing PhysicsFS file handle. You should
 *  dispose of any references to the handle after successful creation of
 *  the IOStream. The actual PhysicsFS handle will be destroyed when the
 *  IOStream is closed.
 *
 *   @param handle a valid PhysicsFS file handle.
 *  @return A valid SDL_IOStream structure on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 */
PHYSFS_DECL SDL_IOStream *PHYSFSSDLIOSTREAM_makeIOStream(PHYSFS_File *handle);

#ifdef __cplusplus
}
#endif

#endif /* include-once blocker */

/* end of physfssdliostream.h ... */

