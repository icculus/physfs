/**
 * PhysicsFS; a portable, flexible file i/o abstraction.
 *
 * Documentation is in physfs.h. It's verbose, honest.  :)
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined PHYSFS_PROFILING)
#include <sys/time.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct __PHYSFS_ERRMSGTYPE__
{
    PHYSFS_uint64 tid;
    int errorAvailable;
    char errorString[80];
    struct __PHYSFS_ERRMSGTYPE__ *next;
} ErrMsg;

typedef struct __PHYSFS_DIRINFO__
{
    char *dirName;
    DirHandle *dirHandle;
    struct __PHYSFS_DIRINFO__ *next;
} PhysDirInfo;

typedef struct __PHYSFS_FILEHANDLELIST__
{
    PHYSFS_file handle;
    struct __PHYSFS_FILEHANDLELIST__ *next;
} FileHandleList;


/* The various i/o drivers... */

#if (defined PHYSFS_SUPPORTS_ZIP)
extern const PHYSFS_ArchiveInfo   __PHYSFS_ArchiveInfo_ZIP;
extern const DirFunctions         __PHYSFS_DirFunctions_ZIP;
#endif

#if (defined PHYSFS_SUPPORTS_GRP)
extern const PHYSFS_ArchiveInfo   __PHYSFS_ArchiveInfo_GRP;
extern const DirFunctions         __PHYSFS_DirFunctions_GRP;
#endif

#if (defined PHYSFS_SUPPORTS_QPAK)
extern const PHYSFS_ArchiveInfo   __PHYSFS_ArchiveInfo_QPAK;
extern const DirFunctions         __PHYSFS_DirFunctions_QPAK;
#endif

#if (defined PHYSFS_SUPPORTS_HOG)
extern const PHYSFS_ArchiveInfo   __PHYSFS_ArchiveInfo_HOG;
extern const DirFunctions         __PHYSFS_DirFunctions_HOG;
#endif

#if (defined PHYSFS_SUPPORTS_MVL)
extern const PHYSFS_ArchiveInfo   __PHYSFS_ArchiveInfo_MVL;
extern const DirFunctions         __PHYSFS_DirFunctions_MVL;
#endif

#if (defined PHYSFS_SUPPORTS_WAD)
extern const PHYSFS_ArchiveInfo   __PHYSFS_ArchiveInfo_WAD;
extern const DirFunctions         __PHYSFS_DirFunctions_WAD;
#endif

extern const DirFunctions  __PHYSFS_DirFunctions_DIR;


static const PHYSFS_ArchiveInfo *supported_types[] =
{
#if (defined PHYSFS_SUPPORTS_ZIP)
    &__PHYSFS_ArchiveInfo_ZIP,
#endif

#if (defined PHYSFS_SUPPORTS_GRP)
    &__PHYSFS_ArchiveInfo_GRP,
#endif

#if (defined PHYSFS_SUPPORTS_QPAK)
    &__PHYSFS_ArchiveInfo_QPAK,
#endif

#if (defined PHYSFS_SUPPORTS_HOG)
    &__PHYSFS_ArchiveInfo_HOG,
#endif

#if (defined PHYSFS_SUPPORTS_MVL)
    &__PHYSFS_ArchiveInfo_MVL,
#endif

#if (defined PHYSFS_SUPPORTS_WAD)
    &__PHYSFS_ArchiveInfo_WAD,
#endif

    NULL
};

static const DirFunctions *dirFunctions[] =
{
    &__PHYSFS_DirFunctions_DIR,

#if (defined PHYSFS_SUPPORTS_ZIP)
    &__PHYSFS_DirFunctions_ZIP,
#endif

#if (defined PHYSFS_SUPPORTS_GRP)
    &__PHYSFS_DirFunctions_GRP,
#endif

#if (defined PHYSFS_SUPPORTS_QPAK)
    &__PHYSFS_DirFunctions_QPAK,
#endif

#if (defined PHYSFS_SUPPORTS_HOG)
    &__PHYSFS_DirFunctions_HOG,
#endif

#if (defined PHYSFS_SUPPORTS_MVL)
    &__PHYSFS_DirFunctions_MVL,
#endif

#if (defined PHYSFS_SUPPORTS_WAD)
    &__PHYSFS_DirFunctions_WAD,
#endif

    NULL
};



/* General PhysicsFS state ... */
static int initialized = 0;
static ErrMsg *errorMessages = NULL;
static PhysDirInfo *searchPath = NULL;
static PhysDirInfo *writeDir = NULL;
static FileHandleList *openWriteList = NULL;
static FileHandleList *openReadList = NULL;
static char *baseDir = NULL;
static char *userDir = NULL;
static int allowSymLinks = 0;

/* mutexes ... */
static void *errorLock = NULL;     /* protects error message list.        */
static void *stateLock = NULL;     /* protects other PhysFS static state. */


/* functions ... */

static void __PHYSFS_bubble_sort(void *a, PHYSFS_uint32 lo, PHYSFS_uint32 hi,
                         int (*cmpfn)(void *, PHYSFS_uint32, PHYSFS_uint32),
                         void (*swapfn)(void *, PHYSFS_uint32, PHYSFS_uint32))
{
    PHYSFS_uint32 i;
    int sorted;

    do
    {
        sorted = 1;
        for (i = lo; i < hi; i++)
        {
            if (cmpfn(a, i, i + 1) > 0)
            {
                swapfn(a, i, i + 1);
                sorted = 0;
            } /* if */
        } /* for */
    } while (!sorted);
} /* __PHYSFS_bubble_sort */


static void __PHYSFS_quick_sort(void *a, PHYSFS_uint32 lo, PHYSFS_uint32 hi,
                         int (*cmpfn)(void *, PHYSFS_uint32, PHYSFS_uint32),
                         void (*swapfn)(void *, PHYSFS_uint32, PHYSFS_uint32))
{
    PHYSFS_uint32 i;
    PHYSFS_uint32 j;
    PHYSFS_uint32 v;

    if ((hi - lo) <= PHYSFS_QUICKSORT_THRESHOLD)
        __PHYSFS_bubble_sort(a, lo, hi, cmpfn, swapfn);
    else
    {
        i = (hi + lo) / 2;

        if (cmpfn(a, lo, i) > 0) swapfn(a, lo, i);
        if (cmpfn(a, lo, hi) > 0) swapfn(a, lo, hi);
        if (cmpfn(a, i, hi) > 0) swapfn(a, i, hi);

        j = hi - 1;
        swapfn(a, i, j);
        i = lo;
        v = j;
        while (1)
        {
            while(cmpfn(a, ++i, v) < 0) { /* do nothing */ }
            while(cmpfn(a, --j, v) > 0) { /* do nothing */ }
            if (j < i)
                break;
            swapfn(a, i, j);
        } /* while */
        if (i != (hi-1))
            swapfn(a, i, hi-1);
        __PHYSFS_quick_sort(a, lo, j, cmpfn, swapfn);
        __PHYSFS_quick_sort(a, i+1, hi, cmpfn, swapfn);
    } /* else */
} /* __PHYSFS_quick_sort */


