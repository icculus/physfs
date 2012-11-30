/*
 * Internal function/structure declaration. Do NOT include in your
 *  application.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef _INCLUDE_PHYSFS_INTERNAL_H_
#define _INCLUDE_PHYSFS_INTERNAL_H_

#ifndef __PHYSICSFS_INTERNAL__
#error Do not include this header from your applications.
#endif

#include "physfs.h"

/* The holy trinity. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "physfs_platforms.h"

#include <assert.h>

/* !!! FIXME: remove this when revamping stack allocation code... */
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <malloc.h>
#endif

#if PHYSFS_PLATFORM_SOLARIS
#include <alloca.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
#define PHYSFS_MINIMUM_GCC_VERSION(major, minor) \
    ( ((__GNUC__ << 16) + __GNUC_MINOR__) >= (((major) << 16) + (minor)) )
#else
#define PHYSFS_MINIMUM_GCC_VERSION(major, minor) (0)
#endif

#ifdef __cplusplus
    /* C++ always has a real inline keyword. */
#elif (defined macintosh) && !(defined __MWERKS__)
#   define inline
#elif (defined _MSC_VER)
#   define inline __inline
#endif

#if PHYSFS_PLATFORM_LINUX && !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

/*
 * Interface for small allocations. If you need a little scratch space for
 *  a throwaway buffer or string, use this. It will make small allocations
 *  on the stack if possible, and use allocator.Malloc() if they are too
 *  large. This helps reduce malloc pressure.
 * There are some rules, though:
 * NEVER return a pointer from this, as stack-allocated buffers go away
 *  when your function returns.
 * NEVER allocate in a loop, as stack-allocated pointers will pile up. Call
 *  a function that uses smallAlloc from your loop, so the allocation can
 *  free each time.
 * NEVER call smallAlloc with any complex expression (it's a macro that WILL
 *  have side effects...it references the argument multiple times). Use a
 *  variable or a literal.
 * NEVER free a pointer from this with anything but smallFree. It will not
 *  be a valid pointer to the allocator, regardless of where the memory came
 *  from.
 * NEVER realloc a pointer from this.
 * NEVER forget to use smallFree: it may not be a pointer from the stack.
 * NEVER forget to check for NULL...allocation can fail here, of course!
 */
#define __PHYSFS_SMALLALLOCTHRESHOLD 256
void *__PHYSFS_initSmallAlloc(void *ptr, PHYSFS_uint64 len);

#define __PHYSFS_smallAlloc(bytes) ( \
    __PHYSFS_initSmallAlloc( \
        (((bytes) < __PHYSFS_SMALLALLOCTHRESHOLD) ? \
            alloca((size_t)((bytes)+sizeof(void*))) : NULL), (bytes)) \
)

void __PHYSFS_smallFree(void *ptr);


/* Use the allocation hooks. */
#define malloc(x) Do not use malloc() directly.
#define realloc(x, y) Do not use realloc() directly.
#define free(x) Do not use free() directly.
/* !!! FIXME: add alloca check here. */

#ifndef PHYSFS_SUPPORTS_ZIP
#define PHYSFS_SUPPORTS_ZIP 1
#endif
#ifndef PHYSFS_SUPPORTS_7Z
#define PHYSFS_SUPPORTS_7Z 0
#endif
#ifndef PHYSFS_SUPPORTS_GRP
#define PHYSFS_SUPPORTS_GRP 0
#endif
#ifndef PHYSFS_SUPPORTS_HOG
#define PHYSFS_SUPPORTS_HOG 0
#endif
#ifndef PHYSFS_SUPPORTS_MVL
#define PHYSFS_SUPPORTS_MVL 0
#endif
#ifndef PHYSFS_SUPPORTS_WAD
#define PHYSFS_SUPPORTS_WAD 0
#endif
#ifndef PHYSFS_SUPPORTS_SLB
#define PHYSFS_SUPPORTS_SLB 0
#endif
#ifndef PHYSFS_SUPPORTS_ISO9660
#define PHYSFS_SUPPORTS_ISO9660 0
#endif

/* The latest supported PHYSFS_Io::version value. */
#define CURRENT_PHYSFS_IO_API_VERSION 0

/* The latest supported PHYSFS_Archiver::version value. */
#define CURRENT_PHYSFS_ARCHIVER_API_VERSION 0

/* !!! FIXME: update this documentation.
 * Call this to set the message returned by PHYSFS_getLastError().
 *  Please only use the ERR_* constants above, or add new constants to the
 *  above group, but I want these all in one place.
 *
 * Calling this with a NULL argument is a safe no-op.
 */
void __PHYSFS_setError(const PHYSFS_ErrorCode err);


