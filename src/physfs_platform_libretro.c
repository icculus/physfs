/*
 * libretro support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Vladimir Serbinenko (@phcoder), and Rob Loach (@RobLoach).
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_LIBRETRO

#include <string.h>
#include <errno.h>

/* libretro dependencies */
#include <libretro.h>

/**
 * Disable threading support by defining PHYSFS_PLATFORM_LIBRETRO_NO_THREADS.
 */
#ifndef PHYSFS_PLATFORM_LIBRETRO_NO_THREADS
#include <rthreads/rthreads.h>
#endif  /* PHYSFS_PLATFORM_LIBRETRO_NO_THREADS */

#include "physfs_internal.h"

static retro_environment_t physfs_platform_libretro_environ_cb = NULL;
static struct retro_vfs_interface *physfs_platform_libretro_vfs = NULL;


int __PHYSFS_platformInit(const char *argv0)
{
    /* as a cheat, we expect argv0 to be a retro_environment_t callback. */
    retro_environment_t environ_cb = (retro_environment_t) argv0;
    struct retro_vfs_interface_info vfs_interface_info;
    BAIL_IF(environ_cb == NULL, PHYSFS_ERR_INVALID_ARGUMENT, 0);

    /* get the virtual file system interface, at least version 3 */
    vfs_interface_info.required_interface_version = 3;
    vfs_interface_info.iface = NULL;
    BAIL_IF(!environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_interface_info), PHYSFS_ERR_UNSUPPORTED, 0);

    physfs_platform_libretro_vfs = vfs_interface_info.iface;
    physfs_platform_libretro_environ_cb = environ_cb;

    return 1;
} /* __PHYSFS_platformInit */


void __PHYSFS_platformDeinit(void)
{
    physfs_platform_libretro_environ_cb = NULL;
    physfs_platform_libretro_vfs = NULL;
} /* __PHYSFS_platformDeinit */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    /* no-op. */
} /* __PHYSFS_platformDetectAvailableCDs */


char *physfs_platform_libretro_get_directory(int libretro_dir, int add_slash)
{
    const char *dir = NULL;
    char *retval;
    size_t dir_length;

    BAIL_IF(physfs_platform_libretro_environ_cb == NULL, PHYSFS_ERR_NOT_INITIALIZED, NULL);
    BAIL_IF(!physfs_platform_libretro_environ_cb(libretro_dir, &dir), PHYSFS_ERR_UNSUPPORTED, NULL);

    dir_length = strlen(dir);
    retval = allocator.Malloc(dir_length + 2);
    if (!retval) {
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    }

    strcpy(retval, dir);

    if (add_slash && (dir_length == 0 || retval[dir_length - 1] != __PHYSFS_platformDirSeparator)) {
        retval[dir_length] = __PHYSFS_platformDirSeparator;
        retval[dir_length + 1] = '\0';
    }

    return retval;
}


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    int append_slash = 1;
    char* dir = NULL;
    char* retval;
    size_t dir_length;
    const struct retro_game_info_ext *game_info_ext = NULL;
    BAIL_IF(physfs_platform_libretro_environ_cb == NULL, PHYSFS_ERR_NOT_INITIALIZED, NULL);

    /* try to get the base path to the actively loaded content */
    if (physfs_platform_libretro_environ_cb(RETRO_ENVIRONMENT_GET_GAME_INFO_EXT, &game_info_ext) && game_info_ext != NULL) {
        /* use the archive file if the content is within an archive */
        if (game_info_ext->file_in_archive && game_info_ext->archive_path != NULL) {
            dir = (char *) game_info_ext->archive_path;
            append_slash = 0;
        } else {
            dir = (char *) game_info_ext->dir;
        }
    }

    /* fallback to the system directory */
    if (dir == NULL)
        physfs_platform_libretro_environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &dir);

    /* as a last case, provide an empty string, assuming the platform resolves a working directory */
    if (dir == NULL)
        dir = "";

    dir_length = strlen(dir);
    retval = allocator.Malloc(dir_length + 2);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, dir);

    /* append a slash if needed */
    if (append_slash && (dir_length == 0 || retval[dir_length - 1] != __PHYSFS_platformDirSeparator)) {
        retval[dir_length] = __PHYSFS_platformDirSeparator;
        retval[dir_length + 1] = '\0';
    }
    return retval;
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    const char *userdir = __PHYSFS_getUserDir();
    const size_t len = strlen(userdir) + strlen(org) + strlen(app) + 3;
    char *retval = (char *) allocator.Malloc(len);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    snprintf(retval, len, "%s%s/%s/", userdir, org, app);
    return retval;
} /* __PHYSFS_platformCalcPrefDir */