void __PHYSFS_sort(void *entries, PHYSFS_uint32 max,
                   int (*cmpfn)(void *, PHYSFS_uint32, PHYSFS_uint32),
                   void (*swapfn)(void *, PHYSFS_uint32, PHYSFS_uint32))
{
    /*
     * Quicksort w/ Bubblesort fallback algorithm inspired by code from here:
     *   http://www.cs.ubc.ca/spider/harrison/Java/sorting-demo.html
     */
    __PHYSFS_quick_sort(entries, 0, max - 1, cmpfn, swapfn);
} /* __PHYSFS_sort */



#if (defined PHYSFS_PROFILING)

#define PHYSFS_TEST_SORT_ITERATIONS 150
#define PHYSFS_TEST_SORT_ELEMENTS   (64 * 1024)

static int __PHYSFS_test_sort_cmp(void *_a, PHYSFS_uint32 x, PHYSFS_uint32 y)
{
    PHYSFS_sint32 *a = (PHYSFS_sint32 *) _a;
    PHYSFS_sint32 one = a[x];
    PHYSFS_sint32 two = a[y];

    if (one < two)
        return(-1);
    else if (one > two)
        return(1);

    return(0);
} /* __PHYSFS_test_sort_cmp */


static void __PHYSFS_test_sort_swap(void *_a, PHYSFS_uint32 x, PHYSFS_uint32 y)
{
    PHYSFS_sint32 *a = (PHYSFS_sint32 *) _a;
    PHYSFS_sint32 tmp;
    tmp = a[x];
    a[x] = a[y];
    a[y] = tmp;
} /* __PHYSFS_test_sort_swap */


static int __PHYSFS_test_sort_do(PHYSFS_uint32 *timer,
                         PHYSFS_sint32 *a, PHYSFS_uint32 max,
                         int (*cmpfn)(void *, PHYSFS_uint32, PHYSFS_uint32),
                         void (*swapfn)(void *, PHYSFS_uint32, PHYSFS_uint32))
{
    PHYSFS_uint32 i;
    struct timeval starttime, endtime;

    gettimeofday(&starttime, NULL);
    __PHYSFS_sort(a, max, cmpfn, swapfn);
    gettimeofday(&endtime, NULL);

    for (i = 1; i < max; i++)
    {
        if (a[i] < a[i - 1])
            return(0);
    } /* for */

    if (timer != NULL)
    {
        *timer = ( ((endtime.tv_sec  - starttime.tv_sec) * 1000) +
                   ((endtime.tv_usec - starttime.tv_usec) / 1000) );
    } /* if */

    return(1);
} /* __PHYSFS_test_sort_time */


static void __PHYSFS_test_sort(void)
{
    PHYSFS_uint32 elasped[PHYSFS_TEST_SORT_ITERATIONS];
    PHYSFS_sint32 iter;
    PHYSFS_sint32 a[PHYSFS_TEST_SORT_ELEMENTS];
    PHYSFS_sint32 i, x;
    int success;

    printf("Testing __PHYSFS_sort (linear presorted) ... ");
    for (iter = 0; iter < PHYSFS_TEST_SORT_ITERATIONS; iter++)
    {
        /* set up array to sort. */
        for (i = 0; i < PHYSFS_TEST_SORT_ELEMENTS; i++)
            a[i] = i;

        /* sort it. */
        success = __PHYSFS_test_sort_do(&elasped[iter],
                                        a, PHYSFS_TEST_SORT_ELEMENTS,
                                        __PHYSFS_test_sort_cmp,
                                        __PHYSFS_test_sort_swap);
        if (!success)
            break;
    } /* for */

    if (!success)
        printf("Failed!\n");
    else
    {
        for (x = 0, iter = 0; iter < PHYSFS_TEST_SORT_ITERATIONS; iter++)
            x += elasped[iter];

        x /= PHYSFS_TEST_SORT_ITERATIONS;
        printf("Average run (%lu) ms.\n", (unsigned long) x);
    } /* else */

    printf("Testing __PHYSFS_sort (linear presorted reverse) ... ");
    for (iter = 0; iter < PHYSFS_TEST_SORT_ITERATIONS; iter++)
    {
        /* set up array to sort. */
        for (i = 0, x = PHYSFS_TEST_SORT_ELEMENTS;
             i < PHYSFS_TEST_SORT_ELEMENTS;
             i++, x--)
        {
            a[i] = x;
        } /* for */

        /* sort it. */
        success = __PHYSFS_test_sort_do(&elasped[iter],
                                        a, PHYSFS_TEST_SORT_ELEMENTS,
                                        __PHYSFS_test_sort_cmp,
                                        __PHYSFS_test_sort_swap);
        if (!success)
            break;
    } /* for */

    if (!success)
        printf("Failed!\n");
    else
    {
        for (x = 0, iter = 0; iter < PHYSFS_TEST_SORT_ITERATIONS; iter++)
            x += elasped[iter];

        x /= PHYSFS_TEST_SORT_ITERATIONS;
        printf("Average run (%lu) ms.\n", (unsigned long) x);
    } /* else */


    printf("Testing __PHYSFS_sort (randomized) ... ");
    for (iter = 0; iter < PHYSFS_TEST_SORT_ITERATIONS; iter++)
    {
        /* set up array to sort. */
        for (i = 0; i < PHYSFS_TEST_SORT_ELEMENTS; i++)
            a[i] = (PHYSFS_uint32) rand();

        /* sort it. */
        success = __PHYSFS_test_sort_do(&elasped[iter],
                                        a, PHYSFS_TEST_SORT_ELEMENTS,
                                        __PHYSFS_test_sort_cmp,
                                        __PHYSFS_test_sort_swap);
        if (!success)
            break;
    } /* for */

    if (!success)
        printf("Failed!\n");
    else
    {
        for (x = 0, iter = 0; iter < PHYSFS_TEST_SORT_ITERATIONS; iter++)
            x += elasped[iter];

        x /= PHYSFS_TEST_SORT_ITERATIONS;
        printf("Average run (%lu) ms.\n", (unsigned long) x);
    } /* else */

    printf("__PHYSFS_test_sort() complete.\n\n");
} /* __PHYSFS_test_sort */
#endif


