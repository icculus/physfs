/** \file physfs.h */

/**
 * \mainpage PhysicsFS
 *
 * The latest version of PhysicsFS can be found at:
 *     http://icculus.org/physfs/
 *
 * PhysicsFS; a portable, flexible file i/o abstraction.
 *
 * This API gives you access to a system file system in ways superior to the
 *  stdio or system i/o calls. The brief benefits:
 *
 *   - It's portable.
 *   - It's safe. No file access is permitted outside the specified dirs.
 *   - It's flexible. Archives (.ZIP files) can be used transparently as
 *      directory structures.
 *
 * This system is largely inspired by Quake 3's PK3 files and the related
 *  fs_* cvars. If you've ever tinkered with these, then this API will be
 *  familiar to you.
 *
 * With PhysicsFS, you have a single writing directory and multiple
 *  directories (the "search path") for reading. You can think of this as a
 *  filesystem within a filesystem. If (on Windows) you were to set the
 *  writing directory to "C:\MyGame\MyWritingDirectory", then no PHYSFS calls
 *  could touch anything above this directory, including the "C:\MyGame" and
 *  "C:\" directories. This prevents an application's internal scripting
 *  language from piddling over c:\config.sys, for example. If you'd rather
 *  give PHYSFS full access to the system's REAL file system, set the writing
 *  dir to "C:\", but that's generally A Bad Thing for several reasons.
 *
 * Drive letters are hidden in PhysicsFS once you set up your initial paths.
 *  The search path creates a single, hierarchical directory structure.
 *  Not only does this lend itself well to general abstraction with archives,
 *  it also gives better support to operating systems like MacOS and Unix.
 *  Generally speaking, you shouldn't ever hardcode a drive letter; not only
 *  does this hurt portability to non-Microsoft OSes, but it limits your win32
 *  users to a single drive, too. Use the PhysicsFS abstraction functions and
 *  allow user-defined configuration options, too. When opening a file, you
 *  specify it like it was on a Unix filesystem: if you want to write to
 *  "C:\MyGame\MyConfigFiles\game.cfg", then you might set the write dir to
 *  "C:\MyGame" and then open "MyConfigFiles/game.cfg". This gives an
 *  abstraction across all platforms. Specifying a file in this way is termed
 *  "platform-independent notation" in this documentation. Specifying a
 *  a filename in a form such as "C:\mydir\myfile" or
 *  "MacOS hard drive:My Directory:My File" is termed "platform-dependent
 *  notation". The only time you use platform-dependent notation is when
 *  setting up your write directory and search path; after that, all file
 *  access into those directories are done with platform-independent notation.
 *
 * All files opened for writing are opened in relation to the write directory,
 *  which is the root of the writable filesystem. When opening a file for
 *  reading, PhysicsFS goes through the search path. This is NOT the
 *  same thing as the PATH environment variable. An application using
 *  PhysicsFS specifies directories to be searched which may be actual
 *  directories, or archive files that contain files and subdirectories of
 *  their own. See the end of these docs for currently supported archive
 *  formats.
 *
 * Once the search path is defined, you may open files for reading. If you've
 *  got the following search path defined (to use a win32 example again):
 *
 *    C:\mygame
 *    C:\mygame\myuserfiles
 *    D:\mygamescdromdatafiles
 *    C:\mygame\installeddatafiles.zip
 *
 * Then a call to PHYSFS_openRead("textfiles/myfile.txt") (note the directory
 *  separator, lack of drive letter, and lack of dir separator at the start of
 *  the string; this is platform-independent notation) will check for
 *  C:\mygame\textfiles\myfile.txt, then
 *  C:\mygame\myuserfiles\textfiles\myfile.txt, then
 *  D:\mygamescdromdatafiles\textfiles\myfile.txt, then, finally, for
 *  textfiles\myfile.txt inside of C:\mygame\installeddatafiles.zip. Remember
 *  that most archive types and platform filesystems store their filenames in
 *  a case-sensitive manner, so you should be careful to specify it correctly.
 *
 * Files opened through PhysicsFS may NOT contain "." or ".." or ":" as dir
 *  elements. Not only are these meaningless on MacOS and/or Unix, they are a
 *  security hole. Also, symbolic links (which can be found in some archive
 *  types and directly in the filesystem on Unix platforms) are NOT followed
 *  until you call PHYSFS_permitSymbolicLinks(). That's left to your own
 *  discretion, as following a symlink can allow for access outside the write
 *  dir and search paths. There is no mechanism for creating new symlinks in
 *  PhysicsFS.
 *
 * The write dir is not included in the search path unless you specifically
 *  add it. While you CAN change the write dir as many times as you like,
 *  you should probably set it once and stick to it. Remember that your
 *  program will not have permission to write in every directory on Unix and
 *  NT systems.
 *
 * All files are opened in binary mode; there is no endline conversion for
 *  textfiles. Other than that, PhysicsFS has some convenience functions for
 *  platform-independence. There is a function to tell you the current
 *  platform's dir separator ("\\" on windows, "/" on Unix, ":" on MacOS),
 *  which is needed only to set up your search/write paths. There is a
 *  function to tell you what CD-ROM drives contain accessible discs, and a
 *  function to recommend a good search path, etc.
 *
 * A recommended order for the search path is the write dir, then the base dir,
 *  then the cdrom dir, then any archives discovered. Quake 3 does something
 *  like this, but moves the archives to the start of the search path. Build
 *  Engine games, like Duke Nukem 3D and Blood, place the archives last, and
 *  use the base dir for both searching and writing. There is a helper
 *  function (PHYSFS_setSaneConfig()) that puts together a basic configuration
 *  for you, based on a few parameters. Also see the comments on
 *  PHYSFS_getBaseDir(), and PHYSFS_getUserDir() for info on what those
 *  are and how they can help you determine an optimal search path.
 *
 * PhysicsFS is mostly thread safe. The error messages returned by
 *  PHYSFS_getLastError are unique by thread, and library-state-setting
 *  functions are mutex'd. For efficiency, individual file accesses are 
 *  not locked, so you can not safely read/write/seek/close/etc the same 
 *  file from two threads at the same time. Other race conditions are bugs 
 *  that should be reported/patched.
 *
 * While you CAN use stdio/syscall file access in a program that has PHYSFS_*
 *  calls, doing so is not recommended, and you can not use system
 *  filehandles with PhysicsFS and vice versa.
 *
 * Note that archives need not be named as such: if you have a ZIP file and
 *  rename it with a .PKG extension, the file will still be recognized as a
 *  ZIP archive by PhysicsFS; the file's contents are used to determine its
 *  type.
 *
 * Currently supported archive types:
 *   - .ZIP (pkZip/WinZip/Info-ZIP compatible)
 *   - .GRP (Build Engine groupfile archives)
 *
 * Please see the file LICENSE in the source's root directory for licensing
 *  and redistribution rights.
 *
 * Please see the file CREDITS in the source's root directory for a complete
 *  list of who's responsible for this.
 *
 *  \author Ryan C. Gordon.
 */

#ifndef _INCLUDE_PHYSFS_H_
#define _INCLUDE_PHYSFS_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
#if (defined _MSC_VER)
#define __EXPORT__ __declspec(dllexport)
#else
#define __EXPORT__
#endif
#endif  /* DOXYGEN_SHOULD_IGNORE_THIS */

