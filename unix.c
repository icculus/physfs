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

/* end of unix.c ... */