static ErrMsg *findErrorForCurrentThread(void)
{
    ErrMsg *i;
    PHYSFS_uint64 tid;

    if (errorLock != NULL)
        __PHYSFS_platformGrabMutex(errorLock);

    if (errorMessages != NULL)
    {
        tid = __PHYSFS_platformGetThreadID();

        for (i = errorMessages; i != NULL; i = i->next)
        {
            if (i->tid == tid)
            {
                if (errorLock != NULL)
                    __PHYSFS_platformReleaseMutex(errorLock);
                return(i);
            } /* if */
        } /* for */
    } /* if */

    if (errorLock != NULL)
        __PHYSFS_platformReleaseMutex(errorLock);

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

        memset((void *) err, '\0', sizeof (ErrMsg));
        err->tid = __PHYSFS_platformGetThreadID();

        if (errorLock != NULL)
            __PHYSFS_platformGrabMutex(errorLock);

        err->next = errorMessages;
        errorMessages = err;

        if (errorLock != NULL)
            __PHYSFS_platformReleaseMutex(errorLock);
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


/* MAKE SURE that errorLock is held before calling this! */
static void freeErrorMessages(void)
{
    ErrMsg *i;
    ErrMsg *next;

    for (i = errorMessages; i != NULL; i = next)
    {
        next = i->next;
        free(i);
    } /* for */

    errorMessages = NULL;
} /* freeErrorMessages */


void PHYSFS_getLinkedVersion(PHYSFS_Version *ver)
{
    if (ver != NULL)
    {
        ver->major = PHYSFS_VER_MAJOR;
        ver->minor = PHYSFS_VER_MINOR;
        ver->patch = PHYSFS_VER_PATCH;
    } /* if */
} /* PHYSFS_getLinkedVersion */


static const char *find_filename_extension(const char *fname)
{
    const char *retval = strchr(fname, '.');
    const char *p = retval;

    while (p != NULL)
    {
        p = strchr(p + 1, '.');
        if (p != NULL)
            retval = p;
    } /* while */

    if (retval != NULL)
        retval++;  /* skip '.' */

    return(retval);
} /* find_filename_extension */


static DirHandle *openDirectory(const char *d, int forWriting)
{
    const DirFunctions **i;
    const char *ext;

    BAIL_IF_MACRO(!__PHYSFS_platformExists(d), ERR_NO_SUCH_FILE, NULL);

    ext = find_filename_extension(d);
    if (ext != NULL)
    {
        /* Look for archivers with matching file extensions first... */
        for (i = dirFunctions; *i != NULL; i++)
        {
            if (__PHYSFS_platformStricmp(ext, (*i)->info->extension) == 0)
            {
                if ((*i)->isArchive(d, forWriting))
                    return( (*i)->openArchive(d, forWriting) );
            } /* if */
        } /* for */

        /* failing an exact file extension match, try all the others... */
        for (i = dirFunctions; *i != NULL; i++)
        {
            if (__PHYSFS_platformStricmp(ext, (*i)->info->extension) != 0)
            {
                if ((*i)->isArchive(d, forWriting))
                    return( (*i)->openArchive(d, forWriting) );
            } /* if */
        } /* for */
    } /* if */

    else  /* no extension? Try them all. */
    {
        for (i = dirFunctions; *i != NULL; i++)
        {
            if ((*i)->isArchive(d, forWriting))
                return( (*i)->openArchive(d, forWriting) );
        } /* for */
    } /* else */

    __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
    return(NULL);
} /* openDirectory */


static PhysDirInfo *buildDirInfo(const char *newDir, int forWriting)
{
    DirHandle *dirHandle = NULL;
    PhysDirInfo *di = NULL;

    BAIL_IF_MACRO(newDir == NULL, ERR_INVALID_ARGUMENT, 0);

    dirHandle = openDirectory(newDir, forWriting);
    BAIL_IF_MACRO(dirHandle == NULL, NULL, 0);

    di = (PhysDirInfo *) malloc(sizeof (PhysDirInfo));
    if (di == NULL)
    {
        dirHandle->funcs->dirClose(dirHandle);
        BAIL_IF_MACRO(di == NULL, ERR_OUT_OF_MEMORY, 0);
    } /* if */

    di->dirName = (char *) malloc(strlen(newDir) + 1);
    if (di->dirName == NULL)
    {
        free(di);
        dirHandle->funcs->dirClose(dirHandle);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, 0);
    } /* if */

    di->next = NULL;
    di->dirHandle = dirHandle;
    strcpy(di->dirName, newDir);
    return(di);
} /* buildDirInfo */


/* MAKE SURE you've got the stateLock held before calling this! */
static int freeDirInfo(PhysDirInfo *di, FileHandleList *openList)
{
    FileHandleList *i;

    if (di == NULL)
        return(1);

    for (i = openList; i != NULL; i = i->next)
    {
        const DirHandle *h = ((FileHandle *) &(i->handle.opaque))->dirHandle;
        BAIL_IF_MACRO(h == di->dirHandle, ERR_FILES_STILL_OPEN, 0);
    } /* for */
    
    di->dirHandle->funcs->dirClose(di->dirHandle);
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
                                 strlen(dirsep) + 6);

        if (retval == NULL)
            __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        else
            sprintf(retval, "%susers%s%s", baseDir, dirsep, str);

        if (uname != NULL)
            free((void *) uname);
    } /* else */

    return(retval);
} /* calculateUserDir */


static int appendDirSep(char **dir)
{
    const char *dirsep = PHYSFS_getDirSeparator();
    char *ptr;

    if (strcmp((*dir + strlen(*dir)) - strlen(dirsep), dirsep) == 0)
        return(1);

    ptr = realloc(*dir, strlen(*dir) + strlen(dirsep) + 1);
    if (!ptr)
    {
        free(*dir);
        return(0);
    } /* if */

    strcat(ptr, dirsep);
    *dir = ptr;
    return(1);
} /* appendDirSep */


