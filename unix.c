/*
 * Unix support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


const char *__PHYSFS_PlatformDirSeparator = "/";

char **__PHYSFS_platformDetectAvailableCDs(void)
{
} /* __PHYSFS_detectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(char *argv0)
{
    return(NULL);
} /* __PHYSFS_platformCalcBaseDir */


int __PHYSFS_platformGetThreadID(void)
{
    return((int) pthread_self());
} /* __PHYSFS_platformGetThreadID */


int __PHYSFS_platformStricmp(const char *str1, const char *str2)
{
    return(strcasecmp(str1, str2));
} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformIsSymlink(const char *fname)
{
} /* __PHYSFS_platformIsSymlink */


char *__PHYSFS_platformGetUserName(void)
{
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void);
{
} /* __PHYSFS_platformGetUserDir */


/* end of unix.c ... */

