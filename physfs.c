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

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct __PHYSFS_ERRMSGTYPE__
{
    int tid;
    int errorAvailable;
    char errorString[80];
    struct __PHYSFS_ERRMSGTYPE__ *next;
} ErrMsg;

typedef struct __PHYSFS_SEARCHDIRINFO__
{
    char *dirName;
    DirReader *reader;
    struct __PHYSFS_SEARCHDIRINFO__ *next;
} SearchDirInfo;

typedef struct __PHYSFS_FILEHANDLELIST__
{
    FileHandle *handle;
    struct __PHYSFS_FILEHANDLELIST__ *next;
} FileHandleList;


/* The various i/o drivers... */

#if (defined PHYSFS_SUPPORTS_ZIP)
extern const PHYSFS_ArchiveInfo   __PHYSFS_ArchiveInfo_ZIP;
extern const DirFunctions         __PHYSFS_DirFunctions_ZIP;
#endif

extern const DirFunctions  __PHYSFS_DirFunctions_DIR;

static const PHYSFS_ArchiveInfo *supported_types[] =
{
#if (defined PHYSFS_SUPPORTS_ZIP)
    &__PHYSFS_ArchiveInfo_ZIP,
#endif

    NULL
};

static const DirFunctions *dirFunctions[] =
{
#if (defined PHYSFS_SUPPORTS_ZIP)
    &__PHYSFS_DirFunctions_ZIP,
#endif

    &__PHYSFS_DirFunctions_DIR,
    NULL
};



/* General PhysicsFS state ... */

static int initialized = 0;
static ErrMsg *errorMessages = NULL;
static SearchDirInfo *searchPath = NULL;
static FileHandleList *openWriteList = NULL;
static FileHandleList *openReadList = NULL;
static char *baseDir = NULL;
static char *userDir = NULL;
static char *writeDir = NULL;
static int allowSymLinks = 0;



/* functions ... */

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


void __PHYSFS_setError(const char *str)
{
    ErrMsg *err = findErrorForCurrentThread();

    if (err == NULL)
    {
        err = (ErrMsg *) malloc(sizeof (ErrMsg));
        if (err == NULL)
            return;   /* uhh...? */

        err->tid = __PHYSFS_platformGetThreadID();
        err->next = errorMessages;
        errorMessages = err;
    } /* if */

    err->errorAvailable = 1;
    strncpy(err->errorString, str, sizeof (err->errorString));
    err->errorString[sizeof (err->errorString) - 1] = '\0';
} /* __PHYSFS_setError */


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


static const char *calculateUserDir(void)
{
    char *retval = NULL;
    const char *str = NULL;

    str = __PHYSFS_platformGetUserDir();
    if (str != NULL)
        retval = str;
    else
    {
        const char *dirsep = PHYSFS_getDirSeparator();
        const char *uname = __PHYSFS_platformGetUserName();

        str = (uname != NULL) ? uname : "default";
        retval = malloc(strlen(baseDir) + strlen(str) +
                        (strlen(dirsep) * 2) + 6);

        if (retval == NULL)
            __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        else
            sprintf(retval, "%s%susers%s%s", baseDir, dirsep, dirsep, str);

        if (uname != NULL)
            free(uname);
    } /* else */

    return(retval);
} /* calculateUserDir */


int PHYSFS_init(const char *argv0)
{
    BAIL_IF_MACRO(initialized, ERR_IS_INITIALIZED, 0);
    BAIL_IF_MACRO(argv0 == NULL, ERR_INVALID_ARGUMENT, 0);

    baseDir = calculateBaseDir(argv0);
    if (baseDir == NULL)
        return(0);

    userDir = calculateUserDir();
    if (userDir == NULL)
    {
        free(baseDir);
        baseDir = NULL;
        return(0);
    } /* if */

    initialized = 1;
    return(1);
} /* PHYSFS_init */


static void freeSearchDir(SearchDirInfo *sdi)
{
    FileHandleList *i;

    assert(sdi != NULL);
    for (i = openReadList; i != NULL; i = i->next)
    {
        BAIL_IF_MACRO(i->handle->dirReader == sdi->reader,
                        ERR_FILES_OPEN_READ, 0);
    } /* for */

    sdi->reader->close(sdi->reader);
    free(sdi->dirName);
    free(sdi);
} /* freeSearchDir */


