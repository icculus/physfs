/*
 * Skeleton platform-dependent support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


#error DO NOT COMPILE THIS. IT IS JUST A SKELETON EXAMPLE FILE.


const char *__PHYSFS_platformDirSeparator = ":";


int __PHYSFS_platformInit(void)
{
    return(1);  /* always succeed. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    return(1);  /* always succeed. */
} /* __PHYSFS_platformDeinit */


char **__PHYSFS_platformDetectAvailableCDs(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformGetUserName(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformGetUserDir */


PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    return(1);  /* single threaded. */
} /* __PHYSFS_platformGetThreadID */


int __PHYSFS_platformStricmp(const char *x, const char *y)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformExists(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformIsDirectory */


char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformCvtToDependent */


void __PHYSFS_platformTimeslice(void)
{
} /* __PHYSFS_platformTimeslice */


LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformEnumerateFiles */


char *__PHYSFS_platformCurrentDir(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformCurrentDir */


char *__PHYSFS_platformRealPath(const char *path)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformRealPath */


int __PHYSFS_platformMkDir(const char *path)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformMkDir */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformEOF(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformEOF */


int __PHYSFS_platformFlush(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformFlush */


int __PHYSFS_platformClose(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformDelete */


void *__PHYSFS_platformCreateMutex(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    /* not implemented, but can't call __PHYSFS_setError! */
    return(0);
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    /* not implemented, but can't call __PHYSFS_setError! */
} /* __PHYSFS_platformReleaseMutex */


PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformGetLastModTime */

/* end of skeleton.c ... */

