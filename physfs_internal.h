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

struct __PHYSFS_DIRHANDLE__;
struct __PHYSFS_FILEFUNCTIONS__;

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
    int (*read)(FileHandle *handle, void *buffer,
                unsigned int objSize, unsigned int objCount);

        /*
         * Write more to the file. Archives don't have to implement this.
         *  (Set it to NULL if not implemented).
         * Returns number of objects of (objSize) bytes written to file, -1
         *  if complete failure.
         * On failure, call __PHYSFS_setError().
         */
    int (*write)(FileHandle *handle, void *buffer,
                 unsigned int objSize, unsigned int objCount);

        /*
         * Returns non-zero if at end of file.
         */
    int (*eof)(FileHandle *handle);

        /*
         * Returns byte offset from start of file.
         */
    int (*tell)(FileHandle *handle);

        /*
         * Move read/write pointer to byte offset from start of file.
         *  Returns non-zero on success, zero on error.
         * On failure, call __PHYSFS_setError().
         */
    int (*seek)(FileHandle *handle, int offset);

        /*
         * Close the file, and free the FileHandle structure (including "opaque").
         *  returns non-zero on success, zero if can't close file.
         * On failure, call __PHYSFS_setError().
         */
    int (*close)(FileHandle *handle);
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


typedef struct __PHYSFS_LINKEDSTRINGLIST__
{
    char *str;
    struct __PHYSFS_LINKEDSTRINGLIST__ *next;
} LinkedStringList;

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
         *  chunk.
         * If you have a memory failure, return as much as you can.
         *  This dirname is in platform-independent notation.
         */
    LinkedStringList *(*enumerateFiles)(DirHandle *r, const char *dirname);

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
         * Open file for reading, and return a FileHandle.
         *  This filename is in platform-independent notation.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    FileHandle *(*openRead)(DirHandle *r, const char *filename);

        /*
         * Open file for writing, and return a FileHandle.
         *  This filename is in platform-independent notation.
         *  This method may be NULL.
         * If you can't handle multiple opens of the same file,
         *  you can opt to fail for the second call.
         * Returns NULL on failure, and calls __PHYSFS_setError().
         */
    FileHandle *(*openWrite)(DirHandle *r, const char *filename);

        /*
         * Open file for appending, and return a FileHandle.
         *  This filename is in platform-independent notation.
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
    void (*close)(DirHandle *r);
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
 *     __PHYSFS_convertToDependentNotation("C:\", "my/files", NULL);
 *  ...will return the string "C:\my\files".
 *
 * This is a convenience function; you might want to hack something out that
 *  is less generic (and therefore more efficient).
 *
 * Be sure to free() the return value when done with it.
 */
char *__PHYSFS_convertToDependentNotation(const char *prepend,
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


/* This gets used all over for lessening code clutter. */
#define BAIL_IF_MACRO(c, e, r) if (c) { __PHYSFS_setError(e); return r; }




/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/
/*------------                                              ----------------*/
/*------------  You MUST implement the following functions  ----------------*/
/*------------        if porting to a new platform.         ----------------*/
/*------------         (see unix.c for an example)          ----------------*/
/*------------                                              ----------------*/
/*--------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------*/


/*
 * The dir separator; "/" on unix, "\\" on win32, ":" on MacOS, etc...
 *  Obviously, this isn't a function, but it IS a null-terminated string.
 */
extern const char *__PHYSFS_platformDirSeparator;

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
char *__PHYSFS_platformCalcBaseDir(char *argv0);

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
int __PHYSFS_platformGetThreadID(void);

/*
 * This is a pass-through to whatever stricmp() is called on your platform.
 */
int __PHYSFS_platformStricmp(const char *str1, const char *str2);

/*
 * Return non-zero if filename (in platform-dependent notation) is a symlink.
 */
int __PHYSFS_platformIsSymlink(const char *fname);

/*
 * Return non-zero if filename (in platform-dependent notation) is a symlink.
 */
int __PHYSFS_platformIsDirectory(const char *fname);


#ifdef __cplusplus
extern "C" {
#endif

#endif

/* end of physfs_internal.h ... */

