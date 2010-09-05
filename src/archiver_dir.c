/*
 * Standard directory I/O support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

/* There's no PHYSFS_Io interface here. Use __PHYSFS_createNativeIo(). */

static void *DIR_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_Stat statbuf;
    const char *dirsep = PHYSFS_getDirSeparator();
    char *retval = NULL;
    const size_t namelen = strlen(name);
    const size_t seplen = strlen(dirsep);
    int exists = 0;

    assert(io == NULL);  /* shouldn't create an Io for these. */
    BAIL_IF_MACRO(!__PHYSFS_platformStat(name, &exists, &statbuf), NULL, NULL);
    if ((!exists) || (statbuf.filetype != PHYSFS_FILETYPE_DIRECTORY))
        BAIL_MACRO(ERR_NOT_AN_ARCHIVE, NULL);

    retval = allocator.Malloc(namelen + seplen + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    /* make sure there's a dir separator at the end of the string */
    /* !!! FIXME: is there really any place where (seplen != 1)? */
    strcpy(retval, name);
    if (strcmp((name + namelen) - seplen, dirsep) != 0)
        strcat(retval, dirsep);

    return retval;
} /* DIR_openArchive */


/* !!! FIXME: I would like to smallAlloc() all these conversions somehow. */

static void DIR_enumerateFiles(dvoid *opaque, const char *dname,
                               int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    char *d = __PHYSFS_platformCvtToDependent((char *) opaque, dname, NULL);
    if (d != NULL)
    {
        __PHYSFS_platformEnumerateFiles(d, omitSymLinks, cb,
                                        origdir, callbackdata);
        allocator.Free(d);
    } /* if */
} /* DIR_enumerateFiles */


static PHYSFS_Io *doOpen(dvoid *opaque, const char *name,
                         const int mode, int *fileExists)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    PHYSFS_Io *io = NULL;
    int existtmp = 0;

    if (fileExists == NULL)
        fileExists = &existtmp;

    *fileExists = 0;

    BAIL_IF_MACRO(f == NULL, NULL, NULL);

    io = __PHYSFS_createNativeIo(f, mode);
    allocator.Free(f);
    if (io == NULL)
    {
        PHYSFS_Stat statbuf;
        __PHYSFS_platformStat(f, fileExists, &statbuf);
        return NULL;
    } /* if */

    *fileExists = 1;
    return io;
} /* doOpen */


static PHYSFS_Io *DIR_openRead(dvoid *opaque, const char *fnm, int *exist)
{
    return doOpen(opaque, fnm, 'r', exist);
} /* DIR_openRead */


static PHYSFS_Io *DIR_openWrite(dvoid *opaque, const char *filename)
{
    return doOpen(opaque, filename, 'w', NULL);
} /* DIR_openWrite */


static PHYSFS_Io *DIR_openAppend(dvoid *opaque, const char *filename)
{
    return doOpen(opaque, filename, 'a', NULL);
} /* DIR_openAppend */


static int DIR_remove(dvoid *opaque, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);
    retval = __PHYSFS_platformDelete(f);
    allocator.Free(f);
    return retval;
} /* DIR_remove */


static int DIR_mkdir(dvoid *opaque, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);
    retval = __PHYSFS_platformMkDir(f);
    allocator.Free(f);
    return retval;
} /* DIR_mkdir */


static void DIR_dirClose(dvoid *opaque)
{
    allocator.Free(opaque);
} /* DIR_dirClose */


static int DIR_stat(dvoid *opaque, const char *name, int *exists,
                    PHYSFS_Stat *stat)
{
    char *d = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval = 0;

    BAIL_IF_MACRO(d == NULL, NULL, 0);
    retval = __PHYSFS_platformStat(d, exists, stat);
    allocator.Free(d);
    return retval;
} /* DIR_stat */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_DIR =
{
    "",
    DIR_ARCHIVE_DESCRIPTION,
    "Ryan C. Gordon <icculus@icculus.org>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_DIR =
{
    &__PHYSFS_ArchiveInfo_DIR,
    DIR_openArchive,        /* openArchive() method    */
    DIR_enumerateFiles,     /* enumerateFiles() method */
    DIR_openRead,           /* openRead() method       */
    DIR_openWrite,          /* openWrite() method      */
    DIR_openAppend,         /* openAppend() method     */
    DIR_remove,             /* remove() method         */
    DIR_mkdir,              /* mkdir() method          */
    DIR_dirClose,           /* dirClose() method       */
    DIR_stat                /* stat() method           */
};

/* end of dir.c ... */