/**
 * \typedef PHYSFS_uint8
 * \brief An unsigned, 8-bit integer type.
 */
typedef unsigned char         PHYSFS_uint8;

/**
 * \typedef PHYSFS_sint8
 * \brief A signed, 8-bit integer type.
 */
typedef signed char           PHYSFS_sint8;

/**
 * \typedef PHYSFS_uint16
 * \brief An unsigned, 16-bit integer type.
 */
typedef unsigned short        PHYSFS_uint16;

/**
 * \typedef PHYSFS_sint16
 * \brief A signed, 16-bit integer type.
 */
typedef signed short          PHYSFS_sint16;

/**
 * \typedef PHYSFS_uint32
 * \brief An unsigned, 32-bit integer type.
 */
typedef unsigned int          PHYSFS_uint32;

/**
 * \typedef PHYSFS_sint32
 * \brief A signed, 32-bit integer type.
 */
typedef signed int            PHYSFS_sint32;

/**
 * \typedef PHYSFS_uint64
 * \brief An unsigned, 64-bit integer type.
 * \warning on platforms without any sort of 64-bit datatype, this is
 *           equivalent to PHYSFS_uint32!
 */

/**
 * \typedef PHYSFS_sint64
 * \brief A signed, 64-bit integer type.
 * \warning on platforms without any sort of 64-bit datatype, this is
 *           equivalent to PHYSFS_sint32!
 */


#if (defined PHYSFS_NO_64BIT_SUPPORT)  /* oh well. */
typedef PHYSFS_uint32         PHYSFS_uint64;
typedef PHYSFS_sint32         PHYSFS_sint64;
#elif (defined _MSC_VER)
typedef signed __int64        PHYSFS_sint64;
typedef unsigned __int64      PHYSFS_uint64;
#else
typedef unsigned long long    PHYSFS_uint64;
typedef signed long long      PHYSFS_sint64;
#endif


#ifndef DOXYGEN_SHOULD_IGNORE_THIS
/* Make sure the types really have the right sizes */
#define PHYSFS_COMPILE_TIME_ASSERT(name, x)               \
       typedef int PHYSFS_dummy_ ## name[(x) * 2 - 1]

PHYSFS_COMPILE_TIME_ASSERT(uint8, sizeof(PHYSFS_uint8) == 1);
PHYSFS_COMPILE_TIME_ASSERT(sint8, sizeof(PHYSFS_sint8) == 1);
PHYSFS_COMPILE_TIME_ASSERT(uint16, sizeof(PHYSFS_uint16) == 2);
PHYSFS_COMPILE_TIME_ASSERT(sint16, sizeof(PHYSFS_sint16) == 2);
PHYSFS_COMPILE_TIME_ASSERT(uint32, sizeof(PHYSFS_uint32) == 4);
PHYSFS_COMPILE_TIME_ASSERT(sint32, sizeof(PHYSFS_sint32) == 4);

#ifndef PHYSFS_NO_64BIT_SUPPORT
PHYSFS_COMPILE_TIME_ASSERT(uint64, sizeof(PHYSFS_uint64) == 8);
PHYSFS_COMPILE_TIME_ASSERT(sint64, sizeof(PHYSFS_sint64) == 8);
#endif

#undef PHYSFS_COMPILE_TIME_ASSERT

#endif  /* DOXYGEN_SHOULD_IGNORE_THIS */


/**
 * \struct PHYSFS_file
 * \brief A PhysicsFS file handle.
 *
 * You get a pointer to one of these when you open a file for reading,
 *  writing, or appending via PhysicsFS.
 *
 * As you can see from the lack of meaningful fields, you should treat this
 *  as opaque data. Don't try to manipulate the file handle, just pass the
 *  pointer you got, unmolested, to various PhysicsFS APIs.
 *
 * \sa PHYSFS_openRead
 * \sa PHYSFS_openWrite
 * \sa PHYSFS_openAppend
 * \sa PHYSFS_close
 * \sa PHYSFS_read
 * \sa PHYSFS_write
 * \sa PHYSFS_seek
 * \sa PHYSFS_tell
 * \sa PHYSFS_eof
 */
typedef struct
{
    void *opaque;  /**< That's all you get. Don't touch. */
} PHYSFS_file;



/**
 * \struct PHYSFS_ArchiveInfo
 * \brief Information on various PhysicsFS-supported archives.
 *
 * This structure gives you details on what sort of archives are supported
 *  by this implementation of PhysicsFS. Archives tend to be things like
 *  ZIP files and such.
 *
 * \warning Not all binaries are created equal! PhysicsFS can be built with
 *          or without support for various archives. You can check with
 *          PHYSFS_supportedArchiveTypes() to see if your archive type is
 *          supported.
 *
 * \sa PHYSFS_supportedArchiveTypes
 */
typedef struct
{
    const char *extension;   /**< Archive file extension: "ZIP", for example. */
    const char *description; /**< Human-readable archive description. */
    const char *author;      /**< Person who did support for this archive. */
    const char *url;         /**< URL related to this archive */
} PHYSFS_ArchiveInfo;

/**
 * \struct PHYSFS_Version
 * \brief Information the version of PhysicsFS in use.
 *
 * Represents the library's version as three levels: major revision
 *  (increments with massive changes, additions, and enhancements),
 *  minor revision (increments with backwards-compatible changes to the
 *  major revision), and patchlevel (increments with fixes to the minor
 *  revision).
 *
 * \sa PHYSFS_VERSION
 * \sa PHYFS_getLinkedVersion
 */
typedef struct
{
    PHYSFS_uint8 major; /**< major revision */
    PHYSFS_uint8 minor; /**< minor revision */
    PHYSFS_uint8 patch; /**< patchlevel */
} PHYSFS_Version;

#ifndef DOXYGEN_SHOULD_IGNORE_THIS
#define PHYSFS_VER_MAJOR 0
#define PHYSFS_VER_MINOR 1
#define PHYSFS_VER_PATCH 7
#endif  /* DOXYGEN_SHOULD_IGNORE_THIS */

/**
 * \def PHYSFS_VERSION(x)
 * \brief Macro to determine PhysicsFS version program was compiled against.
 *
 * This macro fills in a PHYSFS_Version structure with the version of the
 *  library you compiled against. This is determined by what header the
 *  compiler uses. Note that if you dynamically linked the library, you might
 *  have a slightly newer or older version at runtime. That version can be
 *  determined with PHYSFS_getLinkedVersion(), which, unlike PHYSFS_VERSION,
 *  is not a macro.
 *
 * \param x A pointer to a PHYSFS_Version struct to initialize.
 *
 * \sa PHYSFS_Version
 * \sa PHYSFS_getLinkedVersion
 */
#define PHYSFS_VERSION(x) \
{ \
    (x)->major = PHYSFS_VER_MAJOR; \
    (x)->minor = PHYSFS_VER_MINOR; \
    (x)->patch = PHYSFS_VER_PATCH; \
}