static void closeFileHandleList(FileHandleList **list)
{
    FileHandleList *i;
    FileHandleList *next = NULL;

    for (i = *list; i != NULL; i = next)
    {
        next = i->next;
        i->handle->close(i->handle);
    } /* for */

    *list = NULL;
} /* closeAllFiles */


static void freeSearchPath(void)
{
    SearchDirInfo *i;
    SearchDirInfo *next = NULL;

    closeFileHandleList(&openReadList);

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

    closeFileHandleList(&openWriteList);
    PHYSFS_setWriteDir(NULL);
    freeSearchPath();
    freeErrorMessages();

    if (baseDir != NULL)
    {
        free(baseDir);
        baseDir = NULL;
    } /* if */

    if (userDir != NULL)
    {
        free(userDir);
        userDir = NULL;
    } /* if */

    allowSymLinks = 0;
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
    return(userDir);   /* this is calculated in PHYSFS_init()... */
} /* PHYSFS_getUserDir */


const char *PHYSFS_getWriteDir(void)
{
    return(writeDir);
} /* PHYSFS_getWriteDir */


int PHYSFS_setWriteDir(const char *newDir)
{
    BAIL_IF_MACRO(openWriteList != NULL, ERR_FILES_OPEN_WRITE, 0);

    if (writeDir != NULL)
    {
        free(writeDir);
        writeDir = NULL;
    } /* if */

    if (newDir != NULL)
    {
        BAIL_IF_MACRO(!createDirs_dependent(newDir), ERR_NO_DIR_CREATE, 0);

        writeDir = malloc(strlen(newDir) + 1);
        BAIL_IF_MACRO(writeDir == NULL, ERR_OUT_OF_MEMORY, 0);

        strcpy(writeDir, newDir);
    } /* if */

    return(1);
} /* PHYSFS_setWriteDir */


static DirReader *getDirReader(const char *d)
{
    DirFunctions **i;

    for (i = dirFunctions; *i != NULL; i++)
    {
        if ((*i)->isArchive(d))
            return( (*i)->openArchive(d) );
    } /* for */

    __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
    return(NULL);
} /* getDirReader */


int PHYSFS_addToSearchPath(const char *newDir, int appendToPath)
{
    char *str = NULL;
    SearchDirInfo *sdi = NULL;
    DirReader *dirReader = NULL;

    BAIL_IF_MACRO(newDir == NULL, ERR_INVALID_ARGUMENT, 0);

    reader = getDirReader(newDir); /* This sets the error message. */
    if (reader == NULL)
        return(0);

    sdi = (SearchDirInfo *) malloc(sizeof (SearchDirInfo));
    if (sdi == NULL)
        reader->close(reader);
    BAIL_IF_MACRO(sdi == NULL, ERR_OUT_OF_MEMORY, 0);

    sdi->dirName = (char *) malloc(strlen(newDir) + 1);
    if (sdi->dirName == NULL)
    {
        free(sdi);
        reader->close(reader);
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        return(0);
    } /* if */

    sdi->dirReader = dirReader;
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
    SearchDirInfo *next = NULL;

    BAIL_IF_MACRO(oldDir == NULL, ERR_INVALID_ARGUMENT, 0);

    for (i = searchPath; i != NULL; i = i->next)
    {
        if (strcmp(i->dirName, oldDir) == 0)
        {
            next = i->next;
            if (!freeSearchDir(i))
                return(0);

            if (prev == NULL)
                searchPath = next;
            else
                prev->next = next;

            return(1);
        } /* if */
        prev = i;
    } /* for */

    __PHYSFS_setError(ERR_NOT_IN_SEARCH_PATH);
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
            __PHYSFS_setError(ERR_OUT_OF_MEMORY);
            return(NULL);
        } /* if */

        strcpy(retval[x], i->dirName);
    } /* for */

    return(retval);
} /* PHYSFS_getSearchPath */


