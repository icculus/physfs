/**
 * PhysicsFS; a portable, flexible file i/o abstraction.
 *
 * Documentation is in physfs.h. It's verbose, honest.  :)
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "physfs.h"

typedef struct __PHYSFS_ERRMSGTYPE__
{
    int tid;
    int errorAvailable;
    char errorString[80];
    struct __PHYSFS_ERRMSGTYPE__ *next;
} ErrMsg;

/* !!!
typedef struct __PHYSFS_READER__
{
    
} Reader;
*/

typedef struct __PHYSFS_SEARCHDIRINFO__
{
    char *dirName;
    Reader *reader;
    struct __PHYSFS_SEARCHDIRINFO__ *next;
} SearchDirInfo;


static int initialized = 0;
static ErrMsg **errorMessages = NULL;  /* uses list functions. */
static char **searchPath = NULL;  /* uses list functions. */
static char *baseDir = NULL;
static char *writeDir = NULL;

static const PHYSFS_ArchiveInfo *supported_types[] =
{
#if (defined PHYSFS_SUPPORTS_ZIP)
    { "ZIP", "PkZip/WinZip/Info-Zip compatible" },
#endif

    NULL
};


/* error messages... */
#define ERR_IS_INITIALIZED       "Already initialized"
#define ERR_NOT_INITIALIZED      "Not initialized"
#define ERR_INVALID_ARGUMENT     "Invalid argument"
#define ERR_FILES_OPEN_WRITE     "Files still open for writing"
#define ERR_NO_DIR_CREATE        "Failed to create directories"
#define ERR_OUT_OF_MEMORY        "Out of memory"
#define ERR_NOT_IN_SEARCH_PATH   "No such entry in search path"


/* This gets used all over for lessening code clutter. */
#define BAIL_IF_MACRO(cond, err, rc) if (cond) { setError(err); return(rc); }


static ErrMsg *findErrorForCurrentThread(void)
{
    ErrMsg *i;
    int tid;

    if (errorMessages != NULL)
    {
        tid = __PHYSFS_platformGetThreadID();

        for (i = errorMessages; i != NULL; i = i->next)
        {
            if (i->tid == tid)
                return(i);
        } /* for */
    } /* if */

    return(NULL);   /* no error available. */
} /* findErrorForCurrentThread */


static void setError(char *str)
{
    ErrMsg *err = findErrorForCurrentThread();

    if (err == NULL)
    {
        err = (ErrMsg *) malloc(sizeof (ErrMsg));
        if (err == NULL)
            return;   /* uhh...? */

        err->next = errorMessages;
        if (errorMessages == NULL)
            errorMessages = err;

        err->tid = __PHYSFS_platformGetThreadID();
    } /* if */

    err->errorAvailable = 1;
    strncpy(err->errorString, str, sizeof (err->errorString));
    err->errorString[sizeof (err->errorString) - 1] = '\0';
} /* setError */


const char *PHYSFS_getLastError(void)
{
    ErrMsg *err = findErrorForCurrentThread();

    if ((err == NULL) || (!err->errorAvailable))
        return(NULL);

    err->errorAvailable = 0;
    return(err->errorString);
} /* PHYSFS_getLastError */


void PHYSFS_getLinkedVersion(PHYSFS_Version *ver)
{
    if (ver != NULL)
    {
        ver->major = PHYSFS_VER_MAJOR;
        ver->minor = PHYSFS_VER_MINOR;
        ver->patch = PHYSFS_VER_PATCH;
    } /* if */
} /* PHYSFS_getLinkedVersion */


int PHYSFS_init(const char *argv0)
{
    BAIL_IF_MACRO(initialized, ERR_IS_INITIALIZED, 0);
    BAIL_IF_MACRO(argv0 == NULL, ERR_INVALID_ARGUMENT, 0);

    baseDir = calculateBaseDir();
    initialized = 1;
    return(1);
} /* PHYSFS_init */


static void freeSearchDir(SearchDirInfo *sdi)
{
    assert(sdi != NULL);
    freeReader(sdi->reader);
    free(sdi->dirName);
    free(sdi);
} /* freeSearchDir */