/* This byteorder stuff was lifted from SDL. http://www.libsdl.org/ */
#define PHYSFS_LIL_ENDIAN  1234
#define PHYSFS_BIG_ENDIAN  4321

#if  defined(__i386__) || defined(__ia64__) || \
     defined(_M_IX86) || defined(_M_IA64) || defined(_M_X64) || \
    (defined(__alpha__) || defined(__alpha)) || \
     defined(__arm__) || defined(ARM) || \
    (defined(__mips__) && defined(__MIPSEL__)) || \
     defined(__SYMBIAN32__) || \
     defined(__x86_64__) || \
     defined(__LITTLE_ENDIAN__)
#define PHYSFS_BYTEORDER    PHYSFS_LIL_ENDIAN
#else
#define PHYSFS_BYTEORDER    PHYSFS_BIG_ENDIAN
#endif


/*
 * When sorting the entries in an archive, we use a modified QuickSort.
 *  When there are less then PHYSFS_QUICKSORT_THRESHOLD entries left to sort,
 *  we switch over to a BubbleSort for the remainder. Tweak to taste.
 *
 * You can override this setting by defining PHYSFS_QUICKSORT_THRESHOLD
 *  before #including "physfs_internal.h".
 */
#ifndef PHYSFS_QUICKSORT_THRESHOLD
#define PHYSFS_QUICKSORT_THRESHOLD 4
#endif

/*
 * Sort an array (or whatever) of (max) elements. This uses a mixture of
 *  a QuickSort and BubbleSort internally.
 * (cmpfn) is used to determine ordering, and (swapfn) does the actual
 *  swapping of elements in the list.
 *
 *  See zip.c for an example.
 */
void __PHYSFS_sort(void *entries, size_t max,
                   int (*cmpfn)(void *, size_t, size_t),
                   void (*swapfn)(void *, size_t, size_t));

/*
 * This isn't a formal error code, it's just for BAIL_MACRO.
 *  It means: there was an error, but someone else already set it for us.
 */
#define ERRPASS PHYSFS_ERR_OK

/* These get used all over for lessening code clutter. */
#define BAIL_MACRO(e, r) do { if (e) __PHYSFS_setError(e); return r; } while (0)
#define BAIL_IF_MACRO(c, e, r) do { if (c) { if (e) __PHYSFS_setError(e); return r; } } while (0)
#define BAIL_MACRO_MUTEX(e, m, r) do { if (e) __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); return r; } while (0)
#define BAIL_IF_MACRO_MUTEX(c, e, m, r) do { if (c) { if (e) __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); return r; } } while (0)
#define GOTO_MACRO(e, g) do { if (e) __PHYSFS_setError(e); goto g; } while (0)
#define GOTO_IF_MACRO(c, e, g) do { if (c) { if (e) __PHYSFS_setError(e); goto g; } } while (0)
#define GOTO_MACRO_MUTEX(e, m, g) do { if (e) __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); goto g; } while (0)
#define GOTO_IF_MACRO_MUTEX(c, e, m, g) do { if (c) { if (e) __PHYSFS_setError(e); __PHYSFS_platformReleaseMutex(m); goto g; } } while (0)

#define __PHYSFS_ARRAYLEN(x) ( (sizeof (x)) / (sizeof (x[0])) )

#ifdef PHYSFS_NO_64BIT_SUPPORT
#define __PHYSFS_SI64(x) ((PHYSFS_sint64) (x))
#define __PHYSFS_UI64(x) ((PHYSFS_uint64) (x))
#elif (defined __GNUC__)
#define __PHYSFS_SI64(x) x##LL
#define __PHYSFS_UI64(x) x##ULL
#elif (defined _MSC_VER)
#define __PHYSFS_SI64(x) x##i64
#define __PHYSFS_UI64(x) x##ui64
#else
#define __PHYSFS_SI64(x) ((PHYSFS_sint64) (x))
#define __PHYSFS_UI64(x) ((PHYSFS_uint64) (x))
#endif


/*
 * Check if a ui64 will fit in the platform's address space.
 *  The initial sizeof check will optimize this macro out entirely on
 *  64-bit (and larger?!) platforms, and the other condition will
 *  return zero or non-zero if the variable will fit in the platform's
 *  size_t, suitable to pass to malloc. This is kinda messy, but effective.
 */
#define __PHYSFS_ui64FitsAddressSpace(s) ( \
    (sizeof (PHYSFS_uint64) <= sizeof (size_t)) || \
    ((s) < (__PHYSFS_UI64(0xFFFFFFFFFFFFFFFF) >> (64-(sizeof(size_t)*8)))) \
)