/**
 * \fn void PHYSFS_getLinkedVersion(PHYSFS_Version *ver)
 * \brief Get the version of PhysicsFS that is linked against your program.
 *
 * If you are using a shared library (DLL) version of PhysFS, then it is
 *  possible that it will be different than the version you compiled against.
 *
 * This is a real function; the macro PHYSFS_VERSION tells you what version
 *  of PhysFS you compiled against:
 *
 * \code
 * PHYSFS_Version compiled;
 * PHYSFS_Version linked;
 *
 * PHYSFS_VERSION(&compiled);
 * PHYSFS_getLinkedVersion(&linked);
 * printf("We compiled against PhysFS version %d.%d.%d ...\n",
 *           compiled.major, compiled.minor, compiled.patch);
 * printf("But we linked against PhysFS version %d.%d.%d.\n",
 *           linked.major, linked.minor, linked.patch);
 * \endcode
 *
 * This function may be called safely at any time, even before PHYSFS_init().
 *
 * \sa PHYSFS_VERSION
 */
__EXPORT__ void PHYSFS_getLinkedVersion(PHYSFS_Version *ver);


/**
 * \fn int PHYSFS_init(const char *argv0)
 * \brief Initialize the PhysicsFS library.
 *
 * This must be called before any other PhysicsFS function.
 *
 * This should be called prior to any attempts to change your process's
 *  current working directory.
 *
 *   \param argv0 the argv[0] string passed to your program's mainline.
 *          This may be NULL on most platforms (such as ones without a
 *          standard main() function), but you should always try to pass
 *          something in here. Unix-like systems such as Linux _need_ to
 *          pass argv[0] from main() in here.
 *  \return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_deinit
 */
__EXPORT__ int PHYSFS_init(const char *argv0);


/**
 * \fn int PHYSFS_deinit(void)
 * \brief Deinitialize the PhysicsFS library.
 *
 * This closes any files opened via PhysicsFS, blanks the search/write paths,
 *  frees memory, and invalidates all of your file handles.
 *
 * Note that this call can FAIL if there's a file open for writing that
 *  refuses to close (for example, the underlying operating system was
 *  buffering writes to network filesystem, and the fileserver has crashed,
 *  or a hard drive has failed, etc). It is usually best to close all write
 *  handles yourself before calling this function, so that you can gracefully
 *  handle a specific failure.
 *
 * Once successfully deinitialized, PHYSFS_init() can be called again to
 *  restart the subsystem. All defaults API states are restored at this
 *  point.
 *
 *  \return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError(). If failure, state of PhysFS is
 *          undefined, and probably badly screwed up.
 *
 * \sa PHYSFS_init
 */
__EXPORT__ int PHYSFS_deinit(void);


/**
 * \fn const PHYSFS_ArchiveInfo **PHYSFS_supportedArchiveTypes(void)
 * \brief Get a list of supported archive types.
 *
 * Get a list of archive types supported by this implementation of PhysicFS.
 *  These are the file formats usable for search path entries. This is for
 *  informational purposes only. Note that the extension listed is merely
 *  convention: if we list "ZIP", you can open a PkZip-compatible archive
 *  with an extension of "XYZ", if you like.
 *
 * The returned value is an array of pointers to PHYSFS_ArchiveInfo structures,
 *  with a NULL entry to signify the end of the list:
 *
 * \code
 * PHYSFS_ArchiveInfo **i;
 *
 * for (i = PHYSFS_supportedArchiveTypes(); *i != NULL; i++)
 * {
 *     printf("Supported archive: [%s], which is [%s].\n",
 *              i->extension, i->description);
 * }
 * \endcode
 *
 * The return values are pointers to static internal memory, and should
 *  be considered READ ONLY, and never freed.
 *
 *   \return READ ONLY Null-terminated array of READ ONLY structures.
 */
__EXPORT__ const PHYSFS_ArchiveInfo **PHYSFS_supportedArchiveTypes(void);


/**
 * \fn void PHYSFS_freeList(void *listVar)
 * \brief Deallocate resources of lists returned by PhysicsFS.
 *
 * Certain PhysicsFS functions return lists of information that are
 *  dynamically allocated. Use this function to free those resources.
 *
 *   \param listVar List of information specified as freeable by this function.
 *
 * \sa PHYSFS_getCdRomDirs
 * \sa PHYSFS_enumerateFiles
 * \sa PHYSFS_getSearchPath
 */
__EXPORT__ void PHYSFS_freeList(void *listVar);


/**
 * \fn const char *PHYSFS_getLastError(void)
 * \brief Get human-readable error information.
 *
 * Get the last PhysicsFS error message as a null-terminated string.
 *  This will be NULL if there's been no error since the last call to this
 *  function. The pointer returned by this call points to an internal buffer.
 *  Each thread has a unique error state associated with it, but each time
 *  a new error message is set, it will overwrite the previous one associated
 *  with that thread. It is safe to call this function at anytime, even
 *  before PHYSFS_init().
 *
 *   \return READ ONLY string of last error message.
 */
__EXPORT__ const char *PHYSFS_getLastError(void);


/**
 * \fn const char *PHYSFS_getDirSeparator(void)
 * \brief Get platform-dependent dir separator string.
 *
 * This returns "\\\\" on win32, "/" on Unix, and ":" on MacOS. It may be more
 *  than one character, depending on the platform, and your code should take
 *  that into account. Note that this is only useful for setting up the
 *  search/write paths, since access into those dirs always use '/'
 *  (platform-independent notation) to separate directories. This is also
 *  handy for getting platform-independent access when using stdio calls.
 *
 *   \return READ ONLY null-terminated string of platform's dir separator.
 */
__EXPORT__ const char *PHYSFS_getDirSeparator(void);


/**
 * \fn void PHYSFS_permitSymbolicLinks(int allow)
 * \brief Enable or disable following of symbolic links.
 *
 * Some physical filesystems and archives contain files that are just pointers
 *  to other files. On the physical filesystem, opening such a link will
 *  (transparently) open the file that is pointed to.
 *
 * By default, PhysicsFS will check if a file is really a symlink during open
 *  calls and fail if it is. Otherwise, the link could take you outside the
 *  write and search paths, and compromise security.
 *
 * If you want to take that risk, call this function with a non-zero parameter.
 *  Note that this is more for sandboxing a program's scripting language, in
 *  case untrusted scripts try to compromise the system. Generally speaking,
 *  a user could very well have a legitimate reason to set up a symlink, so
 *  unless you feel there's a specific danger in allowing them, you should
 *  permit them.
 *
 * Symlinks are only explicitly checked when dealing with filenames
 *  in platform-independent notation. That is, when setting up your
 *  search and write paths, etc, symlinks are never checked for.
 *
 * Symbolic link permission can be enabled or disabled at any time after
 *  you've called PHYSFS_init(), and is disabled by default.
 *
 *   \param allow nonzero to permit symlinks, zero to deny linking.
 */
__EXPORT__ void PHYSFS_permitSymbolicLinks(int allow);


