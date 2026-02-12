/*
 * PS Vita support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon and Ezekiel Bethel.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_VITA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <psp2/kernel/error.h>
#include <psp2/io/dirent.h>
#include <psp2/io/stat.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/rtc.h>
#include <sys/errno.h>

#include "physfs_internal.h"


static PHYSFS_ErrorCode errcodeFromRet(const int err)
{
    switch (err & 0xFF)
    {
        case 0: return PHYSFS_ERR_OK;
        case EACCES: return PHYSFS_ERR_PERMISSION;
        case EPERM: return PHYSFS_ERR_PERMISSION;
        case EDQUOT: return PHYSFS_ERR_NO_SPACE;
        case EIO: return PHYSFS_ERR_IO;
        case ELOOP: return PHYSFS_ERR_SYMLINK_LOOP;
        case EMLINK: return PHYSFS_ERR_NO_SPACE;
        case ENAMETOOLONG: return PHYSFS_ERR_BAD_FILENAME;
        case ENOENT: return PHYSFS_ERR_NOT_FOUND;
        case ENOSPC: return PHYSFS_ERR_NO_SPACE;
        case ENOTDIR: return PHYSFS_ERR_NOT_FOUND;
        case EISDIR: return PHYSFS_ERR_NOT_A_FILE;
        case EROFS: return PHYSFS_ERR_READ_ONLY;
        case ETXTBSY: return PHYSFS_ERR_BUSY;
        case EBUSY: return PHYSFS_ERR_BUSY;
        case ENOMEM: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ENOTEMPTY: return PHYSFS_ERR_DIR_NOT_EMPTY;
        default: return PHYSFS_ERR_OS_ERROR;
    } /* switch */
} /* errcodeFromErrnoError */

int __PHYSFS_platformInit(const char *argv0)
{
    return 1;  /* always succeed. */
} /* __PHYSFS_platformInit */

void __PHYSFS_platformDeinit(void)
{
    /* no-op. */
} /* __PHYSFS_platformDeinit */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    char *ret = allocator.Malloc(37);
    BAIL_IF(!ret, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    if (ret != NULL)
        strcpy(ret, "app0:/");
    return ret;
}

char *__PHYSFS_platformCalcUserDir()
{
    char *ret = allocator.Malloc(37);
    BAIL_IF(!ret, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    if (ret != NULL)
        strcpy(ret, "ux0:/data/");
    return ret;
}


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    const char *envr = "ux0:/data/";
    char *retval = NULL;
    char *ptr = NULL;
    size_t len = 0;

    BAIL_IF(!app, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    if (!org) {
        org = "";
    }

    len = strlen(envr);

    len += strlen(org) + strlen(app) + 3;
    retval = (char *) allocator.Malloc(len);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    if (*org) {
        snprintf(retval, len, "%s%s/%s/", envr, org, app);
    } else {
        snprintf(retval, len, "%s%s/", envr, app);
    }

    for (ptr = retval + 1; *ptr; ptr++) {
        if (*ptr == '/') {
            *ptr = '\0';
            sceIoMkdir(retval, 0777);
            *ptr = '/';
        }
    }
    sceIoMkdir(retval, 0777);

    return retval;
} /* __PHYSFS_platformCalcUserDir */

typedef struct SymlinkFilterData_
{
    PHYSFS_EnumerateCallback callback;
    void *callbackData;
    void *dirhandle;
    const char *arcfname;
    PHYSFS_ErrorCode errcode;
} SymlinkFilterData_;

PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                               PHYSFS_EnumerateCallback callback,
                               const char *origdir, void *callbackdata)
{
    SceUID dir;
    SceIoDirent ent;
    PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;

    SymlinkFilterData_ * cbdata = (SymlinkFilterData_*)callbackdata;

    dir = sceIoDopen(dirname);
    BAIL_IF(dir < 0, errcodeFromRet(dir), PHYSFS_ENUM_ERROR);

    while ((retval == PHYSFS_ENUM_OK) && ((sceIoDread(dir, &ent)) > 0))
    {
        const char *name = ent.d_name;
        if (name[0] == '.')  /* ignore "." and ".." */
        {
            if ((name[1] == '\0') || ((strlen(name) > 2) && (name[1] == '.') && (name[2] == '\0')))
                continue;
        } /* if */

        retval = callback(callbackdata, origdir, name);
        if (retval == PHYSFS_ENUM_ERROR)
            PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
    } /* while */

    sceIoDclose(dir);

    return retval;
} /* __PHYSFS_platformEnumerate */


int __PHYSFS_platformMkDir(const char *path)
{
    const int rc = sceIoMkdir(path, 0666);
    BAIL_IF(rc < 0, errcodeFromRet(rc), 0);
    return 1;
} /* __PHYSFS_platformMkDir */