char *__PHYSFS_platformCalcUserDir(void)
{
    return physfs_platform_libretro_get_directory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, 1);
} /* __PHYSFS_platformCalcUserDir */


#ifndef PHYSFS_PLATFORM_LIBRETRO_NO_THREADS
/**
 * Thread mutex data structure.
 */
typedef struct
{
    slock_t *mutex;
    uintptr_t owner;
    PHYSFS_uint32 count;
} PthreadMutex;
#endif  /* PHYSFS_PLATFORM_LIBRETRO_NO_THREADS */


void *__PHYSFS_platformGetThreadID(void)
{
#ifdef PHYSFS_PLATFORM_LIBRETRO_NO_THREADS
    return (void *) (size_t) 0x1;
#else
    return (void *) sthread_get_current_thread_id();
#endif  /* PHYSFS_PLATFORM_LIBRETRO_NO_THREADS */
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
#ifdef PHYSFS_PLATFORM_LIBRETRO_NO_THREADS
    return (void *) (size_t) 0x1;
#else
    PthreadMutex *m = (PthreadMutex *) allocator.Malloc(sizeof (PthreadMutex));
    BAIL_IF(!m, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    m->mutex = slock_new();
    if (!m->mutex) {
        allocator.Free(m);
        BAIL(PHYSFS_ERR_OS_ERROR, NULL);
    } /* if */

    m->count = 0;
    m->owner = (uintptr_t) 0xDEADBEEF;
    return (void *) m;
#endif  /* PHYSFS_PLATFORM_LIBRETRO_NO_THREADS */
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
#ifndef PHYSFS_PLATFORM_LIBRETRO_NO_THREADS
    PthreadMutex *m = (PthreadMutex *) mutex;

    /* Destroying a locked mutex is a bug, but we'll try to be helpful. */
    if ((m->owner == sthread_get_current_thread_id()) && (m->count > 0))
        slock_unlock(m->mutex);

    slock_free(m->mutex);
    allocator.Free(m);
#endif  /* PHYSFS_PLATFORM_LIBRETRO_NO_THREADS */
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
#ifdef PHYSFS_PLATFORM_LIBRETRO_NO_THREADS
    return 0;
#else
    PthreadMutex *m = (PthreadMutex *) mutex;
    uintptr_t tid = sthread_get_current_thread_id();
    if (m->owner != tid)
    {
        slock_lock(m->mutex);
        m->owner = tid;
    } /* if */

    m->count++;
    return 1;
#endif  /* PHYSFS_PLATFORM_LIBRETRO_NO_THREADS */
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
#ifndef PHYSFS_PLATFORM_LIBRETRO_NO_THREADS
    PthreadMutex *m = (PthreadMutex *) mutex;
    assert(m->owner == sthread_get_current_thread_id());  /* catch programming errors. */
    assert(m->count > 0);  /* catch programming errors. */
    if (m->owner == sthread_get_current_thread_id()) {
        if (--m->count == 0) {
            m->owner = (uintptr_t) 0xDEADBEEF;
            slock_unlock(m->mutex);
        } /* if */
    } /* if */
#endif  /* PHYSFS_PLATFORM_LIBRETRO_NO_THREADS */
} /* __PHYSFS_platformReleaseMutex */


PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                               PHYSFS_EnumerateCallback callback,
                               const char *origdir, void *callbackdata)
{
    struct retro_vfs_dir_handle* dir;
    PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->opendir == NULL || physfs_platform_libretro_vfs->readdir == NULL, PHYSFS_ERR_NOT_INITIALIZED, PHYSFS_ENUM_ERROR);

    dir = physfs_platform_libretro_vfs->opendir(dirname, true);
    BAIL_IF(dir == NULL, PHYSFS_ERR_NOT_FOUND, PHYSFS_ENUM_ERROR);

    while (physfs_platform_libretro_vfs->readdir(dir)) {
        const char *name = physfs_platform_libretro_vfs->dirent_get_name(dir);
        if (name[0] == '.') {  /* ignore "." and ".." */
            if ((name[1] == '\0') || ((name[1] == '.') && (name[2] == '\0'))) {
                continue;
            } /* if */
        } /* if */

        retval = callback(callbackdata, origdir, name);
        if (retval == PHYSFS_ENUM_ERROR) {
            PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
            break;
        }
    } /* while */

    if (physfs_platform_libretro_vfs->closedir != NULL) {
        physfs_platform_libretro_vfs->closedir(dir);
    }

    return retval;
} /* __PHYSFS_platformEnumerate */