/**
 * \fn char **PHYSFS_getCdRomDirs(void)
 * \brief Get an array of paths to available CD-ROM drives.
 *
 * The dirs returned are platform-dependent ("D:\" on Win32, "/cdrom" or
 *  whatnot on Unix). Dirs are only returned if there is a disc ready and
 *  accessible in the drive. So if you've got two drives (D: and E:), and only
 *  E: has a disc in it, then that's all you get. If the user inserts a disc
 *  in D: and you call this function again, you get both drives. If, on a
 *  Unix box, the user unmounts a disc and remounts it elsewhere, the next
 *  call to this function will reflect that change. Fun.
 *
 * The returned value is an array of strings, with a NULL entry to signify the
 *  end of the list:
 *
 * \code
 * char **cds = PHYSFS_getCdRomDirs();
 * char **i;
 *
 * for (i = cds; *i != NULL; i++)
 *     printf("cdrom dir [%s] is available.\n", *i);
 *
 * PHYSFS_freeList(cds);
 * \endcode
 *
 * This call may block while drives spin up. Be forewarned.
 *
 * When you are done with the returned information, you may dispose of the
 *  resources by calling PHYSFS_freeList() with the returned pointer.
 *
 *   \return Null-terminated array of null-terminated strings.
 */
__EXPORT__ char **PHYSFS_getCdRomDirs(void);


/**
 * \fn const char *PHYSFS_getBaseDir(void)
 * \brief Get the path where the application resides.
 *
 * Helper function.
 *
 * Get the "base dir". This is the directory where the application was run
 *  from, which is probably the installation directory, and may or may not
 *  be the process's current working directory.
 *
 * You should probably use the base dir in your search path.
 *
 *  \return READ ONLY string of base dir in platform-dependent notation.
 *
 * \sa PHYSFS_getUserDir
 */
__EXPORT__ const char *PHYSFS_getBaseDir(void);


/**
 * \fn const char *PHYSFS_getUserDir(void)
 * \brief Get the path where user's home directory resides.
 *
 * Helper function.
 *
 * Get the "user dir". This is meant to be a suggestion of where a specific
 *  user of the system can store files. On Unix, this is her home directory.
 *  On systems with no concept of multiple home directories (MacOS, win95),
 *  this will default to something like "C:\mybasedir\users\username"
 *  where "username" will either be the login name, or "default" if the
 *  platform doesn't support multiple users, either.
 *
 * You should probably use the user dir as the basis for your write dir, and
 *  also put it near the beginning of your search path.
 *
 *  \return READ ONLY string of user dir in platform-dependent notation.
 *
 * \sa PHYSFS_getBaseDir
 */
__EXPORT__ const char *PHYSFS_getUserDir(void);


/**
 * \fn const char *PHYSFS_getWriteDir(void)
 * \brief Get path where PhysicsFS will allow file writing.
 *
 * Get the current write dir. The default write dir is NULL.
 *
 *  \return READ ONLY string of write dir in platform-dependent notation,
 *           OR NULL IF NO WRITE PATH IS CURRENTLY SET.
 *
 * \sa PHYSFS_setWriteDir
 */
__EXPORT__ const char *PHYSFS_getWriteDir(void);


/**
 * \fn int PHYSFS_setWriteDir(const char *newDir)
 * \brief Tell PhysicsFS where it may write files.
 *
 * Set a new write dir. This will override the previous setting. If the
 *  directory or a parent directory doesn't exist in the physical filesystem,
 *  PhysicsFS will attempt to create them as needed.
 *
 * This call will fail (and fail to change the write dir) if the current
 *  write dir still has files open in it.
 *
 *   \param newDir The new directory to be the root of the write dir,
 *                   specified in platform-dependent notation. Setting to NULL
 *                   disables the write dir, so no files can be opened for
 *                   writing via PhysicsFS.
 *  \return non-zero on success, zero on failure. All attempts to open a file
 *           for writing via PhysicsFS will fail until this call succeeds.
 *           Specifics of the error can be gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_getWriteDir
 */
__EXPORT__ int PHYSFS_setWriteDir(const char *newDir);


/**
 * \fn int PHYSFS_addToSearchPath(const char *newDir, int appendToPath)
 * \brief Add an archive or directory to the search path.
 *
 * If this is a duplicate, the entry is not added again, even though the
 *  function succeeds.
 *
 *   \param newDir directory or archive to add to the path, in
 *                   platform-dependent notation.
 *   \param appendToPath nonzero to append to search path, zero to prepend.
 *  \return nonzero if added to path, zero on failure (bogus archive, dir
 *                   missing, etc). Specifics of the error can be
 *                   gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_removeFromSearchPath
 * \sa PHYSFS_getSearchPath
 */
__EXPORT__ int PHYSFS_addToSearchPath(const char *newDir, int appendToPath);


/**
 * \fn int PHYSFS_removeFromSearchPath(const char *oldDir)
 * \brief Remove a directory or archive from the search path.
 *
 * This must be a (case-sensitive) match to a dir or archive already in the
 *  search path, specified in platform-dependent notation.
 *
 * This call will fail (and fail to remove from the path) if the element still
 *  has files open in it.
 *
 *    \param oldDir dir/archive to remove.
 *   \return nonzero on success, zero on failure.
 *            Specifics of the error can be gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_addToSearchPath
 * \sa PHYSFS_getSearchPath
 */
__EXPORT__ int PHYSFS_removeFromSearchPath(const char *oldDir);


/**
 * \fn char **PHYSFS_getSearchPath(void)
 * \brief Get the current search path.
 *
 * The default search path is an empty list.
 *
 * The returned value is an array of strings, with a NULL entry to signify the
 *  end of the list:
 *
 * \code
 * char **i;
 *
 * for (i = PHYSFS_getSearchPath(); *i != NULL; i++)
 *     printf("[%s] is in the search path.\n", *i);
 * \endcode
 *
 * When you are done with the returned information, you may dispose of the
 *  resources by calling PHYSFS_freeList() with the returned pointer.
 *
 *   \return Null-terminated array of null-terminated strings. NULL if there
 *            was a problem (read: OUT OF MEMORY).
 *
 * \sa PHYSFS_addToSearchPath
 * \sa PHYSFS_removeFromSearchPath
 */
__EXPORT__ char **PHYSFS_getSearchPath(void);


