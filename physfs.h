/**
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
 * With PhysicsFS, you have a single writing path and multiple "search paths"
 *  for reading. You can think of this as a filesystem within a
 *  filesystem. If (on Windows) you were to set the writing directory to
 *  "C:\MyGame\MyWritingDirectory", then no PHYSFS calls could touch anything
 *  above this directory, including the "C:\MyGame" and "C:\" directories.
 *  This prevents an application's internal scripting language from piddling
 *  over c:\config.sys, for example. If you'd rather give PHYSFS full access
 *  to the system's REAL file system, set the writing path to "C:\", but
 *  that's generally A Bad Thing for several reasons.
 *
 * Drive letters are hidden in PhysicsFS once you set up your initial paths.
 *  The search paths create a single, hierarchical directory structure.
 *  Not only does this lend itself well to general abstraction with archives,
 *  it also gives better support to operating systems like MacOS and Unix.
 *  Generally speaking, you shouldn't ever hardcode a drive letter; not only
 *  does this hurt portability to non-Microsoft OSes, but it limits your win32
 *  users to a single drive, too. Use the PhysicsFS abstraction functions and
 *  allow user-defined configuration options, too. When opening a file, you
 *  specify it like it was on a Unix filesystem: if you want to write to
 *  "C:\MyGame\MyConfigFiles\game.cfg", then you might set the write path to
 *  "C:\MyGame" and then open "MyConfigFiles/game.cfg". This gives an
 *  abstraction across all platforms. Specifying a file in this way is termed
 *  "platform-independent notation" in this documentation. Specifying a path
 *  as "C:\mydir\myfile" or "MacOS hard drive:My Directory:My File" is termed
 *  "platform-dependent notation". The only time you use platform-dependent
 *  notation is when setting up your write and search paths; after that, all
 *  file access into those paths are done with platform-independent notation.
 *
 * All files opened for writing are opened in relation to the write path,
 *  which is the root of the writable filesystem. When opening a file for
 *  reading, PhysicsFS goes through it's internal search path. This is NOT the
 *  same thing as the PATH environment variable. An application using
 *  PhysicsFS specifies directories to be searched which may be actual
 *  directories, or archive files that contain files and subdirectories of
 *  their own. See the end of these docs for currently supported archive
 *  formats.
 *
 * Once a search path is defined, you may open files for reading. If you've
 *  got the following search path defined (to use a win32 example again):
 *
 *    C:\mygame
 *    C:\mygame\myuserfiles
 *    D:\mygamescdromdatafiles
 *    C:\mygame\installeddatafiles.zip
 *
 * Then a call to PHYSFS_openread("textfiles/myfile.txt") (note the directory
 *  separator, lack of drive letter, and lack of dir separator at the start of
 *  the string; this is platform-independent notation) will check for
 *  C:\mygame\textfiles\myfile.txt, then
 *  C:\mygame\myuserfiles\textfiles\myfile.txt, then
 *  D:\mygamescdromdatafiles\textfiles\myfile.txt, then, finally, for
 *  textfiles\myfile.txt inside of C:\mygame\installeddatafiles.zip. Remember
 *  that most archive types and platform filesystems store their filenames in
 *  a case-sensitive manner, so you should be careful to specify it correctly.
 *
 * Files opened through PhysicsFS may NOT contain "." or ".." as path
 *  elements. Not only are these meaningless on MacOS, they are a security
 *  hole. Also, symbolic links (which can be found in some archive types and
 *  directly in the filesystem on Unix platforms) are NOT followed until you
 *  call PHYSFS_permitSymbolicLinks(). That's left to your own discretion, as
 *  following a symlink can allow for access outside the write and search
 *  paths. There is no mechanism for creating new symlinks in PhysicsFS.
 *
 * The write path is not included in the search path unless you specifically
 *  add it. While you CAN change the write path as many times as you like,
 *  you should probably set it once and stick to that path. Remember that
 *  your program will not have permission to write in every directory on
 *  Unix and NT systems.
 *
 * All files are opened in binary mode; there is no endline conversion for
 *  textfiles. Other than that, PhysicsFS has some convenience functions for
 *  platform-independence. There is a function to tell you the current
 *  platform's path separator ("\\" on windows, "/" on Unix, ":" on MacOS),
 *  which is needed only to set up your search/write paths. There is a
 *  function to tell you what CD-ROM drives contain accessible discs, and a
 *  function to recommend a good search path, etc.
 *
 * A recommended order for a search path is the write path, then the base path,
 *  then the cdrom path, then any archives discovered. Quake 3 does something
 *  like this, but moves the archives to the start of the search path. Build
 *  Engine games, like Duke Nukem 3D and Blood, place the archives last, and
 *  use the base path for both searching and writing. There is a helper
 *  function (PHYSFS_setSanePaths()) that puts together a basic configuration
 *  for you, based on a few parameters. Also see the comments on
 *  PHYSFS_getBasePath(), and PHYSFS_getUserPath() for info on what those
 *  are and how they can help you determine an optimal searchpath.
 *
 * While you CAN use stdio/syscall file access in a program that has PHYSFS_*
 *  calls, doing so is not recommended, and you can not use system
 *  filehandles with PhysicsFS filehandles and vice versa.
 *
 * Note that archives need not be named as such: if you have a ZIP file and
 *  rename it with a .PKG extension, the file will still be recognized as a
 *  ZIP archive by PhysicsFS; the file's contents are used to determine its
 *  type.
 *
 * Currently supported archive types:
 *   - .ZIP (pkZip/WinZip/Info-ZIP compatible)
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#ifndef _INCLUDE_PHYSFS_H_
#define _INCLUDE_PHYSFS_H_

#ifdef __cplusplus
extern "C" {
#endif


typedef struct __PHYSFS_FILE__
{
    unsigned int opaque;
} PHYSFS_file;

typedef struct __PHYSFS_ARCHIVEINFO__
{
    const char *extension;
    const char *description;
} PHYSFS_ArchiveInfo;


/* functions... */