static void freeSearchPath(void)
{
    SearchDirInfo *i;
    SearchDirInfo *next = NULL;

    if (searchPath != NULL)
    {
        for (i = searchPath; i != NULL; i = next)
        {
            next = i;
            freeSearchDir(i);
        } /* for */
        searchPath = NULL;
    } /* if */
} /* freeSearchPath */


void PHYSFS_deinit(void)
{
    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);

    /* close/cleanup open handles. */

    if (baseDir != NULL)
        free(baseDir);

    PHYSFS_setWriteDir(NULL);
    freeSearchPath();
    freeErrorMessages();

    initialized = 0;
    return(1);
} /* PHYSFS_deinit */


const PHYSFS_ArchiveInfo **PHYSFS_supportedArchiveTypes(void)
{
    return(supported_types);
} /* PHYSFS_supportedArchiveTypes */


void PHYSFS_freeList(void *list)
{
    void **i;

    for (i = (void **) list; *i != NULL; i++)
        free(*i);

    free(list);
} /* PHYSFS_freeList */


const char *PHYSFS_getDirSeparator(void)
{
    return(__PHYSFS_pathSeparator);
} /* PHYSFS_getDirSeparator */


char **PHYSFS_getCdRomDirs(void)
{
    return(__PHYSFS_platformDetectAvailableCDs());
} /* PHYSFS_getCdRomDirs */


const char *PHYSFS_getBaseDir(void)
{
    return(baseDir);   /* this is calculated in PHYSFS_init()... */
} /* PHYSFS_getBaseDir */


const char *PHYSFS_getUserDir(void)
{
    return(__PHYSFS_platformGetUserDir());
} /* PHYSFS_getUserDir */


const char *PHYSFS_getWriteDir(void)
{
    return(writeDir);
} /* PHYSFS_getWriteDir */


int PHYSFS_setWriteDir(const char *newDir)
{
    BAIL_IF_MACRO(openWriteCount > 0, ERR_FILES_OPEN_WRITE, 0);

    if (writeDir != NULL)
    {
        free(writeDir);
        writeDir = NULL;
    } /* if */

    if (newDir == NULL)   /* we're done already! */
        return(1);

    BAIL_IF_MACRO(!createDirs_dependent(newDir), ERR_NO_DIR_CREATE, 0);

    writeDir = malloc(strlen(newDir) + 1);
    BAIL_IF_MACRO(writeDir == NULL, ERR_OUT_OF_MEMORY, 0);

    strcpy(writeDir, newDir);
    return(1);
} /* PHYSFS_setWriteDir */


int PHYSFS_addToSearchPath(const char *newDir, int appendToPath)
{
    char *str = NULL;
    SearchDirInfo *sdi = NULL;
    __PHYSFS_Reader *reader = NULL;

    BAIL_IF_MACRO(newDir == NULL, ERR_INVALID_ARGUMENT, 0);

    reader = getReader(newDir); /* This sets the error message. */
    if (reader == NULL)
        return(0);

    sdi = (SearchDirInfo *) malloc(sizeof (SearchDirInfo));
    BAIL_IF_MACRO(sdi == NULL, ERR_OUT_OF_MEMORY, 0);

    sdi->dirName = (char *) malloc(strlen(newDir) + 1);
    if (sdi->dirName == NULL)
    {
        free(sdi);
        freeReader(reader);
        setError(ERR_OUT_OF_MEMORY);
        return(0);
    } /* if */

    sdi->reader = reader;
    strcpy(sdi->dirName, newDir);

    if (appendToPath)
    {
        sdi->next = searchPath;
        searchPath = sdi;
    } /* if */
    else
    {
        SearchDirInfo *i = searchPath;
        SearchDirInfo *prev = NULL;

        sdi->next = NULL;
        while (i != NULL)
            prev = i;

        if (prev == NULL)
            searchPath = sdi;
        else
            prev->next = sdi;
    } /* else */

    return(1);
} /* PHYSFS_addToSearchPath */


