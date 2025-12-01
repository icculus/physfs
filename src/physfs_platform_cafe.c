/*
 * Nintendo Wii U support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 * This file written by TurtleP
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_CAFE

#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>

#include <sys/stat.h>

#include <coreinit/thread.h>
#include <coreinit/mutex.h>
#include <coreinit/debug.h>

#include "physfs_internal.h"

static PHYSFS_ErrorCode errcodeFromErrnoError(const int err)
{
    switch (err)
    {
    case 0:
        return PHYSFS_ERR_OK;
    case EACCES:
        return PHYSFS_ERR_PERMISSION;
    case EPERM:
        return PHYSFS_ERR_PERMISSION;
    case EDQUOT:
        return PHYSFS_ERR_NO_SPACE;
    case EIO:
        return PHYSFS_ERR_IO;
    case ELOOP:
        return PHYSFS_ERR_SYMLINK_LOOP;
    case EMLINK:
        return PHYSFS_ERR_NO_SPACE;
    case ENAMETOOLONG:
        return PHYSFS_ERR_BAD_FILENAME;
    case ENOENT:
        return PHYSFS_ERR_NOT_FOUND;
    case ENOSPC:
        return PHYSFS_ERR_NO_SPACE;
    case ENOTDIR:
        return PHYSFS_ERR_NOT_FOUND;
    case EISDIR:
        return PHYSFS_ERR_NOT_A_FILE;
    case EROFS:
        return PHYSFS_ERR_READ_ONLY;
    case ETXTBSY:
        return PHYSFS_ERR_BUSY;
    case EBUSY:
        return PHYSFS_ERR_BUSY;
    case ENOMEM:
        return PHYSFS_ERR_OUT_OF_MEMORY;
    case ENOTEMPTY:
        return PHYSFS_ERR_DIR_NOT_EMPTY;
    default:
        return PHYSFS_ERR_OS_ERROR;
    }
}

static inline PHYSFS_ErrorCode errcodeFromErrno(void)
{
    return errcodeFromErrnoError(errno);
} /* errcodeFromErrno */

char *__PHYSFS_platformCalcUserDir(void)
{
    /* Use the jail directory (hopefully) found before. */
    return __PHYSFS_strdup(PHYSFS_getBaseDir());
}

int __PHYSFS_platformInit(void)
{
    return 1; /* always succeeds. */
} /* __PHYSFS_platformInit */

void __PHYSFS_platformDeinit(void)
{
    /* no-op */
} /* __PHYSFS_platformDeinit */

void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
} /* __PHYSFS_platformDetectAvailableCDs */

char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    char *result = NULL;

    /*
    ** The user has not provided a path
    ** so we will use the current working directory
    */
    if (argv0 == NULL)
    {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            const size_t length = strlen(cwd);
            result = (char *)allocator.Malloc(length + 2);
            BAIL_IF(!result, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

            strncpy(result, cwd, length);
            result[length] = '/';
            result[length + 1] = '\0';
        }
    }
    else
    {
        /* hbmenu gives us the full path to the application */
        return NULL;
    }

    if (!result)
    {
        result = __PHYSFS_strdup("/wiiu/");
    }

    return result;
}

char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    char *result = NULL;
    size_t length = 0;

    /*
    ** Use the jail directory (hopefully) found before.
    ** This way we don't need an application folder since it's exclusive.
    */
    const char *userDir = __PHYSFS_getUserDir();
    BAIL_IF_ERRPASS(!userDir, NULL);

    const char *append = ".config/";
    length = strlen(userDir) + strlen(append) + 1;
    result = (char *)allocator.Malloc(length);
    BAIL_IF(!result, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    snprintf(result, length, "%s%s", userDir, append);

    return result;
}

int __PHYSFS_platformStat(const char *fname, PHYSFS_Stat *st, const int follow)
{
    struct stat statbuf;
    const int rc = follow ? stat(fname, &statbuf) : lstat(fname, &statbuf);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);

    if (S_ISREG(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = statbuf.st_size;
    } /* if */

    else if (S_ISDIR(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } /* else if */

    else if (S_ISLNK(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_SYMLINK;
        st->filesize = 0;
    } /* else if */

    else
    {
        st->filetype = PHYSFS_FILETYPE_OTHER;
        st->filesize = statbuf.st_size;
    } /* else */

    st->modtime = statbuf.st_mtime;
    st->createtime = statbuf.st_ctime;
    st->accesstime = statbuf.st_atime;

    st->readonly = !(statbuf.st_mode & S_IWRITE);
    return 1;
}

typedef struct
{
    pthread_mutex_t mutex;
    pthread_t owner;
    PHYSFS_uint32 count;
} PthreadMutex;

void *__PHYSFS_platformGetThreadID(void)
{
    return ((void *)OSGetCurrentThread());
} /* __PHYSFS_platformGetThreadID */

void *__PHYSFS_platformCreateMutex(void)
{
    OSMutex *m = allocator.Malloc(sizeof(OSMutex));
    BAIL_IF(!m, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    OSInitMutex(m);

    return ((void *)m);
} /* __PHYSFS_platformCreateMutex */

void __PHYSFS_platformDestroyMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *)mutex;
    allocator.Free((OSMutex *)mutex);
} /* __PHYSFS_platformDestroyMutex */

int __PHYSFS_platformGrabMutex(void *mutex)
{
    OSLockMutex((OSMutex *)mutex);
    return 1;
} /* __PHYSFS_platformGrabMutex */

void __PHYSFS_platformReleaseMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *)mutex;
    OSUnlockMutex((OSMutex *)mutex);
} /* __PHYSFS_platformReleaseMutex */

PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                                                          PHYSFS_EnumerateCallback callback,
                                                          const char *origdir, void *callbackdata)
{
    DIR *dir;
    struct dirent *ent;
    PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;

    dir = opendir(dirname);
    BAIL_IF(dir == NULL, errcodeFromErrno(), PHYSFS_ENUM_ERROR);

    while ((retval == PHYSFS_ENUM_OK) && ((ent = readdir(dir)) != NULL))
    {
        const char *name = ent->d_name;
        if (name[0] == '.') /* ignore "." and ".." */
        {
            if ((name[1] == '\0') || ((name[1] == '.') && (name[2] == '\0')))
                continue;
        } /* if */

        retval = callback(callbackdata, origdir, name);
        if (retval == PHYSFS_ENUM_ERROR)
            PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
    } /* while */

    closedir(dir);

    return retval;
} /* __PHYSFS_platformEnumerate */

int __PHYSFS_platformMkDir(const char *path)
{
    const int rc = mkdir(path, S_IRWXU);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformMkDir */

#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
static inline void set_CLOEXEC(int fildes)
{
    int flags = fcntl(fildes, F_GETFD);
    if (flags != -1)
    {
        fcntl(fildes, F_SETFD, flags | FD_CLOEXEC);
    }
}
#endif

static void *doOpen(const char *filename, int mode)
{
    const int appending = (mode & O_APPEND);
    int fd;
    int *retval;

    errno = 0;

    /* O_APPEND doesn't actually behave as we'd like. */
    mode &= ~O_APPEND;

#ifdef O_CLOEXEC
    /* Add O_CLOEXEC if defined */
    mode |= O_CLOEXEC;
#endif

    do
    {
        fd = open(filename, mode, S_IRUSR | S_IWUSR);
    } while ((fd < 0) && (errno == EINTR));
    BAIL_IF(fd < 0, errcodeFromErrno(), NULL);

#if !defined(O_CLOEXEC) && defined(FD_CLOEXEC)
    set_CLOEXEC(fd);
#endif

    if (appending)
    {
        if (lseek(fd, 0, SEEK_END) < 0)
        {
            const int err = errno;
            close(fd);
            BAIL(errcodeFromErrnoError(err), NULL);
        } /* if */
    }     /* if */

    retval = (int *)allocator.Malloc(sizeof(int));
    if (!retval)
    {
        close(fd);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    *retval = fd;
    return ((void *)retval);
} /* doOpen */

void *__PHYSFS_platformOpenRead(const char *filename)
{
    return doOpen(filename, O_RDONLY);
} /* __PHYSFS_platformOpenRead */

void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return doOpen(filename, O_WRONLY | O_CREAT | O_TRUNC);
} /* __PHYSFS_platformOpenWrite */

void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return doOpen(filename, O_WRONLY | O_CREAT | O_APPEND);
} /* __PHYSFS_platformOpenAppend */

PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint64 len)
{
    const int fd = *((int *)opaque);
    ssize_t rc = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    do
    {
        rc = read(fd, buffer, (size_t)len);
    } while ((rc == -1) && (errno == EINTR));
    BAIL_IF(rc == -1, errcodeFromErrno(), -1);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64)rc;
} /* __PHYSFS_platformRead */

PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    const int fd = *((int *)opaque);
    ssize_t rc = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    do
    {
        rc = write(fd, (void *)buffer, (size_t)len);
    } while ((rc == -1) && (errno == EINTR));
    BAIL_IF(rc == -1, errcodeFromErrno(), rc);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64)rc;
} /* __PHYSFS_platformWrite */

int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    const int fd = *((int *)opaque);
    const off_t rc = lseek(fd, (off_t)pos, SEEK_SET);
    BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformSeek */

PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    const int fd = *((int *)opaque);
    PHYSFS_sint64 retval;
    retval = (PHYSFS_sint64)lseek(fd, 0, SEEK_CUR);
    BAIL_IF(retval == -1, errcodeFromErrno(), -1);
    return retval;
} /* __PHYSFS_platformTell */

PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    const int fd = *((int *)opaque);
    struct stat statbuf;
    BAIL_IF(fstat(fd, &statbuf) == -1, errcodeFromErrno(), -1);
    return ((PHYSFS_sint64)statbuf.st_size);
} /* __PHYSFS_platformFileLength */

int __PHYSFS_platformFlush(void *opaque)
{
    const int fd = *((int *)opaque);
    int rc = -1;
    if ((fcntl(fd, F_GETFL) & O_ACCMODE) != O_RDONLY)
    {
        do
        {
            rc = fsync(fd);
        } while ((rc == -1) && (errno == EINTR));
        BAIL_IF(rc == -1, errcodeFromErrno(), 0);
    }
    return 1;
} /* __PHYSFS_platformFlush */

void __PHYSFS_platformClose(void *opaque)
{
    const int fd = *((int *)opaque);
    int rc = -1;
    do
    {
        rc = close(fd); /* we don't check this. You should have used flush! */
    } while ((rc == -1) && (errno == EINTR));
    allocator.Free(opaque);
} /* __PHYSFS_platformClose */

int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF(remove(path) == -1, errcodeFromErrno(), 0);
    return 1;
} /* __PHYSFS_platformDelete */

#endif
