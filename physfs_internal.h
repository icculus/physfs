/*
 * Internal function/structure declaration. Do NOT include in your
 *  application.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef _INCLUDE_PHYSFS_INTERNAL_H_
#define _INCLUDE_PHYSFS_INTERNAL_H_

#ifndef __PHYSICSFS_INTERNAL__
#error Do not include this header from your applications.
#endif

#include "physfs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct __PHYSFS_DIRHANDLE__;
struct __PHYSFS_FILEFUNCTIONS__;


typedef struct __PHYSFS_LINKEDSTRINGLIST__
{
    char *str;
    struct __PHYSFS_LINKEDSTRINGLIST__ *next;
} LinkedStringList;


typedef struct __PHYSFS_FILEHANDLE__
{
        /*
         * This is reserved for the driver to store information.
         */
    void *opaque;

        /*
         * This should be the DirHandle that created this FileHandle.
         */
    const struct __PHYSFS_DIRHANDLE__ *dirHandle;

        /*
         * Pointer to the file i/o functions for this filehandle.
         */
    const struct __PHYSFS_FILEFUNCTIONS__ *funcs;
} FileHandle;


typedef struct __PHYSFS_FILEFUNCTIONS__
{
        /*
         * Read more from the file.
         * Returns number of objects of (objSize) bytes read from file, -1
         *  if complete failure.
         * On failure, call __PHYSFS_setError().
         */
    PHYSFS_sint64 (*read)(FileHandle *handle, void *buffer,
                          PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);

        /*
         * Write more to the file. Archives don't have to implement this.
         *  (Set it to NULL if not implemented).
         * Returns number of objects of (objSize) bytes written to file, -1
         *  if complete failure.
         * On failure, call __PHYSFS_setError().
         */
    PHYSFS_sint64 (*write)(FileHandle *handle, const void *buffer,
                 PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);

        /*
         * Returns non-zero if at end of file.
         */
    int (*eof)(FileHandle *handle);

        /*
         * Returns byte offset from start of file.
         */
    PHYSFS_sint64 (*tell)(FileHandle *handle);

        /*
         * Move read/write pointer to byte offset from start of file.
         *  Returns non-zero on success, zero on error.
         * On failure, call __PHYSFS_setError().
         */
    int (*seek)(FileHandle *handle, PHYSFS_uint64 offset);

        /*
         * Return number of bytes available in the file, or -1 if you
         *  aren't able to determine.
         * On failure, call __PHYSFS_setError().
         */
    PHYSFS_sint64 (*fileLength)(FileHandle *handle);

        /*
         * Close the file, and free the FileHandle structure (including "opaque").
         *  returns non-zero on success, zero if can't close file.
         * On failure, call __PHYSFS_setError().
         */
    int (*fileClose)(FileHandle *handle);
} FileFunctions;


typedef struct __PHYSFS_DIRHANDLE__
{
        /*
         * This is reserved for the driver to store information.
         */
    void *opaque;

        /*
         * Pointer to the directory i/o functions for this handle.
         */
    const struct __PHYSFS_DIRFUNCTIONS__ *funcs;
} DirHandle;


/*
 * Symlinks should always be followed; PhysicsFS will use
 *  DirFunctions->isSymLink() and make a judgement on whether to
 *  continue to call other methods based on that.
 */