/**
 * Initialize PhysicsFS. This must be called before any other PhysicsFS
 *  function.
 *
 *   @param argv0 the argv[0] string passed to your program's mainline.
 *  @return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
int PHYSFS_init(const char *argv0);


/**
 * Shutdown PhysicsFS. This closes any files opened via PhysicsFS, blanks the
 *  search/write paths, frees memory, and invalidates all of your handles.
 *
 * Once deinitialized, PHYSFS_init() can be called again to restart the
 *  subsystem.
 *
 *  @return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError(). If failure, state of PhysFS is
 *          undefined, and probably badly screwed up.
 */
void PHYSFS_deinit(void);


/**
 * Get a list of archive types supported by this implementation of PhysicFS.
 *  These are the file formats usable for search path entries. This is for
 *  informational purposes only. Note that the extension listed is merely
 *  convention: if we list "ZIP", you can open a PkZip-compatible archive
 *  with an extension of "XYZ", if you like.
 *
 * The returned value is an array of strings, with a NULL entry to signify the
 *  end of the list:
 *
 * PHYSFS_ArchiveInfo **i;
 *
 * for (i = PHYSFS_supportedArchiveTypes(); *i != NULL; i++)
 * {
 *     printf("Supported archive: [%s], which is [%s].\n",
 *              i->extension, i->description);
 * }
 *
 * The return values are pointers to static internal memory, and should
 *  be considered READ ONLY, and never freed.
 *
 *   @return READ ONLY Null-terminated array of READ ONLY structures.
 */
const PHYSFS_ArchiveInfo *PHYSFS_supportedArchiveTypes(void);


/**
 * Certain PhysicsFS functions return lists of information that are
 *  dynamically allocated. Use this function to free those resources.
 *
 *   @param list List of information specified as freeable by this function.
 */
