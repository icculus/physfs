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

typedef struct __PHYSFS_FILEHANDLE__
{
        /*
         * This is reserved for the driver to store information.
         */
    void *opaque;

        /*
         * This should be the DirReader that created this FileHandle.
         */
    struct __PHYSFS_DIRREADER__ *dirReader;

        /*
         * Read more from the file.
         */
    int (*read)(struct __PHYSFS_FILEHANDLE__ *handle, void *buffer,
                unsigned int objSize, unsigned int objCount);

        /*
         * Write more to the file. Archives don't have to implement this.
         *  (Set it to NULL if not implemented).
         */
    int (*write)(struct __PHYSFS_FILEHANDLE__ *handle, void *buffer,
                 unsigned int objSize, unsigned int objCount);

        /*
         * Returns non-zero if at end of file.
         */
    int (*eof)(struct __PHYSFS_FILEHANDLE__ *handle);

        /*
         * Returns byte offset from start of file.
         */
    int (*tell)(struct __PHYSFS_FILEHANDLE__ *handle);

        /*
         * Move read/write pointer to byte offset from start of file.
         *  Returns non-zero on success, zero on error.
         */
    int (*seek)(struct __PHYSFS_FILEHANDLE__ *handle, int offset);

        /*
         * Close the file, and free this structure (including "opaque").
         */
    int (*close)(void);
} FileHandle;


typedef struct __PHYSFS_DIRREADER__
{
        /*
         * This is reserved for the driver to store information.
         */
    void *opaque;

        /*
         * Returns a list (freeable via PHYSFS_freeList()) of
         *  all files in dirname.
         *  Symlinks should be followed.
         */
    char **(*enumerateFiles)(struct __PHYSFS_DIRREADER__ *r, const char *dirname);

        /*
         * Returns non-zero if filename is really a directory.
         *  Symlinks should be followed.
         */
    int (*isDirectory)(struct __PHYSFS_DIRREADER__ *r, const char *name);

        /*
         * Returns non-zero if filename is really a symlink.
         */
    int (*isSymLink)(struct __PHYSFS_DIRREADER__ *r, const char *name);

        /*
         * Returns non-zero if filename can be opened for reading.
         *  Symlinks should be followed.
         */
    int (*isOpenable)(struct __PHYSFS_DIRREADER__ *r, const char *name);

        /*
         * Open file for reading, and return a FileHandle.
         *  Symlinks should be followed.
         */
    FileHandle *(*openRead)(struct __PHYSFS_DIRREADER__ *r, const char *filename);

        /*
         * Close directories/archives, and free this structure, including
         *  the "opaque" entry. This should assume that it won't be called if
         *  there are still files open from this DirReader.
         */
    void (*close)(struct __PHYSFS_DIRREADER__ *r);
} DirReader;


/* error messages... */
#define ERR_IS_INITIALIZED       "Already initialized"
#define ERR_NOT_INITIALIZED      "Not initialized"
#define ERR_INVALID_ARGUMENT     "Invalid argument"
#define ERR_FILES_OPEN_WRITE     "Files still open for writing"
#define ERR_NO_DIR_CREATE        "Failed to create directories"
#define ERR_OUT_OF_MEMORY        "Out of memory"
#define ERR_NOT_IN_SEARCH_PATH   "No such entry in search path"
#define ERR_NOT_SUPPORTED        "Operation not supported"


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


#ifdef __cplusplus
extern "C" {
#endif

#endif

/* end of physfs_internal.h ... */

