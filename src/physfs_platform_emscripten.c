/*
 * Emscripten support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_EMSCRIPTEN

#include "physfs_internal.h"

int __PHYSFS_platformInit(const char *argv0)
{
    return 1;  /* always succeed. */
} /* __PHYSFS_platformInit */


void __PHYSFS_platformDeinit(void)
{
    /* no-op */
} /* __PHYSFS_platformDeinit */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    /* never CD-ROMs on Emscripten. */
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    return __PHYSFS_strdup("/");
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    #ifndef PHYSFS_EMSCRIPTEN_STORAGE_PATH
    /* historically, this is where Emscripten chose by default, since it was
       using the Unix codepath, so we'll keep it. */
    #define PHYSFS_EMSCRIPTEN_STORAGE_PATH "/home/web_user/.local/share"
    #endif

    const char *deflt = PHYSFS_EMSCRIPTEN_STORAGE_PATH;
    const char *envr = getenv("XDG_DATA_HOME"); /* check this just in case the app overrode it when we used the Unix code... */
    const char *base = envr ? envr : deflt;
    const size_t len = strlen(base) + strlen(app) + 3;
    char *retval = (char *) allocator.Malloc(len);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    snprintf(retval, len, "%s/%s/", base, app);
    return retval;
} /* __PHYSFS_platformCalcPrefDir */

#endif /* PHYSFS_PLATFORM_EMSCRIPTEN */

/* end of physfs_platform_emscripten.c ... */

