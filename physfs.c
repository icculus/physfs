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

typedef struct __PHYSFS_DIRINFO__
{
    char *dirName;
    DirHandle *dirHandle;
    struct __PHYSFS_DIRINFO__ *next;
} DirInfo;

typedef struct __PHYSFS_FILEHANDLELIST__
{
    PHYSFS_file *handle;
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
static DirInfo *searchPath = NULL;
static DirInfo *writeDir = NULL;
static FileHandleList *openWriteList = NULL;
static FileHandleList *openReadList = NULL;
static char *baseDir = NULL;
static char *userDir = NULL;
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
    ErrMsg *err;

    if (str == NULL)
        return;

    err = findErrorForCurrentThread();

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


static void freeErrorMessages(void)
{
    ErrMsg *i;
    ErrMsg *next;

    for (i = errorMessages; i != NULL; i = next)
    {
        next = i;
        free(i);
    } /* for */
} /* freeErrorMessages */


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


static DirHandle *openDirectory(const char *d, int forWriting)
{
    const DirFunctions **i;

    for (i = dirFunctions; *i != NULL; i++)
    {
        if ((*i)->isArchive(d, forWriting))
            return( (*i)->openArchive(d, forWriting) );
    } /* for */

    __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
    return(NULL);
} /* openDirectory */


static DirInfo *buildDirInfo(const char *newDir, int forWriting)
{
    DirHandle *dirHandle = NULL;
    DirInfo *di = NULL;

    BAIL_IF_MACRO(newDir == NULL, ERR_INVALID_ARGUMENT, 0);

    dirHandle = openDirectory(newDir, forWriting);
    BAIL_IF_MACRO(dirHandle == NULL, NULL, 0);

    di = (DirInfo *) malloc(sizeof (DirInfo));
    if (di == NULL)
        dirHandle->funcs->close(dirHandle);
    BAIL_IF_MACRO(di == NULL, ERR_OUT_OF_MEMORY, 0);

    di->dirName = (char *) malloc(strlen(newDir) + 1);
    if (di->dirName == NULL)
    {
        free(di);
        dirHandle->funcs->close(dirHandle);
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        return(0);
    } /* if */

    di->next = NULL;
    di->dirHandle = dirHandle;
    strcpy(di->dirName, newDir);
    return(di);
} /* buildDirInfo */


static int freeDirInfo(DirInfo *di, FileHandleList *openList)
{
    FileHandleList *i;

    if (di == NULL)
        return(1);

    for (i = openList; i != NULL; i = i->next)
    {
        const DirHandle *h = ((FileHandle *) i->handle->opaque)->dirHandle;
        BAIL_IF_MACRO(h == di->dirHandle, ERR_FILES_STILL_OPEN, 0);
    } /* for */

    di->dirHandle->funcs->close(di->dirHandle);
    free(di->dirName);
    free(di);
    return(1);
} /* freeDirInfo */


static char *calculateUserDir(void)
{
    char *retval = NULL;
    const char *str = NULL;

    str = __PHYSFS_platformGetUserDir();
    if (str != NULL)
        retval = (char *) str;
    else
    {
        const char *dirsep = PHYSFS_getDirSeparator();
        const char *uname = __PHYSFS_platformGetUserName();

        str = (uname != NULL) ? uname : "default";
        retval = (char *) malloc(strlen(baseDir) + strlen(str) +
                                 (strlen(dirsep) * 2) + 6);

        if (retval == NULL)
            __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        else
            sprintf(retval, "%s%susers%s%s", baseDir, dirsep, dirsep, str);

        if (uname != NULL)
            free((void *) uname);
    } /* else */

    return(retval);
} /* calculateUserDir */


static char *calculateBaseDir(const char *argv0)
{
assert(0); return(NULL);
} /* calculateBaseDir */


int PHYSFS_init(const char *argv0)
{
    BAIL_IF_MACRO(initialized, ERR_IS_INITIALIZED, 0);
    BAIL_IF_MACRO(argv0 == NULL, ERR_INVALID_ARGUMENT, 0);

    baseDir = calculateBaseDir(argv0);
    BAIL_IF_MACRO(baseDir == NULL, NULL, 0);

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


static int closeFileHandleList(FileHandleList **list)
{
    FileHandleList *i;
    FileHandleList *next = NULL;
    FileHandle *h;

    for (i = *list; i != NULL; i = next)
    {
        next = i->next;
        h = (FileHandle *) (i->handle->opaque);
        if (!h->funcs->close(i->handle->opaque))
        {
            *list = i;
            return(0);
        } /* if */

        free(i->handle);
        free(i);
    } /* for */

    *list = NULL;
    return(1);
} /* closeFileHandleList */


static void freeSearchPath(void)
{
    DirInfo *i;
    DirInfo *next = NULL;

    closeFileHandleList(&openReadList);

    if (searchPath != NULL)
    {
        for (i = searchPath; i != NULL; i = next)
        {
            next = i;
            freeDirInfo(i, openReadList);
        } /* for */
        searchPath = NULL;
    } /* if */
} /* freeSearchPath */


int PHYSFS_deinit(void)
{
    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);

    closeFileHandleList(&openWriteList);
    BAIL_IF_MACRO(!PHYSFS_setWriteDir(NULL), ERR_FILES_STILL_OPEN, 0);

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
    return(__PHYSFS_platformDirSeparator);
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
    if (writeDir == NULL)
        return(NULL);

    return(writeDir->dirName);
} /* PHYSFS_getWriteDir */


int PHYSFS_setWriteDir(const char *newDir)
{
    if (writeDir != NULL)
    {
        BAIL_IF_MACRO(!freeDirInfo(writeDir, openWriteList), NULL, 0);
        writeDir = NULL;
    } /* if */

    if (newDir != NULL)
    {
        writeDir = buildDirInfo(newDir, 1);
        return(writeDir != NULL);
    } /* if */

    return(1);
} /* PHYSFS_setWriteDir */


int PHYSFS_addToSearchPath(const char *newDir, int appendToPath)
{
    DirInfo *di = buildDirInfo(newDir, 0);

    BAIL_IF_MACRO(di == NULL, NULL, 0);

    if (appendToPath)
    {
        di->next = searchPath;
        searchPath = di;
    } /* if */
    else
    {
        DirInfo *i = searchPath;
        DirInfo *prev = NULL;

        di->next = NULL;
        while (i != NULL)
            prev = i;

        if (prev == NULL)
            searchPath = di;
        else
            prev->next = di;
    } /* else */

    return(1);
} /* PHYSFS_addToSearchPath */


int PHYSFS_removeFromSearchPath(const char *oldDir)
{
    DirInfo *i;
    DirInfo *prev = NULL;
    DirInfo *next = NULL;

    BAIL_IF_MACRO(oldDir == NULL, ERR_INVALID_ARGUMENT, 0);

    for (i = searchPath; i != NULL; i = i->next)
    {
        if (strcmp(i->dirName, oldDir) == 0)
        {
            next = i->next;
            BAIL_IF_MACRO(!freeDirInfo(i, openReadList), NULL, 0);

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
    DirInfo *i;
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
    BAIL_IF_MACRO(!rc, NULL, 0);

        /* Put write dir related dirs on search path... */
    PHYSFS_addToSearchPath(str, 1);
    PHYSFS_mkdir(appName); /* don't care if this fails. */
    strcat(str, dirsep);
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
            PHYSFS_addToSearchPath(*i, 1);
            str = malloc(strlen(*i) + strlen(appName) + strlen(dirsep) + 1);
            if (str != NULL)
            {
                sprintf(str, "%s%s%s", *i, dirsep, appName);
                PHYSFS_addToSearchPath(str, 1);
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
                        PHYSFS_addToSearchPath(str, archivesFirst == 0);
                        free(str);
                    } /* if */
                } /* if */
            } /* if */
        } /* for */

        PHYSFS_freeList(rc);
    } /* if */

    return(1);
} /* PHYSFS_setSaneConfig */