void PHYSFS_freeList(void *list);


/**
 * Get the last PhysicsFS error message as a null-terminated string.
 *  This will be NULL if there's been no error since the last call to this
 *  function. The pointer returned by this call points to an
 *  internal buffer. Each thread has a unique error state associated with it.
 *
 *   @return READ ONLY string of last error message.
 */
const char *PHYSFS_getLastError(void);


/**
 * Get a platform-dependent path separator. This is "\\" on win32, "/" on Unix,
 *  and ":" on MacOS. It may be more than one character, depending on the
 *  platform, and your code should take that into account. Note that this is
 *  only useful for setting up the search/write paths, since access into those
 *  paths always use '/' (platform-independent notation) to separate
 *  directories. This is also handy for getting platform-independent access
 *  when using stdio calls.
 *
 *   @return READ ONLY null-terminated string of platform's path separator.
 */
const char *PHYSFS_getPathSeparator(void);


/**
 * Get an array of paths to available CD-ROM drives.
 *
 * The paths returned are platform-dependent ("D:\" on Win32, "/cdrom" or
 *  whatnot on Unix). Paths are only returned if there is a disc ready and
 *  accessible in the drive. So if you've got two drives (D: and E:), and only
 *  E: has a disc in it, then that's all you get. If the user inserts a disc
 *  in D: and you call this function again, you get both drives. If, on a
 *  Unix box, the user unmounts a disc and remounts it elsewhere, the next
 *  call to this function will reflect that change. Fun.
 *
 * The returned value is an array of strings, with a NULL entry to signify the
 *  end of the list:
 *
 * char **i;
 *
 * for (i = PHYSFS_getCdRomPaths(); *i != NULL; i++)
 *     printf("cdrom path [%s] is available.\n", *i);
 *
 * This call may block while drives spin up. Be forewarned.
 *
 * When you are done with the returned information, you may dispose of the
 *  resources by calling PHYSFS_freeList() with the returned pointer.
 *
 *   @return Null-terminated array of null-terminated strings.
 */
char **PHYSFS_getCdRomPaths(void);


/**
 * Helper function.
 *
 * Get the "base path". This is the directory where the application was run
 *  from, which is probably the installation directory, and may or may not
 *  be the process's current working directory.
 *
 * You should probably use the base path in your search path.
 *
 *  @return READ ONLY string of base path in platform-dependent notation.
 */
const char *PHYSFS_getBasePath(void);


/**
 * Helper function.
 *
 * Get the "user path". This is meant to be a suggestion of where a specific
 *  user of the system can store files. On Unix, this is her home directory.
 *  On systems with no concept of multiple home directories (MacOS, win95),
 *  this will default to something like "C:\mybasepath\users\username"
 *  where "username" will either be the login name, or "default" if the
 *  platform doesn't support multiple users, either.
 *
 * You should probably use the user path as the basis for your write path, and
 *  also put it near the beginning of your search path.
 *
 *  @return READ ONLY string of user path in platform-dependent notation.
 */
const char *PHYSFS_getUserPath(void);


/**
 * Get the current write path. The default write path is NULL.
 *
 *  @return READ ONLY string of write path in platform-dependent notation,
 *           OR NULL IF NO WRITE PATH IS CURRENTLY SET.
 */
const char *PHYSFS_getWritePath(char *buffer, int bufferSize);


/**
 * Set a new write path. This will override the previous setting. If the
 *  directory or a parent directory doesn't exist in the physical filesystem,
 *  PhysicsFS will attempt to create them as needed.
 *
 * This call will fail (and fail to change the write path) if the current path
 *  still has files open in it.
 *
 *   @param newPath The new directory to be the root of the write path,
 *                   specified in platform-dependent notation. Setting to NULL
 *                   disables the write path, so no files can be opened for
 *                   writing via PhysicsFS.
 *  @return non-zero on success, zero on failure. All attempts to open a file
 *           for writing via PhysicsFS will fail until this call succeeds.
 *           Specifics of the error can be gleaned from PHYSFS_getLastError().
 *
 */