typedef struct __PHYSFS_DIRFUNCTIONS__
{
        /*
         * Returns non-zero if (filename) is a valid archive that this
         *  driver can handle. This filename is in platform-dependent
         *  notation. forWriting is non-zero if this is to be used for
         *  the write directory, and zero if this is to be used for an
         *  element of the search path.
         */
    int (*isArchive)(const char *filename, int forWriting);

        /*
         * Return a DirHandle for dir/archive (name).
         *  This filename is in platform-dependent notation.
         *  forWriting is non-zero if this is to be used for
         *  the write directory, and zero if this is to be used for an
         *  element of the search path.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    DirHandle *(*openArchive)(const char *name, int forWriting);

        /*
         * Returns a list of all files in dirname. Each element of this list
         *  (and its "str" field) will be deallocated with the system's free()
         *  function by the caller, so be sure to explicitly malloc() each
         *  chunk. Omit symlinks if (omitSymLinks) is non-zero.
         * If you have a memory failure, return as much as you can.
         *  This dirname is in platform-independent notation.
         */
    LinkedStringList *(*enumerateFiles)(DirHandle *r,
                                        const char *dirname,
                                        int omitSymLinks);


        /*
         * Returns non-zero if filename can be opened for reading.
         *  This filename is in platform-independent notation.
         */
    int (*exists)(DirHandle *r, const char *name);

        /*
         * Returns non-zero if filename is really a directory.
         *  This filename is in platform-independent notation.
         */
    int (*isDirectory)(DirHandle *r, const char *name);

        /*
         * Returns non-zero if filename is really a symlink.
         *  This filename is in platform-independent notation.
         */
    int (*isSymLink)(DirHandle *r, const char *name);

        /*
	     * Retrieve the last modification time (mtime) of a file.
    	 *  Returns -1 on failure, or the file's mtime in seconds since
    	 *  the epoch (Jan 1, 1970) on success.
         *  This filename is in platform-independent notation.
    	 */
    PHYSFS_sint64 (*getLastModTime)(DirHandle *r, const char *filename);

        /*
         * Open file for reading, and return a FileHandle.
         *  This filename is in platform-independent notation.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Fail if the file does not exist.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    FileHandle *(*openRead)(DirHandle *r, const char *filename);

        /*
         * Open file for writing, and return a FileHandle.
         * If the file does not exist, it should be created. If it exists,
         *  it should be truncated to zero bytes. The writing
         *  offset should be the start of the file.
         * This filename is in platform-independent notation.
         *  This method may be NULL.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    FileHandle *(*openWrite)(DirHandle *r, const char *filename);

        /*
         * Open file for appending, and return a FileHandle.
         * If the file does not exist, it should be created. The writing
         *  offset should be the end of the file.
         * This filename is in platform-independent notation.
         *  This method may be NULL.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    FileHandle *(*openAppend)(DirHandle *r, const char *filename);

        /*
         * Delete a file in the archive/directory.
         *  Return non-zero on success, zero on failure.
         *  This filename is in platform-independent notation.
         *  This method may be NULL.
         * On failure, call __PHYSFS_setError().
         */
    int (*remove)(DirHandle *r, const char *filename);

        /*
         * Create a directory in the archive/directory.
         *  If the application is trying to make multiple dirs, PhysicsFS
         *  will split them up into multiple calls before passing them to
         *  your driver.
         *  Return non-zero on success, zero on failure.
         *  This filename is in platform-independent notation.
         *  This method may be NULL.
         * On failure, call __PHYSFS_setError().
         */
    int (*mkdir)(DirHandle *r, const char *filename);

        /*
         * Close directories/archives, and free the handle, including
         *  the "opaque" entry. This should assume that it won't be called if
         *  there are still files open from this DirHandle.
         */
    void (*dirClose)(DirHandle *r);
} DirFunctions;


