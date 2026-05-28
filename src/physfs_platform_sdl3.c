/*
 * SDL3 support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Rob Loach (@RobLoach).
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#ifdef PHYSFS_PLATFORM_SDL3

#include <SDL3/SDL.h>

/**
 * Context for enumeration.
 *
 * @see __PHYSFS_platformEnumerate()
 */
typedef struct platformEnumerateContext
{
    PHYSFS_EnumerateCallback callback;
    const char *origdir;
    void *callbackdata;
    PHYSFS_EnumerateCallbackResult result;
} platformEnumerateContext;

int __PHYSFS_platformInit(const char *argv0)
{
    (void)argv0;
    return 1; /* always succeed. */
} /* __PHYSFS_platformInit */

void __PHYSFS_platformDeinit(void)
{
    /* no-op */
} /* __PHYSFS_platformDeinit */

void *__PHYSFS_platformGetThreadID(void)
{
    return (void *)(size_t)SDL_GetCurrentThreadID();
} /* __PHYSFS_platformGetThreadID */

void *__PHYSFS_platformCreateMutex(void)
{
    return (void *)SDL_CreateMutex();
} /* __PHYSFS_platformCreateMutex */

void __PHYSFS_platformDestroyMutex(void *mutex)
{
    SDL_DestroyMutex((SDL_Mutex *)mutex);
} /* __PHYSFS_platformDestroyMutex */

int __PHYSFS_platformGrabMutex(void *mutex)
{
    SDL_LockMutex((SDL_Mutex *)mutex);
    return 1;
} /* __PHYSFS_platformGrabMutex */

void __PHYSFS_platformReleaseMutex(void *mutex)
{
    SDL_UnlockMutex((SDL_Mutex *)mutex);
} /* __PHYSFS_platformReleaseMutex */

/**
 * Not supported by SDL3.
 */
void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    (void)cb;
    (void)data;
} /* __PHYSFS_platformDetectAvailableCDs */

char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    const char *base = SDL_GetBasePath();
    (void)argv0;
    BAIL_IF(base == NULL, PHYSFS_ERR_OS_ERROR, NULL);
    return __PHYSFS_strdup(base);
} /* __PHYSFS_platformCalcBaseDir */

char *__PHYSFS_platformCalcUserDir(void)
{
    const char *home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    BAIL_IF(home == NULL, PHYSFS_ERR_OS_ERROR, NULL);
    return __PHYSFS_strdup(home);
} /* __PHYSFS_platformCalcUserDir */

char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    char *out = SDL_GetPrefPath(org, app);
    BAIL_IF(out == NULL, PHYSFS_ERR_OS_ERROR, NULL);
    /* Unlike SDL_GetBasePath() or SDL_GetUserFolder(), SDL_GetPrefPath() allocates the string for us. */
    return out;
} /* __PHYSFS_platformCalcPrefDir */

static SDL_EnumerationResult SDLCALL platformEnumerateCallback(void *userdata,
                                                               const char *dirname,
                                                               const char *fname)
{
    platformEnumerateContext *ctx = (platformEnumerateContext *)userdata;
    (void)dirname;

    ctx->result = ctx->callback(ctx->callbackdata, ctx->origdir, fname);

    switch (ctx->result) {
        case PHYSFS_ENUM_OK:    return SDL_ENUM_CONTINUE;
        case PHYSFS_ENUM_STOP:  return SDL_ENUM_SUCCESS;
        case PHYSFS_ENUM_ERROR: return SDL_ENUM_FAILURE;
    }
    return SDL_ENUM_FAILURE;
} /* platformEnumerateCallback */

PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                                                          PHYSFS_EnumerateCallback callback,
                                                          const char *origdir,
                                                          void *callbackdata)
{
    platformEnumerateContext ctx;
    ctx.callback = callback;
    ctx.origdir = origdir;
    ctx.callbackdata = callbackdata;
    ctx.result = PHYSFS_ENUM_OK;

    BAIL_IF(!SDL_EnumerateDirectory(dirname, platformEnumerateCallback, &ctx),
            /* Determine the correct PhysFS error to report. */
            ctx.result == PHYSFS_ENUM_ERROR ? PHYSFS_ERR_APP_CALLBACK : PHYSFS_ERR_OS_ERROR,
            PHYSFS_ENUM_ERROR);

    return ctx.result;
} /* __PHYSFS_platformEnumerate */

