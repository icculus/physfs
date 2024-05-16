/*
 * libretro support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Vladimir Serbinenko (@phcoder), and Rob Loach.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_LIBRETRO

#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

/* libretro dependencies */
#include <libretro.h>
#include <rthreads/rthreads.h>
#include <streams/file_stream.h>
#include <retro_dirent.h>

#include "physfs_internal.h"

static retro_environment_t physfs_platform_libretro_environ_cb = NULL;
static struct retro_vfs_interface *physfs_platform_libretro_vfs = NULL;

int __PHYSFS_platformInit(const char *argv0)
{
	retro_environment_t environ_cb = (retro_environment_t)argv0;
	struct retro_vfs_interface_info vfs_interface_info;

    /* as a cheat, we expect argv0 to be a retro_environment_t callback. */
	if (environ_cb == NULL) {
		return 0;
	}

	/* get the virtual file system interface */
	vfs_interface_info.required_interface_version = 3;
	vfs_interface_info.iface = NULL;
	if (!environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_interface_info)) {
		return 0;
	}

	physfs_platform_libretro_vfs = vfs_interface_info.iface;
	physfs_platform_libretro_environment = environ_cb;

	return 1;
} /* __PHYSFS_platformInit */


void __PHYSFS_platformDeinit(void)
{
	physfs_platform_libretro_environ_cb = NULL;
	physfs_platform_libretro_vfs = NULL;
} /* __PHYSFS_platformDeinit */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
} /* __PHYSFS_platformDetectAvailableCDs */

const char* physfs_platform_libretro_get_directory(int libretro_dir, int extra_memory, int add_slash)
{
	const char* dir = NULL;
	const char* retval;
	size_t dir_length = 0;

	BAIL_IF(physfs_platform_environ_cb == NULL, PHYSFS_ERR_NOT_INITIALIZED, NULL);

	if (!physfs_platform_environ_cb(libretro_dir, &dir)) {
		return NULL;
	}

	dir_length = strlen(dir);
	retval = allocator.Malloc(dir_length + 2 + extra_memory);
	if (!retval) {
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
	}

	strcpy(retval, dir);

	if (add_slash && (dir_length == 0 || ret[dir_length - 1] != __PHYSFS_platformDirSeparator)) {
		ret[dir_length] = __PHYSFS_platformDirSeparator;
		ret[dir_length + 1] = '\0';
	}

	return retval;
}

char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
	return physfs_platform_libretro_get_directory(RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY, 0);
} /* __PHYSFS_platformCalcBaseDir */

char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
	const char* retval = physfs_platform_libretro_get_directory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, strlen(org) + strlen(app) + 2);
    snprintf(retval, strlen(retval), "%s/%s/%s/", utf8, org, app);
} /* __PHYSFS_platformCalcPrefDir */

char *__PHYSFS_platformCalcUserDir(void)
{
	return physfs_platform_libretro_get_directory(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, 0);
} /* __PHYSFS_platformCalcUserDir */


 // Single-threaded. WiiU is multi-threaded but our frontend is single-threaded and we're statically linked
typedef struct
{
	slock_t *mutex;
	uintptr_t owner;
	PHYSFS_uint32 count;
} PthreadMutex;

void *__PHYSFS_platformGetThreadID(void)
{
	return ( (void *) sthread_get_current_thread_id() );
} /* __PHYSFS_platformGetThreadID */