/* error messages... */
#define ERR_IS_INITIALIZED       "Already initialized"
#define ERR_NOT_INITIALIZED      "Not initialized"
#define ERR_INVALID_ARGUMENT     "Invalid argument"
#define ERR_FILES_STILL_OPEN     "Files still open"
#define ERR_NO_DIR_CREATE        "Failed to create directories"
#define ERR_OUT_OF_MEMORY        "Out of memory"
#define ERR_NOT_IN_SEARCH_PATH   "No such entry in search path"
#define ERR_NOT_SUPPORTED        "Operation not supported"
#define ERR_UNSUPPORTED_ARCHIVE  "Archive type unsupported"
#define ERR_NOT_A_HANDLE         "Not a file handle"
#define ERR_INSECURE_FNAME       "Insecure filename"
#define ERR_SYMLINK_DISALLOWED   "Symbolic links are disabled"
#define ERR_NO_WRITE_DIR         "Write directory is not set"
#define ERR_NO_SUCH_FILE         "No such file"
#define ERR_PAST_EOF             "Past end of file"
#define ERR_ARC_IS_READ_ONLY     "Archive is read-only"
#define ERR_IO_ERROR             "I/O error"
#define ERR_CANT_SET_WRITE_DIR   "Can't set write directory"
#define ERR_SYMLINK_LOOP         "Infinite symbolic link loop"
#define ERR_COMPRESSION          "(De)compression error"
#define ERR_NOT_IMPLEMENTED      "Not implemented"
#define ERR_OS_ERROR             "Operating system reported error"
#define ERR_FILE_EXISTS          "File already exists"
#define ERR_NOT_A_DIR            "Not a directory"
#define ERR_FILE_NOT_FOUND       "File not found"
#define ERR_NOT_AN_ARCHIVE       "Not an archive"
#define ERR_CORRUPTED            "Corrupted archive"


/*
 * Call this to set the message returned by PHYSFS_getLastError().
 *  Please only use the ERR_* constants above, or add new constants to the
 *  above group, but I want these all in one place.
 *
 * Calling this with a NULL argument is a safe no-op.
 */
void __PHYSFS_setError(const char *err);


/*
 * Convert (dirName) to platform-dependent notation, then prepend (prepend)
 *  and append (append) to the converted string.
 *
 *  So, on Win32, calling:
 *     __PHYSFS_convertToDependent("C:\", "my/files", NULL);
 *  ...will return the string "C:\my\files".
 *
 * This is a convenience function; you might want to hack something out that
 *  is less generic (and therefore more efficient).
 *
 * Be sure to free() the return value when done with it.
 */
char *__PHYSFS_convertToDependent(const char *prepend,
                                  const char *dirName,
                                  const char *append);

/*
 * Verify that (fname) (in platform-independent notation), in relation
 *  to (h) is secure. That means that each element of fname is checked
 *  for symlinks (if they aren't permitted). Also, elements such as
 *  ".", "..", or ":" are flagged.
 *
 * Returns non-zero if string is safe, zero if there's a security issue.
 *  PHYSFS_getLastError() will specify what was wrong.
 */
int __PHYSFS_verifySecurity(DirHandle *h, const char *fname);


/* These get used all over for lessening code clutter. */
#define BAIL_MACRO(e, r) { __PHYSFS_setError(e); return r; }
#define BAIL_IF_MACRO(c, e, r) if (c) { __PHYSFS_setError(e); return r; }
#define BAIL_MACRO_MUTEX(e, m, r) { __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); return r; }
#define BAIL_IF_MACRO_MUTEX(c, e, m, r) if (c) { __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); return r; }




/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------                                              ----------------*/
/*------------  You MUST implement the following functions  ----------------*/
/*------------        if porting to a new platform.         ----------------*/
/*------------     (see platform/unix.c for an example)     ----------------*/
/*------------                                              ----------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/


/*
 * The dir separator; "/" on unix, "\\" on win32, ":" on MacOS, etc...
 *  Obviously, this isn't a function, but it IS a null-terminated string.
 */
extern const char *__PHYSFS_platformDirSeparator;


/*
 * Initialize the platform. This is called when PHYSFS_init() is called from
 *  the application. You can use this to (for example) determine what version
 *  of Windows you're running.
 *
 * Return zero if there was a catastrophic failure (which prevents you from
 *  functioning at all), and non-zero otherwise.
 */
int __PHYSFS_platformInit(void);


/*
 * Deinitialize the platform. This is called when PHYSFS_deinit() is called
 *  from the application. You can use this to clean up anything you've
 *  allocated in your platform driver.
 *
 * Return zero if there was a catastrophic failure (which prevents you from
 *  functioning at all), and non-zero otherwise.
 */
int __PHYSFS_platformDeinit(void);