int PHYSFS_removeFromSearchPath(const char *oldDir)
{
    SearchDirInfo *i;
    SearchDirInfo *prev = NULL;

    BAIL_IF_MACRO(oldDir == NULL, ERR_INVALID_ARGUMENT, 0);

    for (i = searchPath; i != NULL; i = i->next)
    {
        if (strcmp(i->dirName, oldDir) == 0)
        {
            if (prev == NULL)
                searchPath = i->next;
            else
                prev->next = i->next;
            freeSearchDir(i);
            return(1);
        } /* if */
        prev = i;
    } /* for */

    setError(ERR_NOT_IN_SEARCH_PATH);
    return(0);
} /* PHYSFS_removeFromSearchPath */


char **PHYSFS_getSearchPath(void)
{
    int count = 1;
    int x;
    SearchDirInfo *i;
    char **retval;

    for (i = searchPath; i != NULL; i = i->next)
        count++;

    retval = (char **) malloc(sizeof (char *) * count);
    BAIL_IF_MACRO(!retval, ERR_OUT_OF_MEMORY, NULL);
    count--;
    retval[count] = NULL;

    for (i = searchPath, x = 0; x < count; i = i->next, x++)
    {
        retval[x] = (char *) malloc(strlen(i->dirName) + 1);
        if (retval[x] == NULL)  /* this is friggin' ugly. */
        {
            while (x > 0)
            {
                x--;
                free(retval[x]);
            } /* while */

            free(retval);
            setError(ERR_OUT_OF_MEMORY);
            return(NULL);
        } /* if */

        strcpy(retval[x], i->dirName);
    } /* for */

    return(retval);
} /* PHYSFS_getSearchPath */


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
 *    - The Write Dir (created if it doesn't exist)
 *    - The Write Dir/appName (created if it doesn't exist)
 *    - The Base Dir (PHYSFS_getBaseDir())
 *    - The Base Dir/appName (if it exists)
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
void PHYSFS_setSaneConfig(const char *appName, const char *archiveExt,
                         int includeCdRoms, int archivesFirst)
{
} /* PHYSFS_setSaneConfig */


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
 *   @param dirName New path to create.
 *  @return nonzero on success, zero on error. Specifics of the error can be
 *          gleaned from PHYSFS_getLastError().
 */
int PHYSFS_mkdir(const char *dirName)
{
} /* PHYSFS_mkdir */


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
int PHYSFS_delete(const char *filename)
{
} /* PHYSFS_delete */


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
void PHYSFS_permitSymbolicLinks(int allow)
{
} /* PHYSFS_permitSymbolicLinks */


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
const char *PHYSFS_getRealDir(const char *filename)
{
} /* PHYSFS_getRealDir */



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
char **PHYSFS_enumerateFiles(const char *path)
{
} /* PHYSFS_enumerateFiles */


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
PHYSFS_file *PHYSFS_openWrite(const char *filename)
{
} /* PHYSFS_openWrite */


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
PHYSFS_file *PHYSFS_openAppend(const char *filename)
{
} /* PHYSFS_openAppend */


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
PHYSFS_file *PHYSFS_openRead(const char *filename)
{
} /* PHYSFS_openRead */


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
int PHYSFS_close(PHYSFS_file *handle)
{
} /* PHYSFS_close */


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
                unsigned int objSize, unsigned int objCount)
{
} /* PHYSFS_read */


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
                 unsigned int objSize, unsigned int objCount)
{
} /* PHYSFS_write */


/**
 * Determine if the end of file has been reached in a PhysicsFS filehandle.
 *
 *   @param handle handle returned from PHYSFS_openRead().
 *  @return nonzero if EOF, zero if not.
 */
int PHYSFS_eof(PHYSFS_file *handle)
{
} /* PHYSFS_eof */


/**
 * Determine current position within a PhysicsFS filehandle.
 *
 *   @param handle handle returned from PHYSFS_open*().
 *  @return offset in bytes from start of file. -1 if error occurred.
 *           Specifics of the error can be gleaned from PHYSFS_getLastError().
 */
int PHYSFS_tell(PHYSFS_file *handle)
{
} /* PHYSFS_tell */


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
int PHYSFS_seek(PHYSFS_file *handle, int pos)
{
} /* PHYSFS_seek */


/* end of physfs.h ... */