/*
 * This is a strcasecmp() or stricmp() replacement that expects both strings
 *  to be in UTF-8 encoding. It will do "case folding" to decide if the
 *  Unicode codepoints in the strings match.
 *
 * It will report which string is "greater than" the other, but be aware that
 *  this doesn't necessarily mean anything: 'a' may be "less than" 'b', but
 *  a random Kanji codepoint has no meaningful alphabetically relationship to
 *  a Greek Lambda, but being able to assign a reliable "value" makes sorting
 *  algorithms possible, if not entirely sane. Most cases should treat the
 *  return value as "equal" or "not equal".
 */
int __PHYSFS_utf8stricmp(const char *s1, const char *s2);

/*
 * This works like __PHYSFS_utf8stricmp(), but takes a character (NOT BYTE
 *  COUNT) argument, like strcasencmp().
 */
int __PHYSFS_utf8strnicmp(const char *s1, const char *s2, PHYSFS_uint32 l);

/*
 * stricmp() that guarantees to only work with low ASCII. The C runtime
 *  stricmp() might try to apply a locale/codepage/etc, which we don't want.
 */
int __PHYSFS_stricmpASCII(const char *s1, const char *s2);

/*
 * strnicmp() that guarantees to only work with low ASCII. The C runtime
 *  strnicmp() might try to apply a locale/codepage/etc, which we don't want.
 */
int __PHYSFS_strnicmpASCII(const char *s1, const char *s2, PHYSFS_uint32 l);


/*
 * The current allocator. Not valid before PHYSFS_init is called!
 */
extern PHYSFS_Allocator __PHYSFS_AllocatorHooks;

/* convenience macro to make this less cumbersome internally... */
#define allocator __PHYSFS_AllocatorHooks

/*
 * Create a PHYSFS_Io for a file in the physical filesystem.
 *  This path is in platform-dependent notation. (mode) must be 'r', 'w', or
 *  'a' for Read, Write, or Append.
 */
PHYSFS_Io *__PHYSFS_createNativeIo(const char *path, const int mode);

/*
 * Create a PHYSFS_Io for a buffer of memory (READ-ONLY). If you already
 *  have one of these, just use its duplicate() method, and it'll increment
 *  its refcount without allocating a copy of the buffer.
 */
PHYSFS_Io *__PHYSFS_createMemoryIo(const void *buf, PHYSFS_uint64 len,
                                   void (*destruct)(void *));


/*
 * Read (len) bytes from (io) into (buf). Returns non-zero on success,
 *  zero on i/o error. Literally: "return (io->read(io, buf, len) == len);"
 */
int __PHYSFS_readAll(PHYSFS_Io *io, void *buf, const PHYSFS_uint64 len);


/* These are shared between some archivers. */

typedef struct
{
    char name[64];
    PHYSFS_uint32 startPos;
    PHYSFS_uint32 size;
} UNPKentry;

void UNPK_closeArchive(void *opaque);
void *UNPK_openArchive(PHYSFS_Io *io,UNPKentry *e,const PHYSFS_uint32 n);
void UNPK_enumerateFiles(void *opaque, const char *dname,
                         PHYSFS_EnumFilesCallback cb,
                         const char *origdir, void *callbackdata);
PHYSFS_Io *UNPK_openRead(void *opaque, const char *fnm, int *fileExists);
PHYSFS_Io *UNPK_openWrite(void *opaque, const char *name);
PHYSFS_Io *UNPK_openAppend(void *opaque, const char *name);
int UNPK_remove(void *opaque, const char *name);
int UNPK_mkdir(void *opaque, const char *name);
int UNPK_stat(void *opaque, const char *fn, PHYSFS_Stat *st);


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
 * The dir separator; '/' on unix, '\\' on win32, ":" on MacOS, etc...
 *  Obviously, this isn't a function. If you need more than one char for this,
 *  you'll need to pull some old pieces of PhysicsFS out of revision control.
 */
#if PHYSFS_PLATFORM_WINDOWS
#define __PHYSFS_platformDirSeparator '\\'
#else
#define __PHYSFS_platformDirSeparator '/'
#endif

/*
 * Initialize the platform. This is called when PHYSFS_init() is called from
 *  the application.
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
 *  cast to whatever data type your platform uses. Read a maximum of (len)
 *  8-bit bytes to the area pointed to by (buf). If there isn't enough data
 *  available, return the number of bytes read, and position the file pointer
 *  immediately after those bytes.
 *  On success, return (len) and position the file pointer immediately past
 *  the end of the last read byte. Return (-1) if there is a catastrophic
 *  error, and call __PHYSFS_setError() to describe the problem; the file
 *  pointer should not move in such a case. A partial read is success; only
 *  return (-1) on total failure; presumably, the next read call after a
 *  partial read will fail as such.
 */
PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buf, PHYSFS_uint64 len);

/*
 * Write more data to a platform-specific file handle. (opaque) should be
 *  cast to whatever data type your platform uses. Write a maximum of (len)
 *  8-bit bytes from the area pointed to by (buffer). If there is a problem,
 *  return the number of bytes written, and position the file pointer
 *  immediately after those bytes. Return (-1) if there is a catastrophic
 *  error, and call __PHYSFS_setError() to describe the problem; the file
 *  pointer should not move in such a case. A partial write is success; only
 *  return (-1) on total failure; presumably, the next write call after a
 *  partial write will fail as such.
 */
PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len);

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
 * On error, call __PHYSFS_setError() and return -1. On success, return >= 0.
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
 * !!! FIXME: comment me.
 */
int __PHYSFS_platformStat(const char *fn, PHYSFS_Stat *stat);

/*
 * Flush any pending writes to disk. (opaque) should be cast to whatever data
 *  type your platform uses. Be sure to check for errors; the caller expects
 *  that this function can fail if there was a flushing error, etc.
 *
 *  Return zero on failure, non-zero on success.
 */
int __PHYSFS_platformFlush(void *opaque);

/*
 * Close file and deallocate resources. (opaque) should be cast to whatever
 *  data type your platform uses. This should close the file in any scenario:
 *  flushing is a separate function call, and this function should never fail.
 *
 * You should clean up all resources associated with (opaque); the pointer
 *  will be considered invalid after this call.
 */
void __PHYSFS_platformClose(void *opaque);

/*
 * Platform implementation of PHYSFS_getCdRomDirsCallback()...
 *  CD directories are discovered and reported to the callback one at a time.
 *  Pointers passed to the callback are assumed to be invalid to the
 *  application after the callback returns, so you can free them or whatever.
 *  Callback does not assume results will be sorted in any meaningful way.
 */
void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data);

/*
 * Calculate the base dir, if your platform needs special consideration.
 *  Just return NULL if the standard routines will suffice. (see
 *  calculateBaseDir() in physfs.c ...)
 * Your string must end with a dir separator if you don't return NULL.
 *  Caller will allocator.Free() the retval if it's not NULL.
 */
char *__PHYSFS_platformCalcBaseDir(const char *argv0);

/*
 * Get the platform-specific user dir.
 * As of PhysicsFS 2.1, returning NULL means fatal error.
 * Your string must end with a dir separator if you don't return NULL.
 *  Caller will allocator.Free() the retval if it's not NULL.
 */
char *__PHYSFS_platformCalcUserDir(void);


/* This is the cached version from PHYSFS_init(). This is a fast call. */
const char *__PHYSFS_getUserDir(void);  /* not deprecated internal version. */


/*
 * Get the platform-specific pref dir.
 * Returning NULL means fatal error.
 * Your string must end with a dir separator if you don't return NULL.
 *  Caller will allocator.Free() the retval if it's not NULL.
 *  Caller will make missing directories if necessary; this just reports
 *   the final path.
 */
char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app);


/*
 * Return a pointer that uniquely identifies the current thread.
 *  On a platform without threading, (0x1) will suffice. These numbers are
 *  arbitrary; the only requirement is that no two threads have the same
 *  pointer.
 */
void *__PHYSFS_platformGetThreadID(void);


/*
 * Enumerate a directory of files. This follows the rules for the
 *  PHYSFS_Archiver::enumerateFiles() method (see above), except that the
 *  (dirName) that is passed to this function is converted to
 *  platform-DEPENDENT notation by the caller. The PHYSFS_Archiver version
 *  uses platform-independent notation. Note that ".", "..", and other
 *  meta-entries should always be ignored.
 */
void __PHYSFS_platformEnumerateFiles(const char *dirname,
                                     PHYSFS_EnumFilesCallback callback,
                                     const char *origdir,
                                     void *callbackdata);

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

/*
 * Called at the start of PHYSFS_init() to prepare the allocator, if the user
 *  hasn't selected their own allocator via PHYSFS_setAllocator().
 *  If the platform has a custom allocator, it should fill in the fields of
 *  (a) with the proper function pointers and return non-zero.
 * If the platform just wants to use malloc()/free()/etc, return zero
 *  immediately and the higher level will handle it. The Init and Deinit
 *  fields of (a) are optional...set them to NULL if you don't need them.
 *  Everything else must be implemented. All rules follow those for
 *  PHYSFS_setAllocator(). If Init isn't NULL, it will be called shortly
 *  after this function returns non-zero.
 */
int __PHYSFS_platformSetDefaultAllocator(PHYSFS_Allocator *a);

#ifdef __cplusplus
}
#endif

#endif

/* end of physfs_internal.h ... */