/*
 * Open a file for reading. (filename) is in platform-dependent notation. The
 *  file pointer should be positioned on the first byte of the file.
 *
 * The return value will be some platform-specific datatype that is opaque to
 *  the caller; it could be a (FILE *) under Unix, or a (HANDLE *) under win32.
 *
 * The same file can be opened for read multiple times, and each should have
 *  a unique file handle; this is frequently employed to prevent race
 *  conditions in the archivers.
 *
 * Call __PHYSFS_setError() and return (NULL) if the file can't be opened.
 */
void *__PHYSFS_platformOpenRead(const char *filename);


/*
 * Open a file for writing. (filename) is in platform-dependent notation. If
 *  the file exists, it should be truncated to zero bytes, and if it doesn't
 *  exist, it should be created as a zero-byte file. The file pointer should
 *  be positioned on the first byte of the file.
 *
 * The return value will be some platform-specific datatype that is opaque to
 *  the caller; it could be a (FILE *) under Unix, or a (HANDLE *) under win32,
 *  etc.
 *
 * Opening a file for write multiple times has undefined results.
 *
 * Call __PHYSFS_setError() and return (NULL) if the file can't be opened.
 */
void *__PHYSFS_platformOpenWrite(const char *filename);


/*
 * Open a file for appending. (filename) is in platform-dependent notation. If
 *  the file exists, the file pointer should be place just past the end of the
 *  file, so that the first write will be one byte after the current end of
 *  the file. If the file doesn't exist, it should be created as a zero-byte
 *  file. The file pointer should be positioned on the first byte of the file.
 *
 * The return value will be some platform-specific datatype that is opaque to
 *  the caller; it could be a (FILE *) under Unix, or a (HANDLE *) under win32,
 *  etc.
 *
 * Opening a file for append multiple times has undefined results.
 *
 * Call __PHYSFS_setError() and return (NULL) if the file can't be opened.
 */
void *__PHYSFS_platformOpenAppend(const char *filename);


/*
 * Read more data from a platform-specific file handle. (opaque) should be
 *  cast to whatever data type your platform uses. Read a maximum of (count)
 *  objects of (size) 8-bit bytes to the area pointed to by (buffer). If there
 *  isn't enough data available, return the number of full objects read, and
 *  position the file pointer at the start of the first incomplete object.
 *  On success, return (count) and position the file pointer one byte past
 *  the end of the last read object. Return (-1) if there is a catastrophic
 *  error, and call __PHYSFS_setError() to describe the problem; the file
 *  pointer should not move in such a case.
 */
PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count);

/*
 * Write more data to a platform-specific file handle. (opaque) should be
 *  cast to whatever data type your platform uses. Write a maximum of (count)
 *  objects of (size) 8-bit bytes from the area pointed to by (buffer). If
 *  there isn't enough data available, return the number of full objects
 *  written, and position the file pointer at the start of the first
 *  incomplete object. Return (-1) if there is a catastrophic error, and call
 *  __PHYSFS_setError() to describe the problem; the file pointer should not
 *  move in such a case.
 */
PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count);

/*
 * Set the file pointer to a new position. (opaque) should be cast to
 *  whatever data type your platform uses. (pos) specifies the number
 *  of 8-bit bytes to seek to from the start of the file. Seeking past the
 *  end of the file is an error condition, and you should check for it.
 *
 * Not all file types can seek; this is to be expected by the caller.
 *
 * On error, call __PHYSFS_setError() and return zero. On success, return
 *  a non-zero value.
 */
int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos);


/*
 * Get the file pointer's position, in an 8-bit byte offset from the start of
 *  the file. (opaque) should be cast to whatever data type your platform
 *  uses.
 *
 * Not all file types can "tell"; this is to be expected by the caller.
 *
 * On error, call __PHYSFS_setError() and return zero. On success, return
 *  a non-zero value.
 */
PHYSFS_sint64 __PHYSFS_platformTell(void *opaque);


/*
 * Determine the current size of a file, in 8-bit bytes, from an open file.
 *
 * The caller expects that this information may not be available for all
 *  file types on all platforms.
 *
 * Return -1 if you can't do it, and call __PHYSFS_setError(). Otherwise,
 *  return the file length in 8-bit bytes.
 */