int __PHYSFS_platformMkDir(const char *path)
{
    BAIL_IF(!SDL_CreateDirectory(path), PHYSFS_ERR_OS_ERROR, 0);
    return 1;
} /* __PHYSFS_platformMkDir */

int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF(!SDL_RemovePath(path), PHYSFS_ERR_OS_ERROR, 0);
    return 1;
} /* __PHYSFS_platformDelete */

int __PHYSFS_platformStat(const char *fn, PHYSFS_Stat *stat, const int follow)
{
    SDL_PathInfo info;
    (void)follow;

    BAIL_IF(!SDL_GetPathInfo(fn, &info), PHYSFS_ERR_NOT_FOUND, 0);

    switch (info.type) {
        case SDL_PATHTYPE_FILE:
            stat->filetype = PHYSFS_FILETYPE_REGULAR;
            stat->filesize = (PHYSFS_sint64)info.size;
            break;
        case SDL_PATHTYPE_DIRECTORY:
            stat->filetype = PHYSFS_FILETYPE_DIRECTORY;
            stat->filesize = 0;
            break;
        case SDL_PATHTYPE_OTHER:
            stat->filetype = PHYSFS_FILETYPE_OTHER;
            stat->filesize = -1;
            break;
        case SDL_PATHTYPE_NONE:
        default:
            BAIL(PHYSFS_ERR_NOT_FOUND, 0);
    }

    stat->accesstime = (PHYSFS_sint64)(info.access_time / SDL_NS_PER_SECOND);
    stat->createtime = (PHYSFS_sint64)(info.create_time / SDL_NS_PER_SECOND);
    stat->modtime = (PHYSFS_sint64)(info.modify_time / SDL_NS_PER_SECOND);
    stat->readonly = 0;
    return 1;
} /* __PHYSFS_platformStat */

static void *doOpen(const char *filename, const char* mode)
{
    SDL_IOStream *io = SDL_IOFromFile(filename, mode);
    BAIL_IF(io == NULL, PHYSFS_ERR_OS_ERROR, NULL);
    return io;
} /* doOpen */

void *__PHYSFS_platformOpenRead(const char *filename)
{
    return doOpen(filename, "rb");
}  /* __PHYSFS_platformOpenRead */

void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return doOpen(filename, "wb");
} /* __PHYSFS_platformOpenWrite */

void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return doOpen(filename, "ab");
} /* __PHYSFS_platformOpenAppend */

PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buf, PHYSFS_uint64 len)
{
    SDL_IOStream *io = (SDL_IOStream *)opaque;
    size_t size = (size_t)len;
    size_t rc = SDL_ReadIO(io, buf, size);
    BAIL_IF(rc < size && SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR, PHYSFS_ERR_IO, -1);
    return (PHYSFS_sint64)rc;
} /* __PHYSFS_platformRead */

PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buf, PHYSFS_uint64 len)
{
    SDL_IOStream *io = (SDL_IOStream *)opaque;
    size_t size = (size_t)len;
    size_t rc = SDL_WriteIO(io, buf, size);
    BAIL_IF(rc < size && SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR, PHYSFS_ERR_IO, -1);
    return (PHYSFS_sint64)rc;
} /* __PHYSFS_platformWrite */

int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    BAIL_IF(SDL_SeekIO((SDL_IOStream *)opaque, (Sint64)pos, SDL_IO_SEEK_SET) < 0, PHYSFS_ERR_IO, 0);
    return 1;
} /* __PHYSFS_platformSeek */

PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    Sint64 pos = SDL_TellIO((SDL_IOStream *)opaque);
    BAIL_IF(pos < 0, PHYSFS_ERR_IO, -1);
    return (PHYSFS_sint64)pos;
} /* __PHYSFS_platformTell */

PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    Sint64 size = SDL_GetIOSize((SDL_IOStream *)opaque);
    BAIL_IF(size < 0, PHYSFS_ERR_IO, -1);
    return (PHYSFS_sint64)size;
} /* __PHYSFS_platformFileLength */

int __PHYSFS_platformFlush(void *opaque)
{
    BAIL_IF(!SDL_FlushIO((SDL_IOStream *)opaque), PHYSFS_ERR_IO, 0);
    return 1;
} /* __PHYSFS_platformFlush */

void __PHYSFS_platformClose(void *opaque)
{
    if (!SDL_CloseIO((SDL_IOStream *)opaque)) {
        SDL_ClearError();
    }
} /* __PHYSFS_platformClose */

#endif
