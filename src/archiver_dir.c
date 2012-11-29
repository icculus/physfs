/*
 * Standard directory I/O support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

/* There's no PHYSFS_Io interface here. Use __PHYSFS_createNativeIo(). */



static char *cvtToDependent(const char *prepend, const char *path, char *buf)
{
    BAIL_IF_MACRO(buf == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    sprintf(buf, "%s%s", prepend ? prepend : "", path);

    if (__PHYSFS_platformDirSeparator != '/')
    {
        char *p;
        for (p = strchr(buf, '/'); p != NULL; p = strchr(p + 1, '/'))
            *p = __PHYSFS_platformDirSeparator;
    } /* if */

    return buf;
} /* cvtToDependent */


#define CVT_TO_DEPENDENT(buf, pre, dir) { \
    const size_t len = ((pre) ? strlen((char *) pre) : 0) + strlen(dir) + 1; \
    buf = cvtToDependent((char*)pre,dir,(char*)__PHYSFS_smallAlloc(len)); \
}



static void *DIR_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_Stat st;
    const char dirsep = __PHYSFS_platformDirSeparator;
    char *retval = NULL;
    const size_t namelen = strlen(name);
    const size_t seplen = 1;
    int exists = 0;

    assert(io == NULL);  /* shouldn't create an Io for these. */
    BAIL_IF_MACRO(!__PHYSFS_platformStat(name, &exists, &st), ERRPASS, NULL);
    if (st.filetype != PHYSFS_FILETYPE_DIRECTORY)
        BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);

    retval = allocator.Malloc(namelen + seplen + 1);
    BAIL_IF_MACRO(retval == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    strcpy(retval, name);

    /* make sure there's a dir separator at the end of the string */
    if (retval[namelen - 1] != dirsep)
    {
        retval[namelen] = dirsep;
        retval[namelen + 1] = '\0';
    } /* if */

    return retval;
} /* DIR_openArchive */


static void DIR_enumerateFiles(void *opaque, const char *dname,
                               PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    char *d;

    CVT_TO_DEPENDENT(d, opaque, dname);
    if (d != NULL)
    {
        __PHYSFS_platformEnumerateFiles(d, cb, origdir, callbackdata);
        __PHYSFS_smallFree(d);
    } /* if */
} /* DIR_enumerateFiles */


static PHYSFS_Io *doOpen(void *opaque, const char *name,
                         const int mode, int *fileExists)
{
    char *f;
    PHYSFS_Io *io = NULL;
    int existtmp = 0;

    CVT_TO_DEPENDENT(f, opaque, name);
    BAIL_IF_MACRO(!f, ERRPASS, NULL);

    if (fileExists == NULL)
        fileExists = &existtmp;

    io = __PHYSFS_createNativeIo(f, mode);
    if (io == NULL)
    {
        const PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
        PHYSFS_Stat statbuf;
        __PHYSFS_platformStat(f, fileExists, &statbuf);
        __PHYSFS_setError(err);
    } /* if */
    else
    {
        *fileExists = 1;
    } /* else */

    __PHYSFS_smallFree(f);

    return io;
} /* doOpen */


static PHYSFS_Io *DIR_openRead(void *opaque, const char *fnm, int *exist)
{
    return doOpen(opaque, fnm, 'r', exist);
} /* DIR_openRead */


static PHYSFS_Io *DIR_openWrite(void *opaque, const char *filename)
{
    return doOpen(opaque, filename, 'w', NULL);
} /* DIR_openWrite */


static PHYSFS_Io *DIR_openAppend(void *opaque, const char *filename)
{
    return doOpen(opaque, filename, 'a', NULL);
} /* DIR_openAppend */


static int DIR_remove(void *opaque, const char *name)
{
    int retval;
    char *f;

    CVT_TO_DEPENDENT(f, opaque, name);
    BAIL_IF_MACRO(!f, ERRPASS, 0);
    retval = __PHYSFS_platformDelete(f);
    __PHYSFS_smallFree(f);
    return retval;
} /* DIR_remove */


static int DIR_mkdir(void *opaque, const char *name)
{
    int retval;
    char *f;

    CVT_TO_DEPENDENT(f, opaque, name);
    BAIL_IF_MACRO(!f, ERRPASS, 0);
    retval = __PHYSFS_platformMkDir(f);
    __PHYSFS_smallFree(f);
    return retval;
} /* DIR_mkdir */


static void DIR_closeArchive(void *opaque)
{
    allocator.Free(opaque);
} /* DIR_closeArchive */


static int DIR_stat(void *opaque, const char *name,
                    int *exists, PHYSFS_Stat *stat)
{
    int retval = 0;
    char *d;

    CVT_TO_DEPENDENT(d, opaque, name);
    BAIL_IF_MACRO(!d, ERRPASS, 0);
    retval = __PHYSFS_platformStat(d, exists, stat);
    __PHYSFS_smallFree(d);
    return retval;
} /* DIR_stat */


const PHYSFS_Archiver __PHYSFS_Archiver_DIR =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "",
        "Non-archive, direct filesystem I/O",
        "Ryan C. Gordon <icculus@icculus.org>",
        "http://icculus.org/physfs/",
    },
    1,  /* supportsSymlinks */
    DIR_openArchive,        /* openArchive() method    */
    DIR_enumerateFiles,     /* enumerateFiles() method */
    DIR_openRead,           /* openRead() method       */
    DIR_openWrite,          /* openWrite() method      */
    DIR_openAppend,         /* openAppend() method     */
    DIR_remove,             /* remove() method         */
    DIR_mkdir,              /* mkdir() method          */
    DIR_closeArchive,       /* closeArchive() method   */
    DIR_stat                /* stat() method           */
};

/* end of archiver_dir.c ... */