PHYSFS_sint64 __PHYSFS_platformFileLength(void *handle);

/*
 * Determine if a file is at EOF. (opaque) should be cast to whatever data
 *  type your platform uses.
 *
 * The caller expects that there was a short read before calling this.
 *
 * Return non-zero if EOF, zero if it is _not_ EOF.
 */
int __PHYSFS_platformEOF(void *opaque);

/*
 * Flush any pending writes to disk. (opaque) should be cast to whatever data
 *  type your platform uses. Be sure to check for errors; the caller expects
 *  that this function can fail if there was a flushing error, etc.
 *
 *  Return zero on failure, non-zero on success.
 */
int __PHYSFS_platformFlush(void *opaque);

/*
 * Flush and close a file. (opaque) should be cast to whatever data type
 *  your platform uses. Be sure to check for errors when closing; the
 *  caller expects that this function can fail if there was a flushing
 *  error, etc.
 *
 * You should clean up all resources associated with (opaque).
 *
 *  Return zero on failure, non-zero on success.
 */
int __PHYSFS_platformClose(void *opaque);

/*
 * Platform implementation of PHYSFS_getCdRomDirs()...
 *  See physfs.h. The retval should be freeable via PHYSFS_freeList().
 */
char **__PHYSFS_platformDetectAvailableCDs(void);

/*
 * Calculate the base dir, if your platform needs special consideration.
 *  Just return NULL if the standard routines will suffice. (see
 *  calculateBaseDir() in physfs.c ...)
 *  Caller will free() the retval if it's not NULL.
 */
char *__PHYSFS_platformCalcBaseDir(const char *argv0);

/*
 * Get the platform-specific user name.
 *  Caller will free() the retval if it's not NULL. If it's NULL, the username
 *  will default to "default".
 */
char *__PHYSFS_platformGetUserName(void);

/*
 * Get the platform-specific user dir.
 *  Caller will free() the retval if it's not NULL. If it's NULL, the userdir
 *  will default to basedir/username.
 */
char *__PHYSFS_platformGetUserDir(void);

/*
 * Return a number that uniquely identifies the current thread.
 *  On a platform without threading, (1) will suffice. These numbers are
 *  arbitrary; the only requirement is that no two threads have the same
 *  number.
 */
PHYSFS_uint64 __PHYSFS_platformGetThreadID(void);

/*
 * This is a pass-through to whatever stricmp() is called on your platform.
 */
int __PHYSFS_platformStricmp(const char *str1, const char *str2);

/*
 * Return non-zero if filename (in platform-dependent notation) exists.
 *  Symlinks should be followed; if what the symlink points to is missing,
 *  then the retval is false.
 */
int __PHYSFS_platformExists(const char *fname);

/*
 * Return the last modified time (in seconds since the epoch) of a file.
 *  Returns -1 on failure. (fname) is in platform-dependent notation.
 *  Symlinks should be followed; if what the symlink points to is missing,
 *  then the retval is -1.
 */
PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname);


/*
 * Return non-zero if filename (in platform-dependent notation) is a symlink.
 */
int __PHYSFS_platformIsSymLink(const char *fname);


/*
 * Return non-zero if filename (in platform-dependent notation) is a symlink.
 *  Symlinks should be followed; if what the symlink points to is missing,
 *  or isn't a directory, then the retval is false.
 */
int __PHYSFS_platformIsDirectory(const char *fname);


/*
 * Convert (dirName) to platform-dependent notation, then prepend (prepend)
 *  and append (append) to the converted string.
 *
 *  So, on Win32, calling:
 *     __PHYSFS_platformCvtToDependent("C:\", "my/files", NULL);
 *  ...will return the string "C:\my\files".
 *
 * This can be implemented in a platform-specific manner, so you can get
 *  get a speed boost that the default implementation can't, since
 *  you can make assumptions about the size of strings, etc..
 *
 * Platforms that choose not to implement this may just call
 *  __PHYSFS_convertToDependent() as a passthrough, which may fit the bill
 *  already.
 *
 * Be sure to free() the return value when done with it.
 */