void PHYSFS_permitSymbolicLinks(int allow)
{
    allowSymLinks = allow;
} /* PHYSFS_permitSymbolicLinks */


/* string manipulation in C makes my ass itch. */
char *__PHYSFS_convertToDependentNotation(const char *prepend,
                                          const char *dirName,
                                          const char *append)
{
    const char *dirsep = PHYSFS_getDirSeparator();
    int sepsize = strlen(dirsep);
    char *str;
    char *i1;
    char *i2;
    size_t allocSize;

    allocSize = strlen(dirName) + 1;
    if (prepend != NULL)
        allocSize += strlen(prepend) + sepsize;
    if (append != NULL)
        allocSize += strlen(append) + sepsize;

        /* make sure there's enough space if the dir separator is bigger. */
    if (sepsize > 1)
    {
        str = (char *) dirName;
        do
        {
            str = strchr(str, '/');
            if (str != NULL)
            {
                allocSize += (sepsize - 1);
                str++;
            } /* if */
        } while (str != NULL);
    } /* if */

    str = (char *) malloc(allocSize);
    BAIL_IF_MACRO(str == NULL, ERR_OUT_OF_MEMORY, NULL);
    *str = '\0';

    if (prepend)
    {
        strcpy(str, prepend);
        strcat(str, dirsep);
    } /* if */

    for (i1 = (char *) dirName, i2 = str + strlen(str); *i1; i1++, i2++)
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
} /* __PHYSFS_convertToDependentNotation */