static char *calculateBaseDir(const char *argv0)
{
    const char *dirsep = PHYSFS_getDirSeparator();
    char *retval;
    char *ptr;

    /*
     * See if the platform driver wants to handle this for us...
     */
    retval = __PHYSFS_platformCalcBaseDir(argv0);
    if (retval != NULL)
        return(retval);

    /*
     * Determine if there's a path on argv0. If there is, that's the base dir.
     */
    ptr = strstr(argv0, dirsep);
    if (ptr != NULL)
    {
        char *p = ptr;
        size_t size;
        while (p != NULL)
        {
            ptr = p;
            p = strstr(p + 1, dirsep);
        } /* while */

        size = (size_t) (ptr - argv0);
        retval = (char *) malloc(size + 1);
        BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
        memcpy(retval, argv0, size);
        retval[size] = '\0';
        return(retval);
    } /* if */

    /*
     * Last ditch effort: it's the current working directory. (*shrug*)
     */
    retval = __PHYSFS_platformCurrentDir();
    if(retval != NULL) {
    return(retval);
    }

    /*
     * Ok, current directory doesn't exist, use the root directory.
     * Not a good alternative, but it only happens if the current
     * directory was deleted from under the program.
     */
    retval = (char *) malloc(strlen(dirsep) + 1);
    strcpy(retval, dirsep);
    return(retval);
} /* calculateBaseDir */


static int initializeMutexes(void)
{
    errorLock = __PHYSFS_platformCreateMutex();
    if (errorLock == NULL)
        goto initializeMutexes_failed;

    stateLock = __PHYSFS_platformCreateMutex();
    if (stateLock == NULL)
        goto initializeMutexes_failed;

    return(1);  /* success. */

initializeMutexes_failed:
    if (errorLock != NULL)
        __PHYSFS_platformDestroyMutex(errorLock);

    if (stateLock != NULL)
        __PHYSFS_platformDestroyMutex(stateLock);

    errorLock = stateLock = NULL;
    return(0);  /* failed. */
} /* initializeMutexes */


int PHYSFS_init(const char *argv0)
{
    char *ptr;

    BAIL_IF_MACRO(initialized, ERR_IS_INITIALIZED, 0);
    BAIL_IF_MACRO(!__PHYSFS_platformInit(), NULL, 0);

    BAIL_IF_MACRO(!initializeMutexes(), NULL, 0);

    baseDir = calculateBaseDir(argv0);
    BAIL_IF_MACRO(baseDir == NULL, NULL, 0);

    ptr = __PHYSFS_platformRealPath(baseDir);
    free(baseDir);
    BAIL_IF_MACRO(ptr == NULL, NULL, 0);
    baseDir = ptr;

    BAIL_IF_MACRO(!appendDirSep(&baseDir), NULL, 0);

    userDir = calculateUserDir();
    if (userDir != NULL)
    {
        ptr = __PHYSFS_platformRealPath(userDir);
        free(userDir);
        userDir = ptr;
    } /* if */

    if ((userDir == NULL) || (!appendDirSep(&userDir)))
    {
        free(baseDir);
        baseDir = NULL;
        return(0);
    } /* if */

    initialized = 1;

    /* This makes sure that the error subsystem is initialized. */
    __PHYSFS_setError(PHYSFS_getLastError());

#if (defined PHYSFS_PROFILING)
    srand(time(NULL));
    setbuf(stdout, NULL);
    printf("\n");
    printf("********************************************************\n");
    printf("Warning! Profiling is built into this copy of PhysicsFS!\n");
    printf("********************************************************\n");
    printf("\n");
    printf("\n");
    __PHYSFS_test_sort();
#endif

    return(1);
} /* PHYSFS_init */


/* MAKE SURE you hold stateLock before calling this! */
static int closeFileHandleList(FileHandleList **list)
{
    FileHandleList *i;
    FileHandleList *next = NULL;
    FileHandle *h;

    for (i = *list; i != NULL; i = next)
    {
        next = i->next;
        h = (FileHandle *) (i->handle.opaque);
        if (!h->funcs->fileClose(h))
        {
            *list = i;
            return(0);
        } /* if */

        free(i);
    } /* for */

    *list = NULL;
    return(1);
} /* closeFileHandleList */


/* MAKE SURE you hold the stateLock before calling this! */
static void freeSearchPath(void)
{
    PhysDirInfo *i;
    PhysDirInfo *next = NULL;

    closeFileHandleList(&openReadList);

    if (searchPath != NULL)
    {
        for (i = searchPath; i != NULL; i = next)
        {
            next = i->next;
            freeDirInfo(i, openReadList);
        } /* for */
        searchPath = NULL;
    } /* if */
} /* freeSearchPath */


int PHYSFS_deinit(void)
{
    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);
    BAIL_IF_MACRO(!__PHYSFS_platformDeinit(), NULL, 0);

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

    __PHYSFS_platformDestroyMutex(errorLock);
    __PHYSFS_platformDestroyMutex(stateLock);

    errorLock = stateLock = NULL;
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
    const char *retval = NULL;

    __PHYSFS_platformGrabMutex(stateLock);
    if (writeDir != NULL)
        retval = writeDir->dirName;
    __PHYSFS_platformReleaseMutex(stateLock);

    return(retval);
} /* PHYSFS_getWriteDir */


int PHYSFS_setWriteDir(const char *newDir)
{
    int retval = 1;

    __PHYSFS_platformGrabMutex(stateLock);

    if (writeDir != NULL)
    {
        BAIL_IF_MACRO_MUTEX(!freeDirInfo(writeDir, openWriteList), NULL, 
                            stateLock, 0);
        writeDir = NULL;
    } /* if */

    if (newDir != NULL)
    {
        writeDir = buildDirInfo(newDir, 1);
        retval = (writeDir != NULL);
    } /* if */

    __PHYSFS_platformReleaseMutex(stateLock);

    return(retval);
} /* PHYSFS_setWriteDir */


int PHYSFS_addToSearchPath(const char *newDir, int appendToPath)
{
    PhysDirInfo *di;
    PhysDirInfo *prev = NULL;
    PhysDirInfo *i;

    __PHYSFS_platformGrabMutex(stateLock);

    for (i = searchPath; i != NULL; i = i->next)
    {
        /* already in search path? */
        BAIL_IF_MACRO_MUTEX(strcmp(newDir, i->dirName)==0, NULL, stateLock, 1);
        prev = i;
    } /* for */

    di = buildDirInfo(newDir, 0);
    BAIL_IF_MACRO_MUTEX(di == NULL, NULL, stateLock, 0);

    if (appendToPath)
    {
        di->next = NULL;
        if (prev == NULL)
            searchPath = di;
        else
            prev->next = di;
    } /* if */
    else
    {
        di->next = searchPath;
        searchPath = di;
    } /* else */

    __PHYSFS_platformReleaseMutex(stateLock);
    return(1);
} /* PHYSFS_addToSearchPath */