char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append);


/*
 * Make the current thread give up a timeslice. This is called in a loop
 *  while waiting for various external forces to get back to us.
 */
void __PHYSFS_platformTimeslice(void);


/*
 * Enumerate a directory of files. This follows the rules for the
 *  DirFunctions->enumerateFiles() method (see above), except that the
 *  (dirName) that is passed to this function is converted to
 *  platform-DEPENDENT notation by the caller. The DirFunctions version
 *  uses platform-independent notation. Note that ".", "..", and other
 *  metaentries should always be ignored.
 */
LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks);


/*
 * Get the current working directory. The return value should be an
 *  absolute path in platform-dependent notation. The caller will deallocate
 *  the return value with the standard C runtime free() function when it
 *  is done with it.
 * On error, return NULL and set the error message.
 */
char *__PHYSFS_platformCurrentDir(void);


/*
 * Get the real physical path to a file. (path) is specified in
 *  platform-dependent notation, as should your return value be.
 *  All relative paths should be removed, leaving you with an absolute
 *  path. Symlinks should be resolved, too, so that the returned value is
 *  the most direct path to a file.
 * The return value will be deallocated with the standard C runtime free()
 *  function when the caller is done with it.
 * On error, return NULL and set the error message.
 */
char *__PHYSFS_platformRealPath(const char *path);


/*
 * Make a directory in the actual filesystem. (path) is specified in
 *  platform-dependent notation. On error, return zero and set the error
 *  message. Return non-zero on success.
 */
int __PHYSFS_platformMkDir(const char *path);


/*
 * Remove a file or directory entry in the actual filesystem. (path) is
 *  specified in platform-dependent notation. Note that this deletes files
 *  _and_ directories, so you might need to do some determination.
 *  Non-empty directories should report an error and not delete themselves
 *  or their contents.
 *
 * Deleting a symlink should remove the link, not what it points to.
 *
 * On error, return zero and set the error message. Return non-zero on success.
 */
int __PHYSFS_platformDelete(const char *path);


/*
 * Create a platform-specific mutex. This can be whatever datatype your
 *  platform uses for mutexes, but it is cast to a (void *) for abstractness.
 *
 * Return (NULL) if you couldn't create one. Systems without threads can
 *  return any arbitrary non-NULL value.
 */
void *__PHYSFS_platformCreateMutex(void);

/*
 * Destroy a platform-specific mutex, and clean up any resources associated
 *  with it. (mutex) is a value previously returned by
 *  __PHYSFS_platformCreateMutex(). This can be a no-op on single-threaded
 *  platforms.
 */
void __PHYSFS_platformDestroyMutex(void *mutex);

/*
 * Grab possession of a platform-specific mutex. Mutexes should be recursive;
 *  that is, the same thread should be able to call this function multiple
 *  times in a row without causing a deadlock. This function should block 
 *  until a thread can gain possession of the mutex.
 *
 * Return non-zero if the mutex was grabbed, zero if there was an 
 *  unrecoverable problem grabbing it (this should not be a matter of 
 *  timing out! We're talking major system errors; block until the mutex 
 *  is available otherwise.)
 *
 * _DO NOT_ call __PHYSFS_setError() in here! Since setError calls this
 *  function, you'll cause an infinite recursion. This means you can't
 *  use the BAIL_*MACRO* macros, either.
 */
int __PHYSFS_platformGrabMutex(void *mutex);

/*
 * Relinquish possession of the mutex when this method has been called 
 *  once for each time that platformGrabMutex was called. Once possession has
 *  been released, the next thread in line to grab the mutex (if any) may
 *  proceed.
 *
 * _DO NOT_ call __PHYSFS_setError() in here! Since setError calls this
 *  function, you'll cause an infinite recursion. This means you can't
 *  use the BAIL_*MACRO* macros, either.
 */
void __PHYSFS_platformReleaseMutex(void *mutex);

#ifdef __cplusplus
}
#endif

#endif

/* end of physfs_internal.h ... */