int PHYSFS_setWritePath(const char *newPath);


/**
 * Add a directory or archive to the search path. If this is a duplicate, the
 *  entry is not added again, even though the function succeeds.
 *
 *   @param newPath directory or archive to add to the path, in
 *                   platform-dependent notation.
 *   @param appendToPath nonzero to append to search path, zero to prepend.
 *  @return nonzero if added to path, zero on failure (bogus archive, path
 *                   missing, etc). Specifics of the error can be
 *                   gleaned from PHYSFS_getLastError().
 */
int PHYSFS_addToSearchPath(const char *newPath, int appendToPath);


/**
 * Remove a directory or archive from the search path.
 *
 * This must be a (case-sensitive) match to a dir or archive already in the
 *  search path, specified in platform-dependent notation.
 *
 * This call will fail (and fail to remove from the path) if the element still
 *  has files open in it.
 *
 *    @param oldPath dir/archive to remove.
 *   @return nonzero on success, zero on failure.
 *            Specifics of the error can be gleaned from PHYSFS_getLastError().
 */
int PHYSFS_removeFromSearchPath(const char *oldPath);


/**
 * Get the current search path. The default search path is an empty list.
 *
 * The returned value is an array of strings, with a NULL entry to signify the
 *  end of the list:
 *
 * char **i;
 *
 * for (i = PHYSFS_getSearchPath(); *i != NULL; i++)
 *     printf("[%s] is in the search path.\n", *i);
 *
 * When you are done with the returned information, you may dispose of the
 *  resources by calling PHYSFS_freeList() with the returned pointer.
 *
 *   @return Null-terminated array of null-terminated strings.
 */
char **PHYSFS_getSearchPath(void);


/**
 * Helper function.
 *
 * Set up sane, default paths. The write path will be set to
 *  "userpath/.appName", which is created if it doesn't exist.
 *
 * The above is sufficient to make sure your program's configuration directory
 *  is separated from other clutter, and platform-independent. The period
 *  before "mygame" even hides the directory on Unix systems.
 *
 *  The search path will be:
 *
 *    - The Write Path (created if it doesn't exist)
 *    - The Write Path/appName (created if it doesn't exist)
 *    - The Base Path (PHYSFS_getBasePath())
 *    - The Base Path/appName (if it exists)
 *    - All found CD-ROM paths (optionally)
 *    - All found CD-ROM paths/appName (optionally, and if they exist)
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
 *    @param appName Program-specific name of your program, to separate it
 *                   from other programs using PhysicsFS.
 *
 *    @param archiveExt File extention used by your program to specify an
 *                      archive. For example, Quake 3 uses "pk3", even though
 *                      they are just zipfiles. Specify NULL to not dig out
 *                      archives automatically.
 *
 *    @param includeCdRoms Non-zero to include CD-ROMs in the search path, and
 *                         (if (archiveExt) != NULL) search them for archives.
 *                         This may cause a significant amount of blocking
 *                         while discs are accessed, and if there are no discs
 *                         in the drive (or even not mounted on Unix systems),
 *                         then they may not be made available anyhow. You may
 *                         want to specify zero and handle the disc setup
 *                         yourself.
 *
 *    @param archivesFirst Non-zero to prepend the archives to the search path.
 *                          Zero to append them. Ignored if !(archiveExt).
 */
void PHYSFS_setSanePaths(const char *appName, const char *archiveExt,
                         int includeCdRoms, int archivesFirst);