static void *doOpen(const char *filename, int mode)
{
    const int appending = (mode & SCE_O_APPEND);
    SceUID fd;
    int *retval;

    /* O_APPEND doesn't actually behave as we'd like. */
    mode &= ~SCE_O_APPEND;

    fd = sceIoOpen(filename, mode, 0666);
    BAIL_IF(fd < 0, errcodeFromRet(fd), NULL);

    if (appending)
    {
        if (sceIoLseek(fd, 0, SEEK_END) < 0)
        {
            sceIoClose(fd);
            BAIL(PHYSFS_ERR_IO, NULL);
        } /* if */
    } /* if */

    retval = (SceUID *) allocator.Malloc(sizeof (SceUID));
    if (!retval)
    {
        sceIoClose(fd);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    *retval = fd;
    return ((void *) retval);
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return doOpen(filename, SCE_O_RDONLY);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return doOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return doOpen(filename, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint64 len)
{
    SceUID fd = *((int *) opaque);
    SceSSize rc = sceIoRead(fd, buffer, len);

    BAIL_IF(rc < 0, errcodeFromRet(rc), rc);
    assert(rc <= len);

    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    SceUID fd = *((int *) opaque);
    SceSSize rc = sceIoWrite(fd, (void *) buffer, len);

    BAIL_IF(rc < 0, errcodeFromRet(rc), rc);
    assert(rc <= len);

    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    SceUID fd = *((int *) opaque);
    PHYSFS_sint64 retval;
    retval = (PHYSFS_sint64) sceIoLseek(fd, (SceOff)pos, SCE_SEEK_SET);
    BAIL_IF(retval < 0, errcodeFromRet(retval), 0);
    return 1;
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    SceUID fd = *((int *) opaque);
    PHYSFS_sint64 retval;
    retval = (PHYSFS_sint64) sceIoLseek(fd, 0, SCE_SEEK_CUR);
    BAIL_IF(retval < 0, errcodeFromRet(retval), -1);
    return retval;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    SceUID fd = *((int *) opaque);
    SceIoStat statbuf;
    int ret = sceIoGetstatByFd(fd, &statbuf);
    BAIL_IF(ret < 0, errcodeFromRet(ret), -1);
    return ((PHYSFS_sint64) statbuf.st_size);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
    SceUID fd = *((int *) opaque);
    int ret = sceIoSyncByFd(fd, 0);
    BAIL_IF(ret < 0, errcodeFromRet(ret), 0);
    return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    SceUID fd = *((int *) opaque);
    int ret = sceIoClose(fd);
    BAIL_IF(ret < 0, errcodeFromRet(ret), );
    allocator.Free(opaque);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    int ret = sceIoRemove(path);
    BAIL_IF(ret < 0, errcodeFromRet(ret), 0);
    return 1;
} /* __PHYSFS_platformDelete */


int __PHYSFS_platformStat(const char *fname, PHYSFS_Stat *st, const int follow)
{
    SceIoStat statbuf;
    const int rc = sceIoGetstat(fname, &statbuf);
    BAIL_IF(rc < 0, errcodeFromRet(rc), 0);

    if (SCE_S_ISREG(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = statbuf.st_size;
    } /* if */

    else if(SCE_S_ISDIR(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } /* else if */

    else
    {
        st->filetype = PHYSFS_FILETYPE_OTHER;
        st->filesize = statbuf.st_size;
    } /* else */

    sceRtcGetTime64_t(&statbuf.st_atime, &st->accesstime);
    sceRtcGetTime64_t(&statbuf.st_mtime, &st->modtime);
    sceRtcGetTime64_t(&statbuf.st_ctime, &st->createtime);

    st->readonly = 0; // TODO
    return 1;
} /* __PHYSFS_platformStat */


typedef struct
{
    SceUID uid;
} VITA_mutex;

void *__PHYSFS_platformGetThreadID(void)
{
    return ( (void *) (sceKernelGetThreadId()) );
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    VITA_mutex *m = (VITA_mutex *) allocator.Malloc(sizeof (VITA_mutex));
    BAIL_IF(!m, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    m->uid = sceKernelCreateMutex("PHYSFS mutex", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
    if (m->uid < 0)
    {
        allocator.Free(m);
        BAIL(PHYSFS_ERR_OS_ERROR, NULL);
    } /* if */

    return ((void *) m);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    VITA_mutex *m = (VITA_mutex *) mutex;

    if (m == NULL) {
        return;
    }

    sceKernelDeleteMutex(m->uid);
    allocator.Free(m);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    VITA_mutex *m = (VITA_mutex *) mutex;

    if (m == NULL) {
        return 0;
    }

    SceInt32 res = sceKernelLockMutex(m->uid, 1, NULL);
    if (res != SCE_KERNEL_OK) {
        return 0;
    }

    return 1;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    VITA_mutex *m = (VITA_mutex *) mutex;

    if (m == NULL) {
        return;
    }

    sceKernelUnlockMutex(m->uid, 1);
} /* __PHYSFS_platformReleaseMutex */

#endif  /* PHYSFS_PLATFORM_VITA */

/* end of physfs_platform_vita.c ... */