int __PHYSFS_verifySecurity(DirHandle *h, const char *fname)
{
    int retval = 1;
    char *start;
    char *end;
    char *str;

    start = str = malloc(strlen(fname) + 1);
    BAIL_IF_MACRO(str == NULL, ERR_OUT_OF_MEMORY, 0);
    strcpy(str, fname);

    while (1)
    {
        end = strchr(start, '/');
        if (end != NULL)
            *end = '\0';

        if ( (strcmp(start, ".") == 0) ||
             (strcmp(start, "..") == 0) ||
             (strchr(start, ':') != NULL) )
        {
            __PHYSFS_setError(ERR_INSECURE_FNAME);
            retval = 0;
            break;
        } /* if */

        if ((!allowSymLinks) && (h->funcs->isSymLink(h, str)))
        {
            __PHYSFS_setError(ERR_SYMLINK_DISALLOWED);
            retval = 0;
            break;
        } /* if */

        if (end == NULL)
            break;

        *end = '/';
        start = end + 1;
    } /* while */

    free(str);
    return(retval);
} /* __PHYSFS_verifySecurity */


int PHYSFS_mkdir(const char *dirName)
{
    DirHandle *h;
    char *str;
    char *start;
    char *end;
    int retval = 0;

    BAIL_IF_MACRO(writeDir == NULL, ERR_NO_WRITE_DIR, 0);
    h = writeDir->dirHandle;
    BAIL_IF_MACRO(h->funcs->mkdir == NULL, ERR_NOT_SUPPORTED, 0);
    BAIL_IF_MACRO(!__PHYSFS_verifySecurity(h, dirName), NULL, 0);

    start = str = malloc(strlen(dirName) + 1);
    BAIL_IF_MACRO(str == NULL, ERR_OUT_OF_MEMORY, 0);
    strcpy(str, dirName);

    while (1)
    {
        end = strchr(start, '/');
        if (end != NULL)
            *end = '\0';

        retval = h->funcs->mkdir(h, str);
        if (!retval)
            break;

        if (end == NULL)
            break;

        *end = '/';
        start = end + 1;
    } /* while */

    free(str);
    return(retval);
} /* PHYSFS_mkdir */


int PHYSFS_delete(const char *fname)
{
    DirHandle *h;
    BAIL_IF_MACRO(writeDir == NULL, ERR_NO_WRITE_DIR, 0);
    h = writeDir->dirHandle;
    BAIL_IF_MACRO(h->funcs->remove == NULL, ERR_NOT_SUPPORTED, 0);
    BAIL_IF_MACRO(!__PHYSFS_verifySecurity(h, fname), NULL, 0);
    return(h->funcs->remove(h, fname));
} /* PHYSFS_delete */


const char *PHYSFS_getRealDir(const char *filename)
{
    DirInfo *i;

    for (i = searchPath; i != NULL; i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, filename))
        {
            if (h->funcs->exists(h, filename))
                return(i->dirName);
        } /* if */
    } /* for */

    return(NULL);
} /* PHYSFS_getRealDir */


static int countList(LinkedStringList *list)
{
    int retval = 0;
    LinkedStringList *i;

    assert(list != NULL);

    for (i = list; i != NULL; i = i->next)
        retval++;

    return(retval);
} /* countList */


static char **convertStringListToPhysFSList(LinkedStringList *finalList)
{
    int i;
    LinkedStringList *next = NULL;
    int len = countList(finalList);
    char **retval = (char **) malloc((len + 1) * sizeof (char *));

    if (retval == NULL)
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);

    for (i = 0; i < len; i++)
    {
        next = finalList->next;
        if (retval == NULL)
            free(finalList->str);
        else
            retval[i] = finalList->str;
        free(finalList);
        finalList = next;
    } /* for */

    if (retval != NULL);
        retval[i] = NULL;

    return(retval);
} /* convertStringListToPhysFSList */


static void insertStringListItem(LinkedStringList **final,
                                 LinkedStringList *item)
{
    LinkedStringList *i;
    LinkedStringList *prev = NULL;
    int rc;

    for (i = *final; i != NULL; i = i->next)
    {
        rc = strcmp(i->str, item->str);
        if (rc == 0)      /* already in list. */
        {
            free(item->str);
            free(item);
            return;
        } /* if */
        else if (rc > 0)  /* insertion point. */
        {
            if (prev == NULL)
                *final = item;
            else
                prev->next = item;
            item->next = i;
            return;
        } /* else if */
        prev = i;
    } /* for */
} /* insertStringListItem */


/* if we run out of memory anywhere in here, we give back what we can. */
static void interpolateStringLists(LinkedStringList **final,
                                    LinkedStringList *newList)
{
    LinkedStringList *next = NULL;

    while (newList != NULL)
    {
        next = newList->next;
        insertStringListItem(final, newList);
        newList = next;
    } /* while */
} /* interpolateStringLists */