int PHYSFS_removeFromSearchPath(const char *oldDir)
{
    PhysDirInfo *i;
    PhysDirInfo *prev = NULL;
    PhysDirInfo *next = NULL;

    BAIL_IF_MACRO(oldDir == NULL, ERR_INVALID_ARGUMENT, 0);

    __PHYSFS_platformGrabMutex(stateLock);
    for (i = searchPath; i != NULL; i = i->next)
    {
        if (strcmp(i->dirName, oldDir) == 0)
        {
            next = i->next;
            BAIL_IF_MACRO_MUTEX(!freeDirInfo(i, openReadList), NULL,
                                stateLock, 0);

            if (prev == NULL)
                searchPath = next;
            else
                prev->next = next;

            BAIL_MACRO_MUTEX(NULL, stateLock, 1);
        } /* if */
        prev = i;
    } /* for */

    BAIL_MACRO_MUTEX(ERR_NOT_IN_SEARCH_PATH, stateLock, 0);
} /* PHYSFS_removeFromSearchPath */


char **PHYSFS_getSearchPath(void)
{
    int count = 1;
    int x;
    PhysDirInfo *i;
    char **retval;

    __PHYSFS_platformGrabMutex(stateLock);

    for (i = searchPath; i != NULL; i = i->next)
        count++;

    retval = (char **) malloc(sizeof (char *) * count);
    BAIL_IF_MACRO_MUTEX(!retval, ERR_OUT_OF_MEMORY, stateLock, NULL);
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
            BAIL_MACRO_MUTEX(ERR_OUT_OF_MEMORY, stateLock, NULL);
        } /* if */

        strcpy(retval[x], i->dirName);
    } /* for */

    __PHYSFS_platformReleaseMutex(stateLock);
    return(retval);
} /* PHYSFS_getSearchPath */


int PHYSFS_setSaneConfig(const char *organization, const char *appName,
                         const char *archiveExt, int includeCdRoms,
                         int archivesFirst)
{
    const char *basedir = PHYSFS_getBaseDir();
    const char *userdir = PHYSFS_getUserDir();
    const char *dirsep = PHYSFS_getDirSeparator();
    char *str;

    BAIL_IF_MACRO(!initialized, ERR_NOT_INITIALIZED, 0);

        /* set write dir... */
    str = malloc(strlen(userdir) + (strlen(organization) * 2) +
                 (strlen(appName) * 2) + (strlen(dirsep) * 3) + 2);
    BAIL_IF_MACRO(str == NULL, ERR_OUT_OF_MEMORY, 0);
    sprintf(str, "%s.%s%s%s", userdir, organization, dirsep, appName);

    if (!PHYSFS_setWriteDir(str))
    {
        int no_write = 0;
        sprintf(str, ".%s/%s", organization, appName);
        if ( (PHYSFS_setWriteDir(userdir)) &&
             (PHYSFS_mkdir(str)) )
        {
            sprintf(str, "%s.%s%s%s", userdir, organization, dirsep, appName);
            if (!PHYSFS_setWriteDir(str))
                no_write = 1;
        } /* if */
        else
        {
            no_write = 1;
        } /* else */

        if (no_write)
        {
            PHYSFS_setWriteDir(NULL);   /* just in case. */
            free(str);
            BAIL_MACRO(ERR_CANT_SET_WRITE_DIR, 0);
        } /* if */
    } /* if */

    /* Put write dir first in search path... */
    PHYSFS_addToSearchPath(str, 0);
    free(str);

        /* Put base path on search path... */
    PHYSFS_addToSearchPath(basedir, 1);

        /* handle CD-ROMs... */
    if (includeCdRoms)
    {
        char **cds = PHYSFS_getCdRomDirs();
        char **i;
        for (i = cds; *i != NULL; i++)
            PHYSFS_addToSearchPath(*i, 1);

        PHYSFS_freeList(cds);
    } /* if */

        /* Root out archives, and add them to search path... */
    if (archiveExt != NULL)
    {
        char **rc = PHYSFS_enumerateFiles("/");
        char **i;
        size_t extlen = strlen(archiveExt);
        char *ext;

        for (i = rc; *i != NULL; i++)
        {
            size_t l = strlen(*i);
            if ((l > extlen) && ((*i)[l - extlen - 1] == '.'))
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
char * __PHYSFS_convertToDependent(const char *prepend,
                                              const char *dirName,
                                              const char *append)
{
    const char *dirsep = __PHYSFS_platformDirSeparator;
    size_t sepsize = strlen(dirsep);
    char *str;
    char *i1;
    char *i2;
    size_t allocSize;

    while (*dirName == '/')
        dirName++;

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

    if (prepend == NULL)
        *str = '\0';
    else
    {
        strcpy(str, prepend);
        strcat(str, dirsep);
    } /* else */

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
        strcat(str, append);
    } /* if */

    return(str);
} /* __PHYSFS_convertToDependent */


int __PHYSFS_verifySecurity(DirHandle *h, const char *fname, int allowMissing)
{
    int retval = 1;
    char *start;
    char *end;
    char *str;

    if (*fname == '\0')  /* quick rejection. */
        return(1);

    /* !!! FIXME: Can we ditch this malloc()? */
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
             (strchr(start, '\\') != NULL) ||
             (strchr(start, ':') != NULL) )
        {
            __PHYSFS_setError(ERR_INSECURE_FNAME);
            retval = 0;
            break;
        } /* if */

        if (!allowSymLinks)
        {
            if (h->funcs->isSymLink(h, str, &retval))
            {
                __PHYSFS_setError(ERR_SYMLINK_DISALLOWED);
                free(str);
                return(0); /* insecure. */
            } /* if */

            /* break out early if path element is missing. */
            if (!retval)
            {
                /*
                 * We need to clear it if it's the last element of the path,
                 *  since this might be a non-existant file we're opening
                 *  for writing...
                 */
                if ((end == NULL) || (allowMissing))
                    retval = 1;
                break;
            } /* if */
        } /* if */

        if (end == NULL)
            break;

        *end = '/';
        start = end + 1;
    } /* while */

    free(str);
    return(retval);
} /* __PHYSFS_verifySecurity */