/**
 * \fn int PHYSFS_setSaneConfig(const char *organization, const char *appName, const char *archiveExt, int includeCdRoms, int archivesFirst)
 * \brief Set up sane, default paths.
 *
 * Helper function.
 *
 * The write dir will be set to "userdir/.organization/appName", which is
 *  created if it doesn't exist.
 *
 * The above is sufficient to make sure your program's configuration directory
 *  is separated from other clutter, and platform-independent. The period
 *  before "mygame" even hides the directory on Unix systems.
 *
 *  The search path will be:
 *
 *    - The Write Dir (created if it doesn't exist)
 *    - The Base Dir (PHYSFS_getBaseDir())
 *    - All found CD-ROM dirs (optionally)
 *
 * These directories are then searched for files ending with the extension
 *  (archiveExt), which, if they are valid and supported archives, will also
 *  be added to the search path. If you specified "PKG" for (archiveExt), and
 *  there's a file named data.PKG in the base dir, it'll be checked. Archives
 *  can either be appended or prepended to the search path in alphabetical
 *  order, regardless of which directories they were found in.
 *
 * All of this can be accomplished from the application, but this just does it
 *  all for you. Feel free to add more to the search path manually, too.
 *
 *    \param organization Name of your company/group/etc to be used as a
 *                         dirname, so keep it small, and no-frills.
 *
 *    \param appName Program-specific name of your program, to separate it
 *                   from other programs using PhysicsFS.
 *
 *    \param archiveExt File extention used by your program to specify an
 *                      archive. For example, Quake 3 uses "pk3", even though
 *                      they are just zipfiles. Specify NULL to not dig out
 *                      archives automatically. Do not specify the '.' char;
 *                      If you want to look for ZIP files, specify "ZIP" and
 *                      not ".ZIP" ... the archive search is case-insensitive.
 *
 *    \param includeCdRoms Non-zero to include CD-ROMs in the search path, and
 *                         (if (archiveExt) != NULL) search them for archives.
 *                         This may cause a significant amount of blocking
 *                         while discs are accessed, and if there are no discs
 *                         in the drive (or even not mounted on Unix systems),
 *                         then they may not be made available anyhow. You may
 *                         want to specify zero and handle the disc setup
 *                         yourself.
 *
 *    \param archivesFirst Non-zero to prepend the archives to the search path.
 *                          Zero to append them. Ignored if !(archiveExt).
 *
 *  \return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
__EXPORT__ int PHYSFS_setSaneConfig(const char *organization,
                                    const char *appName,
                                    const char *archiveExt,
                                    int includeCdRoms,
                                    int archivesFirst);


/**
 * \fn int PHYSFS_mkdir(const char *dirName)
 * \brief Create a directory.
 *
 * This is specified in platform-independent notation in relation to the
 *  write dir. All missing parent directories are also created if they
 *  don't exist.
 *
 * So if you've got the write dir set to "C:\mygame\writedir" and call
 *  PHYSFS_mkdir("downloads/maps") then the directories
 *  "C:\mygame\writedir\downloads" and "C:\mygame\writedir\downloads\maps"
 *  will be created if possible. If the creation of "maps" fails after we
 *  have successfully created "downloads", then the function leaves the
 *  created directory behind and reports failure.
 *
 *   \param dirName New dir to create.
 *  \return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_delete
 */
__EXPORT__ int PHYSFS_mkdir(const char *dirName);


/**
 * \fn int PHYSFS_delete(const char *filename)
 * \brief Delete a file or directory.
 *
 * (filename) is specified in platform-independent notation in relation to the
 *  write dir.
 *
 * A directory must be empty before this call can delete it.
 *
 * Deleting a symlink will remove the link, not what it points to, regardless
 *  of whether you "permitSymLinks" or not.
 *
 * So if you've got the write dir set to "C:\mygame\writedir" and call
 *  PHYSFS_delete("downloads/maps/level1.map") then the file
 *  "C:\mygame\writedir\downloads\maps\level1.map" is removed from the
 *  physical filesystem, if it exists and the operating system permits the
 *  deletion.
 *
 * Note that on Unix systems, deleting a file may be successful, but the
 *  actual file won't be removed until all processes that have an open
 *  filehandle to it (including your program) close their handles.
 *
 * Chances are, the bits that make up the file still exist, they are just
 *  made available to be written over at a later point. Don't consider this
 *  a security method or anything.  :)
 *
 *   \param filename Filename to delete.
 *  \return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
__EXPORT__ int PHYSFS_delete(const char *filename);


/**
 * \fn const char *PHYSFS_getRealDir(const char *filename)
 * \brief Figure out where in the search path a file resides.
 *
 * The file is specified in platform-independent notation. The returned
 *  filename will be the element of the search path where the file was found,
 *  which may be a directory, or an archive. Even if there are multiple
 *  matches in different parts of the search path, only the first one found
 *  is used, just like when opening a file.
 *
 * So, if you look for "maps/level1.map", and C:\mygame is in your search
 *  path and C:\mygame\maps\level1.map exists, then "C:\mygame" is returned.
 *
 * If a any part of a match is a symbolic link, and you've not explicitly
 *  permitted symlinks, then it will be ignored, and the search for a match
 *  will continue.
 *
 *     \param filename file to look for.
 *    \return READ ONLY string of element of search path containing the
 *             the file in question. NULL if not found.
 */
__EXPORT__ const char *PHYSFS_getRealDir(const char *filename);



/**
 * \fn char **PHYSFS_enumerateFiles(const char *dir)
 * \brief Get a file listing of a search path's directory.
 *
 * Matching directories are interpolated. That is, if "C:\mydir" is in the
 *  search path and contains a directory "savegames" that contains "x.sav",
 *  "y.sav", and "z.sav", and there is also a "C:\userdir" in the search path
 *  that has a "savegames" subdirectory with "w.sav", then the following code:
 *
 * \code
 * char **rc = PHYSFS_enumerateFiles("savegames");
 * char **i;
 *
 * for (i = rc; *i != NULL; i++)
 *     printf(" * We've got [%s].\n", *i);
 *
 * PHYSFS_freeList(rc);
 * \endcode
 *
 *  ...will print:
 *
 * \verbatim
 * We've got [x.sav].
 * We've got [y.sav].
 * We've got [z.sav].
 * We've got [w.sav].\endverbatim
 *
 * Feel free to sort the list however you like. We only promise there will
 *  be no duplicates, but not what order the final list will come back in.
 *
 * Don't forget to call PHYSFS_freeList() with the return value from this
 *  function when you are done with it.
 *
 *    \param dir directory in platform-independent notation to enumerate.
 *   \return Null-terminated array of null-terminated strings.
 */
__EXPORT__ char **PHYSFS_enumerateFiles(const char *dir);


/**
 * \fn int PHYSFS_exists(const char *fname)
 * \brief Determine if a file exists in the search path.
 *
 * Reports true if there is an entry anywhere in the search path by the
 *  name of (fname).
 *
 * Note that entries that are symlinks are ignored if
 *  PHYSFS_permitSymbolicLinks(1) hasn't been called, so you
 *  might end up further down in the search path than expected.
 *
 *    \param fname filename in platform-independent notation.
 *   \return non-zero if filename exists. zero otherwise.
 *
 * \sa PHYSFS_isDirectory
 * \sa PHYSFS_isSymbolicLink
 */
__EXPORT__ int PHYSFS_exists(const char *fname);


/**
 * \fn int PHYSFS_isDirectory(const char *fname)
 * \brief Determine if a file in the search path is really a directory.
 *
 * Determine if the first occurence of (fname) in the search path is
 *  really a directory entry.
 *
 * Note that entries that are symlinks are ignored if
 *  PHYSFS_permitSymbolicLinks(1) hasn't been called, so you
 *  might end up further down in the search path than expected.
 *
 *    \param fname filename in platform-independent notation.
 *   \return non-zero if filename exists and is a directory.  zero otherwise.
 *
 * \sa PHYSFS_exists
 * \sa PHYSFS_isSymbolicLink
 */
__EXPORT__ int PHYSFS_isDirectory(const char *fname);


/**
 * \fn int PHYSFS_isSymbolicLink(const char *fname)
 * \brief Determine if a file in the search path is really a symbolic link.
 *
 * Determine if the first occurence of (fname) in the search path is
 *  really a symbolic link.
 *
 * Note that entries that are symlinks are ignored if
 *  PHYSFS_permitSymbolicLinks(1) hasn't been called, and as such,
 *  this function will always return 0 in that case.
 *
 *    \param fname filename in platform-independent notation.
 *   \return non-zero if filename exists and is a symlink.  zero otherwise.
 *
 * \sa PHYSFS_exists
 * \sa PHYSFS_isDirectory
 */