char **PHYSFS_enumerateFiles(const char *path)
{
    DirInfo *i;
    char **retval = NULL;
    LinkedStringList *rc;
    LinkedStringList *finalList = NULL;

    for (i = searchPath; i != NULL; i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, path))
        {
            rc = h->funcs->enumerateFiles(h, path);
            interpolateStringLists(&finalList, rc);
        } /* if */
    } /* for */

    retval = convertStringListToPhysFSList(finalList);
    return(retval);
} /* PHYSFS_enumerateFiles */


int PHYSFS_exists(const char *fname)
{
    return(PHYSFS_getRealDir(fname) != NULL);
} /* PHYSFS_exists */


int PHYSFS_isDirectory(const char *fname)
{
    DirInfo *i;

    for (i = searchPath; i != NULL; i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, fname))
        {
            if (h->funcs->exists(h, fname))
                return(h->funcs->isDirectory(h, fname));
        } /* if */
    } /* for */

    return(0);
} /* PHYSFS_isDirectory */


int PHYSFS_isSymbolicLink(const char *fname)
{
    DirInfo *i;

    if (!allowSymLinks)
        return(0);

    for (i = searchPath; i != NULL; i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, fname))
        {
            if (h->funcs->exists(h, fname))
                return(h->funcs->isSymLink(h, fname));
        } /* if */
    } /* for */

    return(0);
} /* PHYSFS_isSymbolicLink */


static PHYSFS_file *doOpenWrite(const char *fname, int appending)
{
    PHYSFS_file *retval = (PHYSFS_file *) malloc(sizeof (PHYSFS_file));
    FileHandle *rc = NULL;
    DirHandle *h = (writeDir == NULL) ? NULL : writeDir->dirHandle;
    const DirFunctions *f = (h == NULL) ? NULL : h->funcs;
    FileHandleList *list;

    BAIL_IF_MACRO(!retval, ERR_OUT_OF_MEMORY, NULL);
    BAIL_IF_MACRO(!h, ERR_NO_WRITE_DIR, NULL);
    BAIL_IF_MACRO(!__PHYSFS_verifySecurity(h, fname), NULL, NULL);

    list = (FileHandleList *) malloc(sizeof (FileHandleList));
    BAIL_IF_MACRO(!list, ERR_OUT_OF_MEMORY, NULL);

    rc = (appending) ? f->openAppend(h, fname) : f->openWrite(h, fname);
    if (rc == NULL)
    {
        free(list);
        free(retval);
        retval = NULL;
    } /* if */
    else
    {
        retval->opaque = (void *) rc;
        list->handle = retval;
        list->next = openWriteList;
        openWriteList = list;
    } /* else */

    return(retval);
} /* doOpenWrite */


PHYSFS_file *PHYSFS_openWrite(const char *filename)
{
    return(doOpenWrite(filename, 0));
} /* PHYSFS_openWrite */


PHYSFS_file *PHYSFS_openAppend(const char *filename)
{
    return(doOpenWrite(filename, 1));
} /* PHYSFS_openAppend */


PHYSFS_file *PHYSFS_openRead(const char *fname)
{
    PHYSFS_file *retval = (PHYSFS_file *) malloc(sizeof (PHYSFS_file));
    FileHandle *rc = NULL;
    FileHandleList *list;
    DirInfo *i;

    BAIL_IF_MACRO(!retval, ERR_OUT_OF_MEMORY, NULL);
    list = (FileHandleList *) malloc(sizeof (FileHandleList));
    BAIL_IF_MACRO(!list, ERR_OUT_OF_MEMORY, NULL);

    for (i = searchPath; i != NULL; i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, fname))
        {
            rc = h->funcs->openRead(h, fname);
            if (rc != NULL)
                break;
        } /* if */
    } /* for */

    if (rc == NULL)
    {
        free(list);
        free(retval);
        retval = NULL;
    } /* if */
    else
    {
        retval->opaque = (void *) rc;
        list->handle = retval;
        list->next = openReadList;
        openReadList = list;
    } /* else */

    return(retval);
} /* PHYSFS_openRead */


int PHYSFS_close(PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    FileHandleList *i;
    FileHandleList *prev;
    FileHandleList **_lists[] = { &openWriteList, &openReadList, NULL };
    FileHandleList ***lists = _lists;  /* gay. */
    int rc;

    while (lists != NULL)
    {
        for (i = *(*lists), prev = NULL; i != NULL; prev = i, i = i->next)
        {
            if (i->handle == handle)
            {
                rc = h->funcs->close(h);
                if (!rc)
                    return(0);

                if (prev == NULL)
                    *(*lists) = i->next;
                else
                    prev->next = i->next;

                free(handle);
                free(i);
                return(1);
            } /* if */
        } /* for */
        lists++;
    } /* while */

    __PHYSFS_setError(ERR_NOT_A_HANDLE);
    return(0);
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