static void *__PHYSFS_platformOpen(const char *filename, unsigned lr_mode, int appending)
{
    void* retval;
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->open == NULL, PHYSFS_ERR_NOT_INITIALIZED, NULL);
    BAIL_IF(filename == NULL || strlen(filename) == 0, PHYSFS_ERR_BAD_FILENAME, NULL);
    retval = (void *) physfs_platform_libretro_vfs->open(filename, lr_mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
    BAIL_IF(retval == NULL, PHYSFS_ERR_IO, NULL);

    if (appending) {
        if (physfs_platform_libretro_vfs->seek(retval, 0, RETRO_VFS_SEEK_POSITION_END) < 0) {
            physfs_platform_libretro_vfs->close(retval);
            BAIL(PHYSFS_ERR_IO, NULL);
        } /* if */
    } /* if */

    return retval;
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return __PHYSFS_platformOpen(filename, RETRO_VFS_FILE_ACCESS_READ, 0);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return __PHYSFS_platformOpen(filename, RETRO_VFS_FILE_ACCESS_WRITE, 0);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return __PHYSFS_platformOpen(filename, RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING, 1);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint64 len)
{
    PHYSFS_sint64 retval;
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->read == NULL, PHYSFS_ERR_NOT_INITIALIZED, -1);
    BAIL_IF(opaque == NULL || buffer == NULL, PHYSFS_ERR_INVALID_ARGUMENT, -1);
    retval = (PHYSFS_sint64) physfs_platform_libretro_vfs->read((struct retro_vfs_file_handle *) opaque, buffer, (uint64_t) len);
    BAIL_IF(retval == -1, PHYSFS_ERR_IO, -1);
    return retval;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    PHYSFS_sint64 retval;
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->write == NULL, PHYSFS_ERR_NOT_INITIALIZED, -1);
    BAIL_IF(opaque == NULL || buffer == NULL, PHYSFS_ERR_INVALID_ARGUMENT, -1);
    retval = (PHYSFS_sint64) physfs_platform_libretro_vfs->write((struct retro_vfs_file_handle *) opaque, buffer, (uint64_t)len);
    BAIL_IF(retval == -1, PHYSFS_ERR_IO, -1);
    return retval;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->seek == NULL, PHYSFS_ERR_NOT_INITIALIZED, 0);
    BAIL_IF(opaque == NULL, PHYSFS_ERR_INVALID_ARGUMENT, 0);
    BAIL_IF(physfs_platform_libretro_vfs->seek((struct retro_vfs_file_handle *) opaque, pos, RETRO_VFS_SEEK_POSITION_START) == -1, PHYSFS_ERR_IO, 0);
    return 1;
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->tell == NULL, PHYSFS_ERR_NOT_INITIALIZED, -1);
    BAIL_IF(opaque == NULL, PHYSFS_ERR_INVALID_ARGUMENT, -1);
    return (PHYSFS_sint64) physfs_platform_libretro_vfs->tell((struct retro_vfs_file_handle *) opaque);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->size == NULL, PHYSFS_ERR_NOT_INITIALIZED, -1);
    BAIL_IF(opaque == NULL, PHYSFS_ERR_INVALID_ARGUMENT, -1);
    return (PHYSFS_sint64) physfs_platform_libretro_vfs->size((struct retro_vfs_file_handle *) opaque);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->flush == NULL, PHYSFS_ERR_NOT_INITIALIZED, 0);
    BAIL_IF(opaque == NULL, PHYSFS_ERR_INVALID_ARGUMENT, 0);
    return physfs_platform_libretro_vfs->flush((struct retro_vfs_file_handle *) opaque) == 0 ? 1 : 0;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->close == NULL, PHYSFS_ERR_NOT_INITIALIZED, /* void */);
    BAIL_IF(opaque == NULL, PHYSFS_ERR_INVALID_ARGUMENT, /* void */);
    physfs_platform_libretro_vfs->close((struct retro_vfs_file_handle *) opaque);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->remove == NULL, PHYSFS_ERR_NOT_INITIALIZED, 0);
    return physfs_platform_libretro_vfs->remove(path) == 0 ? 1 : 0;
} /* __PHYSFS_platformDelete */