/**
 * Create a directory. This is specified in platform-independent notation in
 *  relation to the write path. All missing parent directories are also
 *  created if they don't exist.
 *
 * So if you've got the write path set to "C:\mygame\writepath" and call
 *  PHYSFS_mkdir("downloads/maps") then the directories
 *  "C:\mygame\writepath\downloads" and "C:\mygame\writepath\downloads\maps"
 *  will be created if possible. If the creation of "maps" fails after we
 *  have successfully created "downloads", then the function leaves the
 *  created directory behind and reports failure.
 *
 *   @param dirname New path to create.
 *  @return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
int PHYSFS_mkdir(const char *dirName);


/**
 * Delete a file or directory. This is specified in platform-independent
 *  notation in relation to the write path.
 *
 * A directory must be empty before this call can delete it.
 *
 * So if you've got the write path set to "C:\mygame\writepath" and call
 *  PHYSFS_delete("downloads/maps/level1.map") then the file
 *  "C:\mygame\writepath\downloads\maps\level1.map" is removed from the
 *  physical filesystem, if it exists and the operating system permits the
 *  deletion.
 *
 * Note that on Unix systems, deleting a file may be successful, but the
 *  actual file won't be removed until all processes that have an open
 *  filehandle to it (including your program) close their handles.
 *
 *   @param filename Filename to delete.
 *  @return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
int PHYSFS_delete(const char *filename);


/**
 * Enable symbolic links. Some physical filesystems and archives contain
 *  files that are just pointers to other files. On the physical filesystem,
 *  opening such a link will (transparently) open the file that is pointed to.
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
 * Symbolic link permission can be enabled or disabled at any time, and is
 *  disabled by default.
 *
 *   @param allow nonzero to permit symlinks, zero to deny linking.
 */
void PHYSFS_permitSymbolicLinks(int allow);


/**
 * Figure out where in the search path a file resides. The file is specified
 *  in platform-independent notation. The returned filename will be the
 *  element of the search path where the file was found, which may be a
 *  directory, or an archive. Even if there are multiple matches in different
 *  parts of the search path, only the first one found is used, just like
 *  when opening a file.
 *
 * So, if you look for "maps/level1.map", and C:\mygame is in your search
 *  path and C:\mygame\maps\level1.map exists, then "C:\mygame" is returned.
 *
 * If a match is a symbolic link, and you've not explicitly permitted symlinks,
 *  then it will be ignored, and the search for a match will continue.
 *
 *     @param filename file to look for.
 *    @return READ ONLY string of element of search path containing the
 *             the file in question. NULL if not found.
 */
const char *PHYSFS_getRealPath(const char *filename);



/**
 * Get a file listing of a search path's directory. Matching directories are
 *  interpolated. That is, if "C:\mypath" is in the search path and contains a
 *  directory "savegames" that contains "x.sav", "y.sav", and "z.sav", and
 *  there is also a "C:\userpath" in the search path that has a "savegames"
 *  subdirectory with "w.sav", then the following code:
 *
 * ------------------------------------------------
 * char **rc = PHYSFS_enumerateFiles("savegames");
 * char **i;
 *
 * for (i = rc; *i != NULL; i++)
 *     printf("We've got [%s].\n", *i);
 *
 * PHYSFS_freeList(rc);
 * ------------------------------------------------
 *
 *  ...will print:
 *
 * ------------------------------------------------
 * We've got [x.sav].
 * We've got [y.sav].
 * We've got [z.sav].
 * We've got [w.sav].
 * ------------------------------------------------
 *
 * Don't forget to call PHYSFS_freeList() with the return value from this
 *  function when you are done with it.
 *
 *    @param path directory in platform-independent notation to enumerate.
 *   @return Null-terminated array of null-terminated strings.
 */
char **PHYSFS_enumerateFiles(const char *path);


/**
 * Open a file for writing, in platform-independent notation and in relation
 *  to the write path as the root of the writable filesystem. The specified
 *  file is created if it doesn't exist. If it does exist, it is truncated to
 *  zero bytes, and the writing offset is set to the start.
 *
 *   @param filename File to open.
 *  @return A valid PhysicsFS filehandle on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 */