int PHYSFS_setSaneConfig(const char *appName, const char *archiveExt,
                         int includeCdRoms, int archivesFirst)
{
    const char *basedir = PHYSFS_getBaseDir();
    const char *userdir = PHYSFS_getUserDir();
    const char *dirsep = PHYSFS_getDirSeparator();
    char *str;
    int rc;

        /* set write dir... */
    str = malloc(strlen(userdir) + (strlen(appName) * 2) +
                 (strlen(dirsep) * 2) + 2);
    BAIL_IF_MACRO(str == NULL, ERR_OUT_OF_MEMORY, 0);
    sprintf(str, "%s%s.%s", userdir, dirsep, appName);
    rc = PHYSFS_setWriteDir(str);
    if (!rc)
        return(0);  /* error set by PHYSFS_setWriteDir() ... */

        /* Put write dir related dirs on search path... */
    PHYSFS_addToSearchPath(str, 1);
    PHYSFS_mkdir(appName); /* don't care if this fails. */
    strcat(str, dirSep);
    strcat(str, appName);
    PHYSFS_addToSearchPath(str, 1);
    free(str);

        /* Put base path stuff on search path... */
    PHYSFS_addToSearchPath(basedir, 1);
    str = malloc(strlen(basedir) + (strlen(appName) * 2) +
                 (strlen(dirsep) * 2) + 2);
    if (str != NULL)
    {
        sprintf(str, "%s%s.%s", basedir, dirsep, appName);
        PHYSFS_addToSearchPath(str, 1);
        free(str);
    } /* if */

        /* handle CD-ROMs... */
    if (includeCdRoms)
    {
        char **cds = PHYSFS_getCdRomDirs();
        char **i;
        for (i = cds; *i != NULL; i++)
        {
            PHYSFS_addToSearchPath(*i);
            str = malloc(strlen(*i) + strlen(appName) + strlen(dirsep) + 1);
            if (str != NULL)
            {
                sprintf(str, "%s%s%s", *i, dirsep, appName);
                PHYSFS_addToSearchPath(str);
                free(str);
            } /* if */
        } /* for */
        PHYSFS_freeList(cds);
    } /* if */

        /* Root out archives, and add them to search path... */
    if (archiveExt != NULL)
    {
        char **rc = PHYSFS_enumerateFiles("");
        char **i;
        int extlen = strlen(archiveExt);
        char *ext;

        for (i = rc; *i != NULL; i++)
        {
            int l = strlen(*i);
            if ((l > extlen) && ((*i)[l - extlen - 1] == '.'));
            {
                ext = (*i) + (l - extlen);
                if (__PHYSFS_platformStricmp(ext, archiveExt) == 0)
                {
                    const char *d = PHYSFS_getRealDir(*i);
                    str = malloc(strlen(d) + strlen(dirsep) + l + 1);
                    if (str != NULL)
                    {
                        sprintf(str, "%s%s%s", d, dirsep, *i);
                        PHYSFS_addToSearchPath(d, str);
                        free(str);
                    } /* if */
                } /* if */
            } /* if */
        } /* for */

        PHYSFS_freeList(rc);
    } /* if */

    return(1);
} /* PHYSFS_setSaneConfig */


/* string manipulation in C makes my ass itch. */
/*  be sure to free this crap after you're done with it. */
static char *convertToDependentNotation(const char *prepend,
                                        const char *dirName,
                                        const char *append)
{
    const char *dirsep = PHYSFS_getDirSeparator();
    int sepsize = strlen(dirsep);
    char *str;
    char *i1;
    char *i2;
    size_t allocSize;

    allocSize = strlen(dirName) + strlen(writeDir) + sepsize + 1;
    if (prepend != NULL)
        allocSize += strlen(prepend) + sepsize;
    if (append != NULL)
        allocSize += strlen(append) + sepsize;

        /* make sure there's enough space if the dir separator is bigger. */
    if (sepsize > 1)
    {
        for (str = dirName; *str != '\0'; str++)
        {
            if (*str == '/')
                allocSize += (sepsize - 1);
        } /* for */
    } /* if */

    str = (char *) malloc(allocSize);
    BAIL_IF_MACRO(str == NULL, ERR_OUT_OF_MEMORY, NULL);
    *str = '\0';

    if (prepend)
    {
        strcpy(str, prepend);
        strcat(str, dirsep);
    } /* if */

    for (i1 = dirName, i2 = str + strlen(str); *i1 != '\0'; i1++, i2++)
    {
        if (*i1 == '/')
        {
            strcpy(i2, dirsep);
            i2 += sepsize;
        } /* if */
        else
        {
            *i2 = *i1;
        } /* else */
    } /* for */
    *i2 = '\0';

    if (append)
    {
        strcat(str, dirsep);
        strcpy(str, append);
    } /* if */

    return(str);
} /* convertToDependentNotation */