__EXPORT__ int PHYSFS_isSymbolicLink(const char *fname);


/**
 * \fn PHYSFS_file *PHYSFS_openWrite(const char *filename)
 * \brief Open a file for writing.
 *
 * Open a file for writing, in platform-independent notation and in relation
 *  to the write dir as the root of the writable filesystem. The specified
 *  file is created if it doesn't exist. If it does exist, it is truncated to
 *  zero bytes, and the writing offset is set to the start.
 *
 * Note that entries that are symlinks are ignored if
 *  PHYSFS_permitSymbolicLinks(1) hasn't been called, and opening a
 *  symlink with this function will fail in such a case.
 *
 *   \param filename File to open.
 *  \return A valid PhysicsFS filehandle on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_openRead
 * \sa PHYSFS_openAppend
 * \sa PHYSFS_write
 * \sa PHYSFS_close
 */
__EXPORT__ PHYSFS_file *PHYSFS_openWrite(const char *filename);


/**
 * \fn PHYSFS_file *PHYSFS_openAppend(const char *filename)
 * \brief Open a file for appending.
 *
 * Open a file for writing, in platform-independent notation and in relation
 *  to the write dir as the root of the writable filesystem. The specified
 *  file is created if it doesn't exist. If it does exist, the writing offset
 *  is set to the end of the file, so the first write will be the byte after
 *  the end.
 *
 * Note that entries that are symlinks are ignored if
 *  PHYSFS_permitSymbolicLinks(1) hasn't been called, and opening a
 *  symlink with this function will fail in such a case.
 *
 *   \param filename File to open.
 *  \return A valid PhysicsFS filehandle on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_openRead
 * \sa PHYSFS_openWrite
 * \sa PHYSFS_write
 * \sa PHYSFS_close
 */
__EXPORT__ PHYSFS_file *PHYSFS_openAppend(const char *filename);


/**
 * \fn PHYSFS_file *PHYSFS_openRead(const char *filename)
 * \brief Open a file for reading.
 *
 * Open a file for reading, in platform-independent notation. The search path
 *  is checked one at a time until a matching file is found, in which case an
 *  abstract filehandle is associated with it, and reading may be done.
 *  The reading offset is set to the first byte of the file.
 *
 * Note that entries that are symlinks are ignored if
 *  PHYSFS_permitSymbolicLinks(1) hasn't been called, and opening a
 *  symlink with this function will fail in such a case.
 *
 *   \param filename File to open.
 *  \return A valid PhysicsFS filehandle on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_openWrite
 * \sa PHYSFS_openAppend
 * \sa PHYSFS_read
 * \sa PHYSFS_close
 */
__EXPORT__ PHYSFS_file *PHYSFS_openRead(const char *filename);


/**
 * \fn int PHYSFS_close(PHYSFS_file *handle)
 * \brief Close a PhysicsFS filehandle.
 *
 * This call is capable of failing if the operating system was buffering
 *  writes to the physical media, and, now forced to write those changes to
 *  physical media, can not store the data for some reason. In such a case,
 *  the filehandle stays open. A well-written program should ALWAYS check the
 *  return value from the close call in addition to every writing call!
 *
 *   \param handle handle returned from PHYSFS_open*().
 *  \return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_openRead
 * \sa PHYSFS_openWrite
 * \sa PHYSFS_openAppend
 */
__EXPORT__ int PHYSFS_close(PHYSFS_file *handle);


/**
 * \fn PHYSFS_sint64 PHYSFS_getLastModTime(const char *filename)
 * \brief Get the last modification time of a file.
 *
 * The modtime is returned as a number of seconds since the epoch
 *  (Jan 1, 1970). The exact derivation and accuracy of this time depends on
 *  the particular archiver. If there is no reasonable way to obtain this
 *  information for a particular archiver, or there was some sort of error,
 *  this function returns (-1).
 *
 *   \param filename filename to check, in platform-independent notation.
 *  \return last modified time of the file. -1 if it can't be determined.
 */
__EXPORT__ PHYSFS_sint64 PHYSFS_getLastModTime(const char *filename);


/**
 * \fn PHYSFS_sint64 PHYSFS_read(PHYSFS_file *handle, void *buffer, PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
 * \brief Read data from a PhysicsFS filehandle
 *
 * The file must be opened for reading.
 *
 *   \param handle handle returned from PHYSFS_openRead().
 *   \param buffer buffer to store read data into.
 *   \param objSize size in bytes of objects being read from (handle).
 *   \param objCount number of (objSize) objects to read from (handle).
 *  \return number of objects read. PHYSFS_getLastError() can shed light on
 *           the reason this might be < (objCount), as can PHYSFS_eof().
 *            -1 if complete failure.
 *
 * \sa PHYSFS_eof
 */
__EXPORT__ PHYSFS_sint64 PHYSFS_read(PHYSFS_file *handle,
                                     void *buffer,
                                     PHYSFS_uint32 objSize,
                                     PHYSFS_uint32 objCount);

/**
 * \fn PHYSFS_sint64 PHYSFS_write(PHYSFS_file *handle, const void *buffer, PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
 * \brief Write data to a PhysicsFS filehandle
 *
 * The file must be opened for writing.
 *
 *   \param handle retval from PHYSFS_openWrite() or PHYSFS_openAppend().
 *   \param buffer buffer to store read data into.
 *   \param objSize size in bytes of objects being read from (handle).
 *   \param objCount number of (objSize) objects to read from (handle).
 *  \return number of objects written. PHYSFS_getLastError() can shed light on
 *           the reason this might be < (objCount). -1 if complete failure.
 */
__EXPORT__ PHYSFS_sint64 PHYSFS_write(PHYSFS_file *handle,
                                      const void *buffer,
                                      PHYSFS_uint32 objSize,
                                      PHYSFS_uint32 objCount);

/**
 * \fn int PHYSFS_eof(PHYSFS_file *handle)
 * \brief Check for end-of-file state on a PhysicsFS filehandle.
 *
 * Determine if the end of file has been reached in a PhysicsFS filehandle.
 *
 *   \param handle handle returned from PHYSFS_openRead().
 *  \return nonzero if EOF, zero if not.
 *
 * \sa PHYSFS_read
 * \sa PHYSFS_tell
 */
__EXPORT__ int PHYSFS_eof(PHYSFS_file *handle);


/**
 * \fn PHYSFS_sint64 PHYSFS_tell(PHYSFS_file *handle)
 * \brief Determine current position within a PhysicsFS filehandle.
 *
 *   \param handle handle returned from PHYSFS_open*().
 *  \return offset in bytes from start of file. -1 if error occurred.
 *           Specifics of the error can be gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_seek
 */
__EXPORT__ PHYSFS_sint64 PHYSFS_tell(PHYSFS_file *handle);


/**
 * \fn int PHYSFS_seek(PHYSFS_file *handle, PHYSFS_uint64 pos)
 * \brief Seek to a new position within a PhysicsFS filehandle.
 *
 * The next read or write will occur at that place. Seeking past the
 *  beginning or end of the file is not allowed, and causes an error.
 *
 *   \param handle handle returned from PHYSFS_open*().
 *   \param pos number of bytes from start of file to seek to.
 *  \return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 *
 * \sa PHYSFS_tell
 */