static PHYSFS_ErrorCode errcodeFromErrnoError(const int err)
{
    switch (err)
    {
        case 0: return PHYSFS_ERR_OK;
        case EACCES: return PHYSFS_ERR_PERMISSION;
        case EPERM: return PHYSFS_ERR_PERMISSION;
#if defined(EDQUOT)
        case EDQUOT: return PHYSFS_ERR_NO_SPACE;
#elif defined(WSAEDQUOT)
        case WSAEDQUOT: return PHYSFS_ERR_NO_SPACE;
#endif
        case EIO: return PHYSFS_ERR_IO;
#if defined(ELOOP)
        case ELOOP: return PHYSFS_ERR_SYMLINK_LOOP;
#endif
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


static inline PHYSFS_ErrorCode errcodeFromErrno(void)
{
    return errcodeFromErrnoError(errno);
} /* errcodeFromErrno */


int __PHYSFS_platformMkDir(const char *path)
{
    int rc;
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->mkdir == NULL, PHYSFS_ERR_NOT_INITIALIZED, -1);
    rc = physfs_platform_libretro_vfs->mkdir(path);
    BAIL_IF(rc == -1, PHYSFS_ERR_IO, 0); /* Unknown failure */
    BAIL_IF(rc == -2, PHYSFS_ERR_DUPLICATE, 0); /* Already exists */
    return 1;
} /* __PHYSFS_platformMkDir */


int __PHYSFS_platformStat(const char *fname, PHYSFS_Stat *st, const int follow)
{
    int32_t size;
    int flags;
    BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->stat == NULL, PHYSFS_ERR_NOT_INITIALIZED, 0);
    flags = physfs_platform_libretro_vfs->stat(fname, &size);
    if (!(flags & RETRO_VFS_STAT_IS_VALID)) {
        BAIL(PHYSFS_ERR_NOT_FOUND, 0);
    }

    if (flags & RETRO_VFS_STAT_IS_DIRECTORY) {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } else if (flags & RETRO_VFS_STAT_IS_CHARACTER_SPECIAL) {
        st->filetype = PHYSFS_FILETYPE_OTHER;
        st->filesize = 0;
    } else {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = size;
    }

    /* libretro's virtual file system doesn't support retrieving the following values */
    st->modtime = -1;
    st->createtime = -1;
    st->accesstime = -1;
    st->readonly = 0;

    return 1;
} /* __PHYSFS_platformStat */

#endif  /* PHYSFS_PLATFORM_LIBRETRO */

/* end of physfs_platform_libretro.c ... */
