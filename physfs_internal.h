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

struct __PHYSFS_DIRREADER__;
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
    const struct __PHYSFS_DIRREADER__ *dirReader;

        /*
         * Pointer to the file i/o functions for this filehandle.
         */
    const struct __PHYSFS_FILEFUNCTIONS__ *funcs;
} FileHandle;


typedef struct __PHYSFS_FILEFUNCTIONS__
{
        /*
         * Read more from the file.
         */
    int (*read)(FileHandle *handle, void *buffer,
                unsigned int objSize, unsigned int objCount);

        /*
         * Write more to the file. Archives don't have to implement this.
         *  (Set it to NULL if not implemented).
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
         */
    int (*seek)(FileHandle *handle, int offset);

        /*
         * Close the file, and free the FileHandle structure (including "opaque").
         */
    int (*close)(FileHandle *handle);
} FileFunctions;


typedef struct __PHYSFS_DIRREADER__
{
        /*
         * This is reserved for the driver to store information.
         */
    void *opaque;

        /*
         * Pointer to the directory i/o functions for this reader.
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
         *  notation.
         */
    int (*isArchive)(const char *filename);

        /*
         * Return a DirHandle for dir/archive (name).
         *  This filename is in platform-dependent notation.
         *  return (NULL) on error.
         */
    DirHandle *(*openArchive)(const char *name);

        /*
         * Returns a list (freeable via PHYSFS_freeList()) of
         *  all files in dirname.
         *  This dirname is in platform-independent notation.
         */
    char **(*enumerateFiles)(DirHandle *r, const char *dirname);

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
         * Returns non-zero if filename can be opened for reading.
         *  This filename is in platform-independent notation.
         */
    int (*isOpenable)(DirHandle *r, const char *name);

        /*
         * Open file for reading, and return a FileHandle.
         *  This filename is in platform-independent notation.
         */
    FileHandle *(*openRead)(DirHandle *r, const char *filename);

        /*
         * Open file for writing, and return a FileHandle.
         *  This filename is in platform-independent notation.
         *  This method may be NULL.
         */
    FileHandle *(*openWrite)(DirHandle *r, const char *filename);

        /*
         * Open file for appending, and return a FileHandle.
         *  This filename is in platform-independent notation.
         *  This method may be NULL.
         */
    FileHandle *(*openAppend)(DirHandle *r, const char *filename);

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
#define ERR_FILES_OPEN_READ      "Files still open for reading"
#define ERR_FILES_OPEN_WRITE     "Files still open for writing"
#define ERR_NO_DIR_CREATE        "Failed to create directories"
#define ERR_OUT_OF_MEMORY        "Out of memory"
#define ERR_NOT_IN_SEARCH_PATH   "No such entry in search path"
#define ERR_NOT_SUPPORTED        "Operation not supported"
#define ERR_UNSUPPORTED_ARCHIVE  "Archive type unsupported"


/*
 * Call this to set the message returned by PHYSFS_getLastError().
 *  Please only use the ERR_* constants above, or add new constants to the
 *  above group, but I want these all in one place.
 */
void __PHYSFS_setError(const char *err);


/* This gets used all over for lessening code clutter. */
#define BAIL_IF_MACRO(c, e, r) if (c) { __PHYSFS_setError(e); return(r); }




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
extern const char *__PHYSFS_PlatformDirSeparator;

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


#ifdef __cplusplus
extern "C" {
#endif

#endif

/* end of physfs_internal.h ... */