int PHYSFS_mkdir(const char *dname)
{
    DirHandle *h;
    char *str;
    char *start;
    char *end;
    int retval = 0;
    int exists = 1;  /* force existance check on first path element. */

    BAIL_IF_MACRO(dname == NULL, ERR_INVALID_ARGUMENT, 0);
    while (*dname == '/')
        dname++;

    __PHYSFS_platformGrabMutex(stateLock);
    BAIL_IF_MACRO_MUTEX(writeDir == NULL, ERR_NO_WRITE_DIR, stateLock, 0);
    h = writeDir->dirHandle;
    BAIL_IF_MACRO_MUTEX(!__PHYSFS_verifySecurity(h,dname,1),NULL,stateLock,0);
    start = str = malloc(strlen(dname) + 1);
    BAIL_IF_MACRO_MUTEX(str == NULL, ERR_OUT_OF_MEMORY, stateLock, 0);
    strcpy(str, dname);

    while (1)
    {
        end = strchr(start, '/');
        if (end != NULL)
            *end = '\0';

        /* only check for existance if all parent dirs existed, too... */
        if (exists)
            retval = h->funcs->isDirectory(h, str, &exists);

        if (!exists)
            retval = h->funcs->mkdir(h, str);

        if (!retval)
            break;

        if (end == NULL)
            break;

        *end = '/';
        start = end + 1;
    } /* while */

    __PHYSFS_platformReleaseMutex(stateLock);

    free(str);
    return(retval);
} /* PHYSFS_mkdir */


int PHYSFS_delete(const char *fname)
{
    int retval;
    DirHandle *h;

    BAIL_IF_MACRO(fname == NULL, ERR_INVALID_ARGUMENT, 0);
    while (*fname == '/')
        fname++;

    __PHYSFS_platformGrabMutex(stateLock);

    BAIL_IF_MACRO_MUTEX(writeDir == NULL, ERR_NO_WRITE_DIR, stateLock, 0);
    h = writeDir->dirHandle;
    BAIL_IF_MACRO_MUTEX(!__PHYSFS_verifySecurity(h,fname,0),NULL,stateLock,0);
    retval = h->funcs->remove(h, fname);

    __PHYSFS_platformReleaseMutex(stateLock);
    return(retval);
} /* PHYSFS_delete */


const char *PHYSFS_getRealDir(const char *filename)
{
    PhysDirInfo *i;
    const char *retval = NULL;

    while (*filename == '/')
        filename++;

    __PHYSFS_platformGrabMutex(stateLock);
    for (i = searchPath; ((i != NULL) && (retval == NULL)); i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, filename, 0))
        {
            if (h->funcs->exists(h, filename))
                retval = i->dirName;
        } /* if */
    } /* for */
    __PHYSFS_platformReleaseMutex(stateLock);

    return(retval);
} /* PHYSFS_getRealDir */


static int countList(LinkedStringList *list)
{
    int retval = 0;
    LinkedStringList *i;

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

    if (retval != NULL)
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
        if (rc > 0)  /* insertion point. */
            break;
        else if (rc == 0)      /* already in list. */
        {
            free(item->str);
            free(item);
            return;
        } /* else if */
        prev = i;
    } /* for */

        /*
         * If we are here, we are either at the insertion point.
         *  This may be the end of the list, or the list may be empty, too.
         */
    if (prev == NULL)
        *final = item;
    else
        prev->next = item;

    item->next = i;
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
    PhysDirInfo *i;
    char **retval = NULL;
    LinkedStringList *rc;
    LinkedStringList *finalList = NULL;
    int omitSymLinks = !allowSymLinks;

    BAIL_IF_MACRO(path == NULL, ERR_INVALID_ARGUMENT, NULL);
    while (*path == '/')
        path++;

    __PHYSFS_platformGrabMutex(stateLock);
    for (i = searchPath; i != NULL; i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, path, 0))
        {
            rc = h->funcs->enumerateFiles(h, path, omitSymLinks);
            interpolateStringLists(&finalList, rc);
        } /* if */
    } /* for */
    __PHYSFS_platformReleaseMutex(stateLock);

    retval = convertStringListToPhysFSList(finalList);
    return(retval);
} /* PHYSFS_enumerateFiles */


int PHYSFS_exists(const char *fname)
{
    BAIL_IF_MACRO(fname == NULL, ERR_INVALID_ARGUMENT, 0);
    while (*fname == '/')
        fname++;

    return(PHYSFS_getRealDir(fname) != NULL);
} /* PHYSFS_exists */


PHYSFS_sint64 PHYSFS_getLastModTime(const char *fname)
{
    PhysDirInfo *i;
    PHYSFS_sint64 retval = -1;
    int fileExists = 0;

    BAIL_IF_MACRO(fname == NULL, ERR_INVALID_ARGUMENT, 0);
    while (*fname == '/')
        fname++;

    if (*fname == '\0')   /* eh...punt if it's the root dir. */
        return(1);

    __PHYSFS_platformGrabMutex(stateLock);
    for (i = searchPath; ((i != NULL) && (!fileExists)); i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, fname, 0))
            retval = h->funcs->getLastModTime(h, fname, &fileExists);
    } /* for */
    __PHYSFS_platformReleaseMutex(stateLock);

    return(retval);
} /* PHYSFS_getLastModTime */


int PHYSFS_isDirectory(const char *fname)
{
    PhysDirInfo *i;
    int retval = 0;
    int fileExists = 0;

    BAIL_IF_MACRO(fname == NULL, ERR_INVALID_ARGUMENT, 0);
    while (*fname == '/')
        fname++;

    BAIL_IF_MACRO(*fname == '\0', NULL, 1); /* Root is always a dir.  :) */

    __PHYSFS_platformGrabMutex(stateLock);
    for (i = searchPath; ((i != NULL) && (!fileExists)); i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, fname, 0))
            retval = h->funcs->isDirectory(h, fname, &fileExists);
    } /* for */
    __PHYSFS_platformReleaseMutex(stateLock);

    return(retval);
} /* PHYSFS_isDirectory */


int PHYSFS_isSymbolicLink(const char *fname)
{
    PhysDirInfo *i;
    int retval = 0;
    int fileExists = 0;

    BAIL_IF_MACRO(!allowSymLinks, ERR_SYMLINK_DISALLOWED, 0);

    BAIL_IF_MACRO(fname == NULL, ERR_INVALID_ARGUMENT, 0);
    while (*fname == '/')
        fname++;

    BAIL_IF_MACRO(*fname == '\0', NULL, 0);   /* Root is never a symlink */

    __PHYSFS_platformGrabMutex(stateLock);
    for (i = searchPath; ((i != NULL) && (!fileExists)); i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, fname, 0))
            retval = h->funcs->isSymLink(h, fname, &fileExists);
    } /* for */
    __PHYSFS_platformReleaseMutex(stateLock);

    return(retval);
} /* PHYSFS_isSymbolicLink */