int PHYSFS_mkdir(const char *dirName)
{
    char *str;
    int rc;

    BAIL_IF_MACRO(writeDir == NULL, ERR_NO_WRITE_DIR, NULL);

    str = convertToDependentNotation(writeDir, dirName, NULL);
    if (str == NULL)  /* __PHYSFS_setError is called in convert call. */
        return(0);

    rc = createDirs_dependent(str);
    free(str);
    return(rc);
} /* PHYSFS_mkdir */


int PHYSFS_delete(const char *filename)
{
    char *str;
    int rc;

    BAIL_IF_MACRO(writeDir == NULL, ERR_NO_WRITE_DIR, NULL);

    str = convertToDependentNotation(writeDir, fileName, NULL);
    if (str == NULL)  /* __PHYSFS_setError is called in convert call. */
        return(0);

    rc = remove(str);
    free(str);

    rc = (rc == 0);
    if (!rc)
        __PHYSFS_setError(strerror(errno));

    return(rc);
} /* PHYSFS_delete */


void PHYSFS_permitSymbolicLinks(int allow)
{
    allowSymLinks = allow;
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
    FileHandle *h = (FileHandle *) handle->opaque;
    FileHandleList *i;
    FileHandleList **lists[] = { &openWriteList, &openReadList, NULL };
    int rc;

    assert(h != NULL);
    assert(h->funcs != NULL);
    assert(h->funcs->close != NULL);

    while (lists != NULL)
    {
        for (i = *(*lists); i != NULL; i = i->next)
        {
            if (i->handle == h)
            {
                rc = h->close(h);
                if (!rc)
                    return(0);

                if (prev == NULL)
                    *lists = i->next;
                else
                    prev->next = i->next;
                free(i);
                free(handle);
                return(1);
            } /* if */
        } /* for */
        lists++;
    } /* while */

    assert(0);  /* shouldn't EVER hit this. */
} /* PHYSFS_close */


int PHYSFS_read(PHYSFS_file *handle, void *buffer,
                unsigned int objSize, unsigned int objCount)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    assert(h != NULL);
    assert(h->funcs != NULL);
    BAIL_IF_MACRO(h->funcs->read == NULL, ERR_NOT_SUPPORTED, -1);
    return(h->funcs->read(h, buffer, objSize, objCount));
} /* PHYSFS_read */


int PHYSFS_write(PHYSFS_file *handle, void *buffer,
                 unsigned int objSize, unsigned int objCount)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    assert(h != NULL);
    assert(h->funcs != NULL);
    BAIL_IF_MACRO(h->funcs->write == NULL, ERR_NOT_SUPPORTED, -1);
    return(h->funcs->write(h, buffer, objSize, objCount));
} /* PHYSFS_write */


int PHYSFS_eof(PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    assert(h != NULL);
    assert(h->funcs != NULL);
    BAIL_IF_MACRO(h->funcs->eof == NULL, ERR_NOT_SUPPORTED, -1);
    return(h->funcs->eof(h));
} /* PHYSFS_eof */


int PHYSFS_tell(PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    assert(h != NULL);
    assert(h->funcs != NULL);
    BAIL_IF_MACRO(h->funcs->tell == NULL, ERR_NOT_SUPPORTED, -1);
    return(h->funcs->tell(h));
} /* PHYSFS_tell */


int PHYSFS_seek(PHYSFS_file *handle, int pos)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    assert(h != NULL);
    assert(h->funcs != NULL);
    BAIL_IF_MACRO(h->funcs->seek == NULL, ERR_NOT_SUPPORTED, 0);
    return(h->funcs->seek(h, pos));
} /* PHYSFS_seek */

/* end of physfs.c ... */