void *__PHYSFS_platformCreateMutex(void)
{
	PthreadMutex *m = (PthreadMutex *) allocator.Malloc(sizeof (PthreadMutex));
	BAIL_IF(!m, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
	m->mutex = slock_new();
	if (!m->mutex) {
		allocator.Free(m);
		BAIL(PHYSFS_ERR_OS_ERROR, NULL);
	} /* if */

	m->count = 0;
	m->owner = (uintptr_t) 0xDEADBEEF;
	return ((void *) m);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
	PthreadMutex *m = (PthreadMutex *) mutex;

	/* Destroying a locked mutex is a bug, but we'll try to be helpful. */
	if ((m->owner == sthread_get_current_thread_id()) && (m->count > 0))
		slock_unlock(m->mutex);

	slock_free(m->mutex);
	allocator.Free(m);
} /* __PHYSFS_platformDestroyMutex */

int __PHYSFS_platformGrabMutex(void *mutex)
{
	PthreadMutex *m = (PthreadMutex *) mutex;
	uintptr_t tid = sthread_get_current_thread_id();
	if (m->owner != tid)
	{
		slock_lock(m->mutex);
		m->owner = tid;
	} /* if */

	m->count++;
	return 1;
} /* __PHYSFS_platformGrabMutex */

void __PHYSFS_platformReleaseMutex(void *mutex)
{
	PthreadMutex *m = (PthreadMutex *) mutex;
	assert(m->owner == sthread_get_current_thread_id());  /* catch programming errors. */
	assert(m->count > 0);  /* catch programming errors. */
	if (m->owner == sthread_get_current_thread_id()) {
		if (--m->count == 0) {
			m->owner = (uintptr_t) 0xDEADBEEF;
			slock_unlock(m->mutex);
		} /* if */
	} /* if */
} /* __PHYSFS_platformReleaseMutex */

PHYSFS_EnumerateCallbackResult __PHYSFS_platformEnumerate(const char *dirname,
                               PHYSFS_EnumerateCallback callback,
                               const char *origdir, void *callbackdata)
{
	RDIR *dir;
	PHYSFS_EnumerateCallbackResult retval = PHYSFS_ENUM_OK;

	dir = retro_opendir(dirname);
	BAIL_IF(dir == NULL, PHYSFS_ERR_NOT_FOUND, PHYSFS_ENUM_ERROR);

	while (retro_readdir(dir))
	{
		const char *name = retro_dirent_get_name(dir);
		if (name[0] == '.')  /* ignore "." and ".." */
		{
			if ((name[1] == '\0') || ((name[1] == '.') && (name[2] == '\0')))
				continue;
		} /* if */
		
		retval = callback(callbackdata, origdir, name);
		if (retval == PHYSFS_ENUM_ERROR)
			PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
	} /* while */

	retro_closedir(dir);

	return retval;
} /* __PHYSFS_platformEnumerate */

static void *__PHYSFS_platformOpen(const char *filename, unsigned lr_mode, int appending)
{
	RFILE *fd = filestream_open(filename, lr_mode, RETRO_VFS_FILE_ACCESS_HINT_NONE);
	BAIL_IF(fd == NULL, PHYSFS_ERR_NOT_FOUND, NULL); // TODO: improve error handling?

	if (appending)
	{	    
		if (filestream_seek(fd, 0, RETRO_VFS_SEEK_POSITION_END) < 0)
		{
			filestream_close(fd);
			BAIL(PHYSFS_ERR_IO, NULL);
		}
	} /* if */

	return fd;
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
	RFILE *fd = (RFILE *) opaque;
	ssize_t rc = 0;

	if (!__PHYSFS_ui64FitsAddressSpace(len))
		BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

	rc = filestream_read(fd, buffer, (size_t) len);
	BAIL_IF(rc == -1, PHYSFS_ERR_IO, -1);
	assert(rc >= 0);
	assert(rc <= len);
	return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformRead */

PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
	RFILE *fd = (RFILE *) opaque;
	ssize_t rc = 0;

	if (!__PHYSFS_ui64FitsAddressSpace(len))
		BAIL(PHYSFS_ERR_INVALID_ARGUMENT, -1);

	rc = filestream_write(fd, (void *) buffer, (size_t) len);
	BAIL_IF(rc == -1, PHYSFS_ERR_IO, rc);
	assert(rc >= 0);
	assert(rc <= len);
	return (PHYSFS_sint64) rc;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
	RFILE *fd = (RFILE *) opaque;
	const int64_t rc = filestream_seek(fd, (off_t) pos, RETRO_VFS_SEEK_POSITION_START);
	BAIL_IF(rc == -1, PHYSFS_ERR_IO, 0);
	return 1;
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
	RFILE *fd = (RFILE *) opaque;
	PHYSFS_sint64 retval;
	retval = (PHYSFS_sint64) filestream_tell(fd);
	BAIL_IF(retval == -1, PHYSFS_ERR_IO, -1);
	return retval;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
	RFILE *fd = (RFILE *) opaque;
	PHYSFS_sint64 retval;
	retval = (PHYSFS_sint64) filestream_get_size(fd);
	BAIL_IF(retval == -1, PHYSFS_ERR_IO, -1);
	return retval;
} /* __PHYSFS_platformFileLength */

int __PHYSFS_platformFlush(void *opaque)
{
	RFILE *fd = (RFILE *) opaque;
	filestream_flush(fd);
	return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
	RFILE *fd = (RFILE *) opaque;
	filestream_close(fd);
} /* __PHYSFS_platformClose */

int __PHYSFS_platformDelete(const char *path)
{
    BAIL_IF(filestream_delete(path) == -1, PHYSFS_ERR_IO, 0);
    return 1;
} /* __PHYSFS_platformDelete */

static PHYSFS_ErrorCode errcodeFromErrnoError(const int err)
{
    switch (err)
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


static inline PHYSFS_ErrorCode errcodeFromErrno(void)
{
    return errcodeFromErrnoError(errno);
} /* errcodeFromErrno */

int __PHYSFS_platformMkDir(const char *path)
{
	const int rc;
	BAIL_IF(physfs_platform_libretro_vfs == NULL || physfs_platform_libretro_vfs->mkdir == NULL, PHYSFS_ERR_NOT_INITIALIZED, 0);
	rc = physfs_platform_libretro_vfs->mkdir(path);
	BAIL_IF(rc == -1, PHYSFS_ERR_IO, 0);
	BAIL_IF(rc == -2, PHYSFS_ERR_DUPLICATE, 0); //Already exists
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

	// TODO: Fill those properly
	st->modtime = 0;
	st->createtime = 0;
	st->accesstime = 0;
	st->readonly = 0;

	return 1;
} /* __PHYSFS_platformStat */

#endif  /* PHYSFS_PLATFORM_LIBRETRO */

/* end of physfs_platform_libretro.c ... */