static PHYSFS_file *doOpenWrite(const char *fname, int appending)
{
    PHYSFS_file *retval = NULL;
    FileHandle *rc = NULL;
    DirHandle *h;
    const DirFunctions *f;
    FileHandleList *list;

    BAIL_IF_MACRO(fname == NULL, ERR_INVALID_ARGUMENT, NULL);
    while (*fname == '/')
        fname++;

    __PHYSFS_platformGrabMutex(stateLock);
    h = (writeDir == NULL) ? NULL : writeDir->dirHandle;
    BAIL_IF_MACRO_MUTEX(!h, ERR_NO_WRITE_DIR, stateLock, NULL);
    BAIL_IF_MACRO_MUTEX(!__PHYSFS_verifySecurity(h, fname, 0), NULL,
                        stateLock, NULL);

    list = (FileHandleList *) malloc(sizeof (FileHandleList));
    BAIL_IF_MACRO_MUTEX(!list, ERR_OUT_OF_MEMORY, stateLock, NULL);

    f = h->funcs;
    rc = (appending) ? f->openAppend(h, fname) : f->openWrite(h, fname);
    if (rc == NULL)
        free(list);
    else
    {
        rc->buffer = NULL;  /* just in case. */
        rc->buffill = rc->bufpos = rc->bufsize = 0;  /* just in case. */
        rc->forReading = 0;
        list->handle.opaque = (void *) rc;
        list->next = openWriteList;
        openWriteList = list;
        retval = &(list->handle);
    } /* else */

    __PHYSFS_platformReleaseMutex(stateLock);
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
    PHYSFS_file *retval = NULL;
    FileHandle *rc = NULL;
    FileHandleList *list;
    int fileExists = 0;
    PhysDirInfo *i;

    BAIL_IF_MACRO(fname == NULL, ERR_INVALID_ARGUMENT, NULL);
    while (*fname == '/')
        fname++;

    __PHYSFS_platformGrabMutex(stateLock);
    BAIL_IF_MACRO_MUTEX(!searchPath, ERR_NOT_IN_SEARCH_PATH, stateLock, NULL);
    for (i = searchPath; ((i != NULL) && (!fileExists)); i = i->next)
    {
        DirHandle *h = i->dirHandle;
        if (__PHYSFS_verifySecurity(h, fname, 0))
            rc = h->funcs->openRead(h, fname, &fileExists);
    } /* for */
    BAIL_IF_MACRO_MUTEX(rc == NULL, NULL, stateLock, NULL);

    list = (FileHandleList *) malloc(sizeof (FileHandleList));
    BAIL_IF_MACRO_MUTEX(!list, ERR_OUT_OF_MEMORY, stateLock, NULL);
    list->handle.opaque = (void *) rc;
    list->next = openReadList;
    openReadList = list;
    retval = &(list->handle);
    __PHYSFS_platformReleaseMutex(stateLock);

    rc->buffer = NULL;  /* just in case. */
    rc->buffill = rc->bufpos = rc->bufsize = 0;  /* just in case. */
    rc->forReading = 1;

    return(retval);
} /* PHYSFS_openRead */


static int closeHandleInOpenList(FileHandleList **list, PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    FileHandleList *prev = NULL;
    FileHandleList *i;
    int rc = 1;

    for (i = *list; i != NULL; i = i->next)
    {
        if (&i->handle == handle)  /* handle is in this list? */
        {
            PHYSFS_uint8 *tmp = h->buffer;
            rc = PHYSFS_flush(handle);
            if (rc)
                rc = h->funcs->fileClose(h);
            if (!rc)
                return(-1);

            if (tmp != NULL)  /* free any associated buffer. */
                free(tmp);

            if (prev == NULL)
                *list = i->next;
            else
                prev->next = i->next;

            free(i);
            return(1);
        } /* if */
        prev = i;
    } /* for */

    return(0);
} /* closeHandleInOpenList */


int PHYSFS_close(PHYSFS_file *handle)
{
    int rc;

    __PHYSFS_platformGrabMutex(stateLock);

    /* -1 == close failure. 0 == not found. 1 == success. */
    rc = closeHandleInOpenList(&openReadList, handle);
    BAIL_IF_MACRO_MUTEX(rc == -1, NULL, stateLock, 0);
    if (!rc)
    {
        rc = closeHandleInOpenList(&openWriteList, handle);
        BAIL_IF_MACRO_MUTEX(rc == -1, NULL, stateLock, 0);
    } /* if */

    __PHYSFS_platformReleaseMutex(stateLock);
    BAIL_IF_MACRO(!rc, ERR_NOT_A_HANDLE, 0);
    return(1);
} /* PHYSFS_close */


static PHYSFS_sint64 doBufferedRead(PHYSFS_file *handle, void *buffer,
                                    PHYSFS_uint32 objSize,
                                    PHYSFS_uint32 objCount)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    PHYSFS_sint64 retval = 0;
    PHYSFS_uint32 remainder = 0;

    while (objCount > 0)
    {
        PHYSFS_uint32 buffered = h->buffill - h->bufpos;
        PHYSFS_uint64 mustread = (objSize * objCount) - remainder;
        PHYSFS_uint32 copied;

        if (buffered == 0) /* need to refill buffer? */
        {
            PHYSFS_sint64 rc = h->funcs->read(h, h->buffer, 1, h->bufsize);
            if (rc <= 0)
            {
                h->bufpos -= remainder;
                return(((rc == -1) && (retval == 0)) ? -1 : retval);
            } /* if */

            buffered = h->buffill = (PHYSFS_uint32) rc;
            h->bufpos = 0;
        } /* if */

        if (buffered > mustread)
            buffered = (PHYSFS_uint32) mustread;

        memcpy(buffer, h->buffer + h->bufpos, (size_t) buffered);
        buffer = ((PHYSFS_uint8 *) buffer) + buffered;
        h->bufpos += buffered;
        buffered += remainder;  /* take remainder into account. */
        copied = (buffered / objSize);
        remainder = (buffered % objSize);
        retval += copied;
        objCount -= copied;
    } /* while */

    return(retval);
} /* doBufferedRead */


