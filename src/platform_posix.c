/*
 * Posix-esque support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_POSIX

#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#if ((!defined PHYSFS_NO_THREAD_SUPPORT) && (!defined PHYSFS_PLATFORM_BEOS))
#include <pthread.h>
#endif

#ifdef PHYSFS_HAVE_LLSEEK
#include <linux/unistd.h>
#endif

#include "physfs_internal.h"


const char *__PHYSFS_platformDirSeparator = "/";


char *__PHYSFS_platformCopyEnvironmentVariable(const char *varname)
{
    const char *envr = getenv(varname);
    char *retval = NULL;

    if (envr != NULL)
    {
        retval = (char *) allocator.Malloc(strlen(envr) + 1);
        if (retval != NULL)
            strcpy(retval, envr);
    } /* if */

    return retval;
} /* __PHYSFS_platformCopyEnvironmentVariable */


static char *getUserNameByUID(void)
{
    uid_t uid = getuid();
    struct passwd *pw;
    char *retval = NULL;

    pw = getpwuid(uid);
    if ((pw != NULL) && (pw->pw_name != NULL))
    {
        retval = (char *) allocator.Malloc(strlen(pw->pw_name) + 1);
        if (retval != NULL)
            strcpy(retval, pw->pw_name);
    } /* if */
    
    return retval;
} /* getUserNameByUID */


static char *getUserDirByUID(void)
{
    uid_t uid = getuid();
    struct passwd *pw;
    char *retval = NULL;

    pw = getpwuid(uid);
    if ((pw != NULL) && (pw->pw_dir != NULL))
    {
        retval = (char *) allocator.Malloc(strlen(pw->pw_dir) + 1);
        if (retval != NULL)
            strcpy(retval, pw->pw_dir);
    } /* if */
    
    return retval;
} /* getUserDirByUID */


char *__PHYSFS_platformGetUserName(void)
{
    char *retval = getUserNameByUID();
    if (retval == NULL)
        retval = __PHYSFS_platformCopyEnvironmentVariable("USER");
    return retval;
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    char *retval = __PHYSFS_platformCopyEnvironmentVariable("HOME");

    /* if the environment variable was set, make sure it's really a dir. */
    if (retval != NULL)
    {
        struct stat statbuf;
        if ((stat(retval, &statbuf) == -1) || (S_ISDIR(statbuf.st_mode) == 0))
        {
            allocator.Free(retval);
            retval = NULL;
        } /* if */
    } /* if */

    if (retval == NULL)
        retval = getUserDirByUID();

    return retval;
} /* __PHYSFS_platformGetUserDir */


char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append)
{
    int len = ((prepend) ? strlen(prepend) : 0) +
              ((append) ? strlen(append) : 0) +
              strlen(dirName) + 1;
    char *retval = (char *) allocator.Malloc(len);

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    /* platform-independent notation is Unix-style already.  :)  */

    if (prepend)
        strcpy(retval, prepend);
    else
        retval[0] = '\0';

    strcat(retval, dirName);

    if (append)
        strcat(retval, append);

    return retval;
} /* __PHYSFS_platformCvtToDependent */



void __PHYSFS_platformEnumerateFiles(const char *dirname,
                                     int omitSymLinks,
                                     PHYSFS_EnumFilesCallback callback,
                                     const char *origdir,
                                     void *callbackdata)
{
    DIR *dir;
    struct dirent *ent;
    int bufsize = 0;
    char *buf = NULL;
    int dlen = 0;

    if (omitSymLinks)  /* !!! FIXME: this malloc sucks. */
    {
        dlen = strlen(dirname);
        bufsize = dlen + 256;
        buf = (char *) allocator.Malloc(bufsize);
        if (buf == NULL)
            return;
        strcpy(buf, dirname);
        if (buf[dlen - 1] != '/')
        {
            buf[dlen++] = '/';
            buf[dlen] = '\0';
        } /* if */
    } /* if */

    errno = 0;
    dir = opendir(dirname);
    if (dir == NULL)
    {
        allocator.Free(buf);
        return;
    } /* if */

    while ((ent = readdir(dir)) != NULL)
    {
        if (strcmp(ent->d_name, ".") == 0)
            continue;

        if (strcmp(ent->d_name, "..") == 0)
            continue;

        if (omitSymLinks)
        {
            PHYSFS_Stat statbuf;
            int exists = 0;
            char *p;
            int len = strlen(ent->d_name) + dlen + 1;
            if (len > bufsize)
            {
                p = (char *) allocator.Realloc(buf, len);
                if (p == NULL)
                    continue;
                buf = p;
                bufsize = len;
            } /* if */

            strcpy(buf + dlen, ent->d_name);

            if (__PHYSFS_platformStat(buf, &exists, &statbuf))
            {
                if (!exists)
                    continue;
                else if (statbuf.filetype == PHYSFS_FILETYPE_SYMLINK)
                    continue;
            } /* if */
        } /* if */

        callback(callbackdata, origdir, ent->d_name);
    } /* while */

    allocator.Free(buf);
    closedir(dir);
} /* __PHYSFS_platformEnumerateFiles */