PHYSFS_file *PHYSFS_openWrite(const char *filename);


/**
 * Open a file for writing, in platform-independent notation and in relation
 *  to the write path as the root of the writable filesystem. The specified
 *  file is created if it doesn't exist. If it does exist, the writing offset
 *  is set to the end of the file, so the first write will be the byte after
 *  the end.
 *
 *   @param filename File to open.
 *  @return A valid PhysicsFS filehandle on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 */
PHYSFS_file *PHYSFS_openAppend(const char *filename);


/**
 * Open a file for reading, in platform-independent notation. The search path
 *  is checked one at a time until a matching file is found, in which case an
 *  abstract filehandle is associated with it, and reading may be done.
 *  The reading offset is set to the first byte of the file.
 *
 *   @param filename File to open.
 *  @return A valid PhysicsFS filehandle on success, NULL on error. Specifics
 *           of the error can be gleaned from PHYSFS_getLastError().
 */
PHYSFS_file *PHYSFS_openRead(const char *filename);


/**
 * Close a PhysicsFS filehandle. This call is capable of failing if the
 *  operating system was buffering writes to this file, and (now forced to
 *  write those changes to physical media) can not store the data for any
 *  reason. In such a case, the filehandle stays open. A well-written program
 *  should ALWAYS check the return value from the close call in addition to
 *  every writing call!
 *
 *   @param handle handle returned from PHYSFS_open*().
 *  @return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
int PHYSFS_close(PHYSFS_file *handle);


/**
 * Read data from a PhysicsFS filehandle. The file must be opened for reading.
 *
 *   @param handle handle returned from PHYSFS_openRead().
 *   @param buffer buffer to store read data into.
 *   @param objSize size in bytes of objects being read from (handle).
 *   @param objCount number of (objSize) objects to read from (handle).
 *  @return number of objects read. PHYSFS_getLastError() can shed light on
 *           the reason this might be < (objCount), as can PHYSFS_eof().
 */
int PHYSFS_read(PHYSFS_file *handle, void *buffer,
                unsigned int objSize, unsigned int objCount);


/**
 * Write data to a PhysicsFS filehandle. The file must be opened for writing.
 *
 *   @param handle retval from PHYSFS_openWrite() or PHYSFS_openAppend().
 *   @param buffer buffer to store read data into.
 *   @param objSize size in bytes of objects being read from (handle).
 *   @param objCount number of (objSize) objects to read from (handle).
 *  @return number of objects read. PHYSFS_getLastError() can shed light on
 *           the reason this might be < (objCount).
 */
int PHYSFS_write(PHYSFS_file *handle, void *buffer,
                 unsigned int objSize, unsigned int objCount);


/**
 * Determine if the end of file has been reached in a PhysicsFS filehandle.
 *
 *   @param handle handle returned from PHYSFS_openRead().
 *  @return nonzero if EOF, zero if not.
 */
int PHYSFS_eof(PHYSFS_file *handle);


/**
 * Determine current position within a PhysicsFS filehandle.
 *
 *   @param handle handle returned from PHYSFS_open*().
 *  @return offset in bytes from start of file. -1 if error occurred.
 *           Specifics of the error can be gleaned from PHYSFS_getLastError().
 */
int PHYSFS_tell(PHYSFS_file *handle);


/**
 * Seek to a new position within a PhysicsFS filehandle. The next read or write
 *  will occur at that place. Seeking past the beginning or end of the file is
 *  not allowed.
 *
 *   @param handle handle returned from PHYSFS_open*().
 *   @param pos number of bytes from start of file to seek to.
 *  @return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
int PHYSFS_seek(PHYSFS_file *handle, int pos);

#ifdef __cplusplus
}
#endif

#endif  /* !defined _INCLUDE_PHYSFS_H_ */

/* end of physfs.h ... */