__EXPORT__ int PHYSFS_seek(PHYSFS_file *handle, PHYSFS_uint64 pos);


/**
 * \fn PHYSFS_sint64 PHYSFS_fileLength(PHYSFS_file *handle)
 * \brief Get total length of a file in bytes.
 *
 * Note that if the file size can't be determined (since the archive is
 *  "streamed" or whatnot) than this will report (-1). Also note that if
 *  another process/thread is writing to this file at the same time, then
 *  the information this function supplies could be incorrect before you
 *  get it. Use with caution, or better yet, don't use at all.
 *
 *   \param handle handle returned from PHYSFS_open*().
 *  \return size in bytes of the file. -1 if can't be determined.
 *
 * \sa PHYSFS_tell
 * \sa PHYSFS_seek
 */
__EXPORT__ PHYSFS_sint64 PHYSFS_fileLength(PHYSFS_file *handle);


/* Byteorder stuff... */

/**
 * \fn PHYSFS_sint16 PHYSFS_swapSLE16(PHYSFS_sint16 val)
 * \brief Swap littleendian signed 16 to platform's native byte order.
 *
 * Take a 16-bit signed value in littleendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_sint16 PHYSFS_swapSLE16(PHYSFS_sint16 val);


/**
 * \fn PHYSFS_uint16 PHYSFS_swapULE16(PHYSFS_uint16 val)
 * \brief Swap littleendian unsigned 16 to platform's native byte order.
 *
 * Take a 16-bit unsigned value in littleendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_uint16 PHYSFS_swapULE16(PHYSFS_uint16 val);

/**
 * \fn PHYSFS_sint32 PHYSFS_swapSLE32(PHYSFS_sint32 val)
 * \brief Swap littleendian signed 32 to platform's native byte order.
 *
 * Take a 32-bit signed value in littleendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_sint32 PHYSFS_swapSLE32(PHYSFS_sint32 val);


/**
 * \fn PHYSFS_uint32 PHYSFS_swapULE32(PHYSFS_uint32 val)
 * \brief Swap littleendian unsigned 32 to platform's native byte order.
 *
 * Take a 32-bit unsigned value in littleendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_uint32 PHYSFS_swapULE32(PHYSFS_uint32 val);

/**
 * \fn PHYSFS_sint64 PHYSFS_swapSLE64(PHYSFS_sint64 val)
 * \brief Swap littleendian signed 64 to platform's native byte order.
 *
 * Take a 64-bit signed value in littleendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ PHYSFS_sint64 PHYSFS_swapSLE64(PHYSFS_sint64 val);


/**
 * \fn PHYSFS_uint64 PHYSFS_swapULE64(PHYSFS_uint64 val)
 * \brief Swap littleendian unsigned 64 to platform's native byte order.
 *
 * Take a 64-bit unsigned value in littleendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ PHYSFS_uint64 PHYSFS_swapULE64(PHYSFS_uint64 val);


/**
 * \fn PHYSFS_sint16 PHYSFS_swapSBE16(PHYSFS_sint16 val)
 * \brief Swap bigendian signed 16 to platform's native byte order.
 *
 * Take a 16-bit signed value in bigendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_sint16 PHYSFS_swapSBE16(PHYSFS_sint16 val);


/**
 * \fn PHYSFS_uint16 PHYSFS_swapUBE16(PHYSFS_uint16 val)
 * \brief Swap bigendian unsigned 16 to platform's native byte order.
 *
 * Take a 16-bit unsigned value in bigendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_uint16 PHYSFS_swapUBE16(PHYSFS_uint16 val);

/**
 * \fn PHYSFS_sint32 PHYSFS_swapSBE32(PHYSFS_sint32 val)
 * \brief Swap bigendian signed 32 to platform's native byte order.
 *
 * Take a 32-bit signed value in bigendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_sint32 PHYSFS_swapSBE32(PHYSFS_sint32 val);


/**
 * \fn PHYSFS_uint32 PHYSFS_swapUBE32(PHYSFS_uint32 val)
 * \brief Swap bigendian unsigned 32 to platform's native byte order.
 *
 * Take a 32-bit unsigned value in bigendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 */
__EXPORT__ PHYSFS_uint32 PHYSFS_swapUBE32(PHYSFS_uint32 val);


/**
 * \fn PHYSFS_sint64 PHYSFS_swapSBE64(PHYSFS_sint64 val)
 * \brief Swap bigendian signed 64 to platform's native byte order.
 *
 * Take a 64-bit signed value in bigendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ PHYSFS_sint64 PHYSFS_swapSBE64(PHYSFS_sint64 val);


/**
 * \fn PHYSFS_uint64 PHYSFS_swapUBE64(PHYSFS_uint64 val)
 * \brief Swap bigendian unsigned 64 to platform's native byte order.
 *
 * Take a 64-bit unsigned value in bigendian format and convert it to
 *  the platform's native byte order.
 *
 *    \param val value to convert
 *   \return converted value.
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ PHYSFS_uint64 PHYSFS_swapUBE64(PHYSFS_uint64 val);


/**
 * \fn int PHYSFS_readSLE16(PHYSFS_file *file, PHYSFS_sint16 *val)
 * \brief Read and convert a signed 16-bit littleendian value.
 *
 * Convenience function. Read a signed 16-bit littleendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_readSLE16(PHYSFS_file *file, PHYSFS_sint16 *val);


/**
 * \fn int PHYSFS_readULE16(PHYSFS_file *file, PHYSFS_uint16 *val)
 * \brief Read and convert an unsigned 16-bit littleendian value.
 *
 * Convenience function. Read an unsigned 16-bit littleendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 */
__EXPORT__ int PHYSFS_readULE16(PHYSFS_file *file, PHYSFS_uint16 *val);


/**
 * \fn int PHYSFS_readSBE16(PHYSFS_file *file, PHYSFS_sint16 *val)
 * \brief Read and convert a signed 16-bit bigendian value.
 *
 * Convenience function. Read a signed 16-bit bigendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_readSBE16(PHYSFS_file *file, PHYSFS_sint16 *val);


/**
 * \fn int PHYSFS_readUBE16(PHYSFS_file *file, PHYSFS_uint16 *val)
 * \brief Read and convert an unsigned 16-bit bigendian value.
 *
 * Convenience function. Read an unsigned 16-bit bigendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 */
__EXPORT__ int PHYSFS_readUBE16(PHYSFS_file *file, PHYSFS_uint16 *val);


/**
 * \fn int PHYSFS_readSLE32(PHYSFS_file *file, PHYSFS_sint32 *val)
 * \brief Read and convert a signed 32-bit littleendian value.
 *
 * Convenience function. Read a signed 32-bit littleendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_readSLE32(PHYSFS_file *file, PHYSFS_sint32 *val);


/**
 * \fn int PHYSFS_readULE32(PHYSFS_file *file, PHYSFS_uint32 *val)
 * \brief Read and convert an unsigned 32-bit littleendian value.
 *
 * Convenience function. Read an unsigned 32-bit littleendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 */
__EXPORT__ int PHYSFS_readULE32(PHYSFS_file *file, PHYSFS_uint32 *val);