PHYSFS_sint64 PHYSFS_read(PHYSFS_file *handle, void *buffer,
                          PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    FileHandle *h = (FileHandle *) handle->opaque;

    BAIL_IF_MACRO(!h->forReading, ERR_FILE_ALREADY_OPEN_W, -1);
    BAIL_IF_MACRO(objSize == 0, NULL, 0);
    BAIL_IF_MACRO(objCount == 0, NULL, 0);
    if (h->buffer != NULL)
        return(doBufferedRead(handle, buffer, objSize, objCount));

    return(h->funcs->read(h, buffer, objSize, objCount));
} /* PHYSFS_read */


static PHYSFS_sint64 doBufferedWrite(PHYSFS_file *handle, const void *buffer,
                                     PHYSFS_uint32 objSize,
                                     PHYSFS_uint32 objCount)
{
    FileHandle *h = (FileHandle *) handle->opaque;

    /* whole thing fits in the buffer? */
    if (h->buffill + (objSize * objCount) < h->bufsize)
    {
        memcpy(h->buffer + h->buffill, buffer, objSize * objCount);
        h->buffill += (objSize * objCount);
        return(objCount);
    } /* if */

    /* would overflow buffer. Flush and then write the new objects, too. */
    BAIL_IF_MACRO(!PHYSFS_flush(handle), NULL, -1);
    return(h->funcs->write(h, buffer, objSize, objCount));
} /* doBufferedWrite */


PHYSFS_sint64 PHYSFS_write(PHYSFS_file *handle, const void *buffer,
                           PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    FileHandle *h = (FileHandle *) handle->opaque;

    BAIL_IF_MACRO(h->forReading, ERR_FILE_ALREADY_OPEN_R, -1);
    BAIL_IF_MACRO(objSize == 0, NULL, 0);
    BAIL_IF_MACRO(objCount == 0, NULL, 0);
    if (h->buffer != NULL)
        return(doBufferedWrite(handle, buffer, objSize, objCount));

    return(h->funcs->write(h, buffer, objSize, objCount));
} /* PHYSFS_write */


int PHYSFS_eof(PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;

    if (!h->forReading)  /* never EOF on files opened for write/append. */
        return(0);

    /* eof if buffer is empty and archiver says so. */
    return((h->bufpos == h->buffill) && (h->funcs->eof(h)));
} /* PHYSFS_eof */


PHYSFS_sint64 PHYSFS_tell(PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    PHYSFS_sint64 retval = h->forReading ?
                            (h->funcs->tell(h) - h->buffill) + h->bufpos :
                            (h->funcs->tell(h) + h->buffill);
    return(retval);
} /* PHYSFS_tell */


int PHYSFS_seek(PHYSFS_file *handle, PHYSFS_uint64 pos)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    BAIL_IF_MACRO(!PHYSFS_flush(handle), NULL, 0);

    if (h->buffer && h->forReading)
    {
        /* avoid throwing away our precious buffer if seeking within it. */
        PHYSFS_sint64 offset = pos - PHYSFS_tell(handle);
        if ( /* seeking within the already-buffered range? */
            ((offset >= 0) && (offset <= h->buffill - h->bufpos)) /* forwards */
            || ((offset < 0) && (-offset <= h->bufpos)) /* backwards */ )
        {
            h->bufpos += (PHYSFS_uint32) offset;
            return(1); /* successful seek */
        } /* if */
    } /* if */

    /* we have to fall back to a 'raw' seek. */
    h->buffill = h->bufpos = 0;
    return(h->funcs->seek(h, pos));
} /* PHYSFS_seek */


PHYSFS_sint64 PHYSFS_fileLength(PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    return(h->funcs->fileLength(h));
} /* PHYSFS_filelength */


int PHYSFS_setBuffer(PHYSFS_file *handle, PHYSFS_uint64 _bufsize)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    PHYSFS_uint32 bufsize = (PHYSFS_uint32) _bufsize;

    BAIL_IF_MACRO(_bufsize > 0xFFFFFFFF, "buffer must fit in 32-bits", 0);
    BAIL_IF_MACRO(!PHYSFS_flush(handle), NULL, 0);

    /*
     * For reads, we need to move the file pointer to where it would be
     *  if we weren't buffering, so that the next read will get the
     *  right chunk of stuff from the file. PHYSFS_flush() handles writes.
     */
    if ((h->forReading) && (h->buffill != h->bufpos))
    {
        PHYSFS_uint64 pos;
        PHYSFS_sint64 curpos = h->funcs->tell(h);
        BAIL_IF_MACRO(curpos == -1, NULL, 0);
        pos = ((curpos - h->buffill) + h->bufpos);
        BAIL_IF_MACRO(!h->funcs->seek(h, pos), NULL, 0);
    } /* if */

    if (bufsize == 0)  /* delete existing buffer. */
    {
        if (h->buffer != NULL)
        {
            free(h->buffer);
            h->buffer = NULL;
        } /* if */
    } /* if */

    else
    {
        PHYSFS_uint8 *newbuf = realloc(h->buffer, bufsize);
        BAIL_IF_MACRO(newbuf == NULL, ERR_OUT_OF_MEMORY, 0);
        h->buffer = newbuf;
    } /* else */

    h->bufsize = bufsize;
    h->buffill = h->bufpos = 0;
    return(1);
} /* PHYSFS_setBuffer */


int PHYSFS_flush(PHYSFS_file *handle)
{
    FileHandle *h = (FileHandle *) handle->opaque;
    PHYSFS_sint64 rc;

    if ((h->forReading) || (h->bufpos == h->buffill))
        return(1);  /* open for read or buffer empty are successful no-ops. */

    /* dump buffer to disk. */
    rc = h->funcs->write(h, h->buffer + h->bufpos, h->buffill - h->bufpos, 1);
    BAIL_IF_MACRO(rc <= 0, NULL, 0);
    h->bufpos = h->buffill = 0;
    return(1);
} /* PHYSFS_flush */


LinkedStringList *__PHYSFS_addToLinkedStringList(LinkedStringList *retval,
                                                 LinkedStringList **prev,
                                                 const char *str,
                                                 PHYSFS_sint32 len)
{
    LinkedStringList *l;

    l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
    BAIL_IF_MACRO(l == NULL, ERR_OUT_OF_MEMORY, retval);

    if (len < 0)
        len = strlen(str);

    l->str = (char *) malloc(len + 1);
    if (l->str == NULL)
    {
        free(l);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, retval);
    } /* if */

    strncpy(l->str, str, len);
    l->str[len] = '\0';

    if (retval == NULL)
        retval = l;
    else
        (*prev)->next = l;

    *prev = l;
    l->next = NULL;
    return(retval);
} /* __PHYSFS_addToLinkedStringList */

/* end of physfs.c ... */