int __PHYSFS_platformMkDir(const char *path)
{
    int rc;
    errno = 0;
    rc = mkdir(path, S_IRWXU);
    BAIL_IF_MACRO(rc == -1, strerror(errno), 0);
    return 1;
} /* __PHYSFS_platformMkDir */


static void *doOpen(const char *filename, int mode)
{
    const int appending = (mode & O_APPEND);
    int fd;
    int *retval;
    errno = 0;

    /* O_APPEND doesn't actually behave as we'd like. */
    mode &= ~O_APPEND;

    fd = open(filename, mode, S_IRUSR | S_IWUSR);
    BAIL_IF_MACRO(fd < 0, strerror(errno), NULL);

    if (appending)
    {
        if (lseek(fd, 0, SEEK_END) < 0)
        {
            close(fd);
            BAIL_MACRO(strerror(errno), NULL);
        } /* if */
    } /* if */

    retval = (int *) allocator.Malloc(sizeof (int));
    if (retval == NULL)
    {
        close(fd);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    *retval = fd;
    return ((void *) retval);
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
    const int fd = *((int *) opaque);
    ssize_t rc = 0;

    BAIL_IF_MACRO(!__PHYSFS_ui64FitsAddressSpace(len),ERR_INVALID_ARGUMENT,-1);

    rc = read(fd, buffer, (size_t) len);
    BAIL_IF_MACRO(rc == -1, strerror(errno), (PHYSFS_sint64) rc);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    const int fd = *((int *) opaque);
    ssize_t rc = 0;

    BAIL_IF_MACRO(!__PHYSFS_ui64FitsAddressSpace(len),ERR_INVALID_ARGUMENT,-1);

    rc = write(fd, (void *) buffer, (size_t) len);
    BAIL_IF_MACRO(rc == -1, strerror(errno), rc);
    assert(rc >= 0);
    assert(rc <= len);
    return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    const int fd = *((int *) opaque);

    #ifdef PHYSFS_HAVE_LLSEEK
      unsigned long offset_high = ((pos >> 32) & 0xFFFFFFFF);
      unsigned long offset_low = (pos & 0xFFFFFFFF);
      loff_t retoffset;
      int rc = llseek(fd, offset_high, offset_low, &retoffset, SEEK_SET);
      BAIL_IF_MACRO(rc == -1, strerror(errno), 0);
    #else
      BAIL_IF_MACRO(lseek(fd, (int) pos, SEEK_SET) == -1, strerror(errno), 0);
    #endif

    return 1;
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    const int fd = *((int *) opaque);
    PHYSFS_sint64 retval;

    #ifdef PHYSFS_HAVE_LLSEEK
      loff_t retoffset;
      int rc = llseek(fd, 0, &retoffset, SEEK_CUR);
      BAIL_IF_MACRO(rc == -1, strerror(errno), -1);
      retval = (PHYSFS_sint64) retoffset;
    #else
      retval = (PHYSFS_sint64) lseek(fd, 0, SEEK_CUR);
      BAIL_IF_MACRO(retval == -1, strerror(errno), -1);
    #endif

    return retval;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    const int fd = *((int *) opaque);
    struct stat statbuf;
    BAIL_IF_MACRO(fstat(fd, &statbuf) == -1, strerror(errno), -1);
    return ((PHYSFS_sint64) statbuf.st_size);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformEOF(void *opaque)
{
    const PHYSFS_sint64 pos = __PHYSFS_platformTell(opaque);
    const PHYSFS_sint64 len = __PHYSFS_platformFileLength(opaque);
    return (pos >= len);
} /* __PHYSFS_platformEOF */


int __PHYSFS_platformFlush(void *opaque)
{
    const int fd = *((int *) opaque);
    BAIL_IF_MACRO(fsync(fd) == -1, strerror(errno), 0);
    return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    const int fd = *((int *) opaque);
    (void) close(fd);  /* we don't check this. You should have used flush! */
    allocator.Free(opaque);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF_MACRO(remove(path) == -1, strerror(errno), 0);
    return 1;
} /* __PHYSFS_platformDelete */


int __PHYSFS_platformStat(const char *filename, int *exists, PHYSFS_Stat *st)
{
    struct stat statbuf;

    if (lstat(filename, &statbuf))
    {
        if (errno == ENOENT)
        {
            *exists = 0;
            return 0;
        } /* if */

        BAIL_MACRO(strerror(errno), 0);
    } /* if */

    *exists = 1;

    if (S_ISREG(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = statbuf.st_size;
    } /* if */

    else if(S_ISDIR(statbuf.st_mode))
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
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

    /* !!! FIXME: maybe we should just report full permissions? */
    st->readonly = access(filename, W_OK);
    return 1;
} /* __PHYSFS_platformStat */


#ifndef PHYSFS_PLATFORM_BEOS  /* BeOS has its own code in platform_beos.cpp */
#if (defined PHYSFS_NO_THREAD_SUPPORT)

void *__PHYSFS_platformGetThreadID(void) { return ((void *) 0x0001); }
void *__PHYSFS_platformCreateMutex(void) { return ((void *) 0x0001); }
void __PHYSFS_platformDestroyMutex(void *mutex) {}
int __PHYSFS_platformGrabMutex(void *mutex) { return 1; }
void __PHYSFS_platformReleaseMutex(void *mutex) {}

#else

typedef struct
{
    pthread_mutex_t mutex;
    pthread_t owner;
    PHYSFS_uint32 count;
} PthreadMutex;


void *__PHYSFS_platformGetThreadID(void)
{
    return ( (void *) ((size_t) pthread_self()) );
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    int rc;
    PthreadMutex *m = (PthreadMutex *) allocator.Malloc(sizeof (PthreadMutex));
    BAIL_IF_MACRO(m == NULL, ERR_OUT_OF_MEMORY, NULL);
    rc = pthread_mutex_init(&m->mutex, NULL);
    if (rc != 0)
    {
        allocator.Free(m);
        BAIL_MACRO(strerror(rc), NULL);
    } /* if */

    m->count = 0;
    m->owner = (pthread_t) 0xDEADBEEF;
    return ((void *) m);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *) mutex;

    /* Destroying a locked mutex is a bug, but we'll try to be helpful. */
    if ((m->owner == pthread_self()) && (m->count > 0))
        pthread_mutex_unlock(&m->mutex);

    pthread_mutex_destroy(&m->mutex);
    allocator.Free(m);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *) mutex;
    pthread_t tid = pthread_self();
    if (m->owner != tid)
    {
        if (pthread_mutex_lock(&m->mutex) != 0)
            return 0;
        m->owner = tid;
    } /* if */

    m->count++;
    return 1;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    PthreadMutex *m = (PthreadMutex *) mutex;
    if (m->owner == pthread_self())
    {
        if (--m->count == 0)
        {
            m->owner = (pthread_t) 0xDEADBEEF;
            pthread_mutex_unlock(&m->mutex);
        } /* if */
    } /* if */
} /* __PHYSFS_platformReleaseMutex */

#endif /* !PHYSFS_NO_THREAD_SUPPORT */
#endif /* !PHYSFS_PLATFORM_BEOS */

#endif  /* PHYSFS_PLATFORM_POSIX */

/* end of posix.c ... */