/**
 * \fn int PHYSFS_readSBE32(PHYSFS_file *file, PHYSFS_sint32 *val)
 * \brief Read and convert a signed 32-bit bigendian value.
 *
 * Convenience function. Read a signed 32-bit bigendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_readSBE32(PHYSFS_file *file, PHYSFS_sint32 *val);


/**
 * \fn int PHYSFS_readUBE32(PHYSFS_file *file, PHYSFS_uint32 *val)
 * \brief Read and convert an unsigned 32-bit bigendian value.
 *
 * Convenience function. Read an unsigned 32-bit bigendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 */
__EXPORT__ int PHYSFS_readUBE32(PHYSFS_file *file, PHYSFS_uint32 *val);


/**
 * \fn int PHYSFS_readSLE64(PHYSFS_file *file, PHYSFS_sint64 *val)
 * \brief Read and convert a signed 64-bit littleendian value.
 *
 * Convenience function. Read a signed 64-bit littleendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_sint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_readSLE64(PHYSFS_file *file, PHYSFS_sint64 *val);


/**
 * \fn int PHYSFS_readULE64(PHYSFS_file *file, PHYSFS_uint64 *val)
 * \brief Read and convert an unsigned 64-bit littleendian value.
 *
 * Convenience function. Read an unsigned 64-bit littleendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_readULE64(PHYSFS_file *file, PHYSFS_uint64 *val);


/**
 * \fn int PHYSFS_readSBE64(PHYSFS_file *file, PHYSFS_sint64 *val)
 * \brief Read and convert a signed 64-bit bigendian value.
 *
 * Convenience function. Read a signed 64-bit bigendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_sint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_readSBE64(PHYSFS_file *file, PHYSFS_sint64 *val);


/**
 * \fn int PHYSFS_readUBE64(PHYSFS_file *file, PHYSFS_uint64 *val)
 * \brief Read and convert an unsigned 64-bit bigendian value.
 *
 * Convenience function. Read an unsigned 64-bit bigendian value from a
 *  file and convert it to the platform's native byte order.
 *
 *    \param file PhysicsFS file handle from which to read.
 *    \param val pointer to where value should be stored.
 *   \return zero on failure, non-zero on success. If successful, (*val) will
 *           store the result. On failure, you can find out what went wrong
 *           from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_readUBE64(PHYSFS_file *file, PHYSFS_uint64 *val);


/**
 * \fn int PHYSFS_writeSLE16(PHYSFS_file *file, PHYSFS_sint16 val)
 * \brief Convert and write a signed 16-bit littleendian value.
 *
 * Convenience function. Convert a signed 16-bit value from the platform's
 *  native byte order to littleendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeSLE16(PHYSFS_file *file, PHYSFS_sint16 val);


/**
 * \fn int PHYSFS_writeULE16(PHYSFS_file *file, PHYSFS_uint16 val)
 * \brief Convert and write an unsigned 16-bit littleendian value.
 *
 * Convenience function. Convert an unsigned 16-bit value from the platform's
 *  native byte order to littleendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeULE16(PHYSFS_file *file, PHYSFS_uint16 val);


/**
 * \fn int PHYSFS_writeSBE16(PHYSFS_file *file, PHYSFS_sint16 val)
 * \brief Convert and write a signed 16-bit bigendian value.
 *
 * Convenience function. Convert a signed 16-bit value from the platform's
 *  native byte order to bigendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeSBE16(PHYSFS_file *file, PHYSFS_sint16 val);


/**
 * \fn int PHYSFS_writeUBE16(PHYSFS_file *file, PHYSFS_uint16 val)
 * \brief Convert and write an unsigned 16-bit bigendian value.
 *
 * Convenience function. Convert an unsigned 16-bit value from the platform's
 *  native byte order to bigendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeUBE16(PHYSFS_file *file, PHYSFS_uint16 val);


/**
 * \fn int PHYSFS_writeSLE32(PHYSFS_file *file, PHYSFS_sint32 val)
 * \brief Convert and write a signed 32-bit littleendian value.
 *
 * Convenience function. Convert a signed 32-bit value from the platform's
 *  native byte order to littleendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeSLE32(PHYSFS_file *file, PHYSFS_sint32 val);


/**
 * \fn int PHYSFS_writeULE32(PHYSFS_file *file, PHYSFS_uint32 val)
 * \brief Convert and write an unsigned 32-bit littleendian value.
 *
 * Convenience function. Convert an unsigned 32-bit value from the platform's
 *  native byte order to littleendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeULE32(PHYSFS_file *file, PHYSFS_uint32 val);


/**
 * \fn int PHYSFS_writeSBE32(PHYSFS_file *file, PHYSFS_sint32 val)
 * \brief Convert and write a signed 32-bit bigendian value.
 *
 * Convenience function. Convert a signed 32-bit value from the platform's
 *  native byte order to bigendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeSBE32(PHYSFS_file *file, PHYSFS_sint32 val);


/**
 * \fn int PHYSFS_writeUBE32(PHYSFS_file *file, PHYSFS_uint32 val)
 * \brief Convert and write an unsigned 32-bit bigendian value.
 *
 * Convenience function. Convert an unsigned 32-bit value from the platform's
 *  native byte order to bigendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 */
__EXPORT__ int PHYSFS_writeUBE32(PHYSFS_file *file, PHYSFS_uint32 val);


/**
 * \fn int PHYSFS_writeSLE64(PHYSFS_file *file, PHYSFS_sint64 val)
 * \brief Convert and write a signed 64-bit littleendian value.
 *
 * Convenience function. Convert a signed 64-bit value from the platform's
 *  native byte order to littleendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_writeSLE64(PHYSFS_file *file, PHYSFS_sint64 val);


/**
 * \fn int PHYSFS_writeULE64(PHYSFS_file *file, PHYSFS_uint64 val)
 * \brief Convert and write an unsigned 64-bit littleendian value.
 *
 * Convenience function. Convert an unsigned 64-bit value from the platform's
 *  native byte order to littleendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_writeULE64(PHYSFS_file *file, PHYSFS_uint64 val);


/**
 * \fn int PHYSFS_writeSBE64(PHYSFS_file *file, PHYSFS_sint64 val)
 * \brief Convert and write a signed 64-bit bigending value.
 *
 * Convenience function. Convert a signed 64-bit value from the platform's
 *  native byte order to bigendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_writeSBE64(PHYSFS_file *file, PHYSFS_sint64 val);


/**
 * \fn int PHYSFS_writeUBE64(PHYSFS_file *file, PHYSFS_uint64 val)
 * \brief Convert and write an unsigned 64-bit bigendian value.
 *
 * Convenience function. Convert an unsigned 64-bit value from the platform's
 *  native byte order to bigendian and write it to a file.
 *
 *    \param file PhysicsFS file handle to which to write.
 *    \param val Value to convert and write.
 *   \return zero on failure, non-zero on success. On failure, you can
 *           find out what went wrong from PHYSFS_GetLastError().
 *
 * \warning Remember, PHYSFS_uint64 is only 32 bits on platforms without
 *          any sort of 64-bit support.
 */
__EXPORT__ int PHYSFS_writeUBE64(PHYSFS_file *file, PHYSFS_uint64 val);


#ifdef __cplusplus
}
#endif

#endif  /* !defined _INCLUDE_PHYSFS_H_ */

/* end of physfs.h ... */

