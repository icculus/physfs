/*
 * Standard directory I/O support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

static PHYSFS_sint64 DIR_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static PHYSFS_sint64 DIR_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static PHYSFS_sint64 DIR_dummyRead(FileHandle *handle, void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static PHYSFS_sint64 DIR_dummyWrite(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static int DIR_eof(FileHandle *handle);
static PHYSFS_sint64 DIR_tell(FileHandle *handle);
static int DIR_seek(FileHandle *handle, PHYSFS_uint64 offset);
static PHYSFS_sint64 DIR_fileLength(FileHandle *handle);
static int DIR_fileClose(FileHandle *handle);
static int DIR_isArchive(const char *filename, int forWriting);
static void *DIR_openArchive(const char *name, int forWriting);
static LinkedStringList *DIR_enumerateFiles(void *opaque,
                                            const char *dname,
                                            int omitSymLinks);
static int DIR_exists(void *opaque, const char *name);
static int DIR_isDirectory(void *opaque, const char *name, int *fileExists);
static int DIR_isSymLink(void *opaque, const char *name, int *fileExists);
static FileHandle *DIR_openRead(void *opaque, const char *fnm, int *exist);
static PHYSFS_sint64 DIR_getLastModTime(void *opaque, const char *f, int *e);
static FileHandle *DIR_openWrite(void *opaque, const char *filename);
static FileHandle *DIR_openAppend(void *opaque, const char *filename);
static int DIR_remove(void *opaque, const char *name);
static int DIR_mkdir(void *opaque, const char *name);
static void DIR_dirClose(void *opaque);


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_DIR =
{
    "",
    DIR_ARCHIVE_DESCRIPTION,
    "Ryan C. Gordon <icculus@clutteredmind.org>",
    "http://icculus.org/physfs/",
};


static const FileFunctions __PHYSFS_FileFunctions_DIR =
{
    DIR_read,       /* read() method       */
    DIR_dummyWrite, /* write() method      */
    DIR_eof,        /* eof() method        */
    DIR_tell,       /* tell() method       */
    DIR_seek,       /* seek() method       */
    DIR_fileLength, /* fileLength() method */
    DIR_fileClose   /* fileClose() method  */
};


static const FileFunctions __PHYSFS_FileFunctions_DIRW =
{
    DIR_dummyRead,  /* read() method       */
    DIR_write,      /* write() method      */
    DIR_eof,        /* eof() method        */
    DIR_tell,       /* tell() method       */
    DIR_seek,       /* seek() method       */
    DIR_fileLength, /* fileLength() method */
    DIR_fileClose   /* fileClose() method  */
};


const DirFunctions __PHYSFS_DirFunctions_DIR =
{
    &__PHYSFS_ArchiveInfo_DIR,
    DIR_isArchive,          /* isArchive() method      */
    DIR_openArchive,        /* openArchive() method    */
    DIR_enumerateFiles,     /* enumerateFiles() method */
    DIR_exists,             /* exists() method         */
    DIR_isDirectory,        /* isDirectory() method    */
    DIR_isSymLink,          /* isSymLink() method      */
    DIR_getLastModTime,     /* getLastModTime() method */
    DIR_openRead,           /* openRead() method       */
    DIR_openWrite,          /* openWrite() method      */
    DIR_openAppend,         /* openAppend() method     */
    DIR_remove,             /* remove() method         */
    DIR_mkdir,              /* mkdir() method          */
    DIR_dirClose            /* dirClose() method       */
};


static PHYSFS_sint64 DIR_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    PHYSFS_sint64 retval;
    retval = __PHYSFS_platformRead(handle->opaque, buffer, objSize, objCount);
    return(retval);
} /* DIR_read */


static PHYSFS_sint64 DIR_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    PHYSFS_sint64 retval;
    retval = __PHYSFS_platformWrite(handle->opaque, buffer, objSize, objCount);
    return(retval);
} /* DIR_write */


static PHYSFS_sint64 DIR_dummyRead(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* DIR_dummyRead */


static PHYSFS_sint64 DIR_dummyWrite(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* DIR_dummyWrite */


static int DIR_eof(FileHandle *handle)
{
    return(__PHYSFS_platformEOF(handle->opaque));
} /* DIR_eof */


static PHYSFS_sint64 DIR_tell(FileHandle *handle)
{
    return(__PHYSFS_platformTell(handle->opaque));
} /* DIR_tell */


static int DIR_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    return(__PHYSFS_platformSeek(handle->opaque, offset));
} /* DIR_seek */


static PHYSFS_sint64 DIR_fileLength(FileHandle *handle)
{
    return(__PHYSFS_platformFileLength(handle->opaque));
} /* DIR_fileLength */


static int DIR_fileClose(FileHandle *handle)
{
    /*
     * we manually flush the buffer, since that's the place a close will
     *  most likely fail, but that will leave the file handle in an undefined
     *  state if it fails. Flush failures we can recover from.
     */
    BAIL_IF_MACRO(!__PHYSFS_platformFlush(handle->opaque), NULL, 0);
    BAIL_IF_MACRO(!__PHYSFS_platformClose(handle->opaque), NULL, 0);
    free(handle);
    return(1);
} /* DIR_fileClose */


static int DIR_isArchive(const char *filename, int forWriting)
{
    /* directories ARE archives in this driver... */
    return(__PHYSFS_platformIsDirectory(filename));
} /* DIR_isArchive */


static void *DIR_openArchive(const char *name, int forWriting)
{
    const char *dirsep = PHYSFS_getDirSeparator();
    char *retval = NULL;
    size_t namelen = strlen(name);
    size_t seplen = strlen(dirsep);

    /* !!! FIXME: when is this not called right before openArchive? */
    BAIL_IF_MACRO(!DIR_isArchive(name, forWriting),
                    ERR_UNSUPPORTED_ARCHIVE, 0);

    retval = malloc(namelen + seplen + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

        /* make sure there's a dir separator at the end of the string */
    strcpy(retval, name);
    if (strcmp((name + namelen) - seplen, dirsep) != 0)
        strcat(retval, dirsep);

    return(retval);
} /* DIR_openArchive */


static LinkedStringList *DIR_enumerateFiles(void *opaque,
                                            const char *dname,
                                            int omitSymLinks)
{
    char *d = __PHYSFS_platformCvtToDependent((char *)opaque, dname, NULL);
    LinkedStringList *retval;

    BAIL_IF_MACRO(d == NULL, NULL, NULL);
    retval = __PHYSFS_platformEnumerateFiles(d, omitSymLinks);
    free(d);
    return(retval);
} /* DIR_enumerateFiles */


static int DIR_exists(void *opaque, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);
    retval = __PHYSFS_platformExists(f);
    free(f);
    return(retval);
} /* DIR_exists */


static int DIR_isDirectory(void *opaque, const char *name, int *fileExists)
{
    char *d = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval = 0;

    BAIL_IF_MACRO(d == NULL, NULL, 0);
    *fileExists = __PHYSFS_platformExists(d);
    if (*fileExists)
        retval = __PHYSFS_platformIsDirectory(d);
    free(d);
    return(retval);
} /* DIR_isDirectory */


static int DIR_isSymLink(void *opaque, const char *name, int *fileExists)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval = 0;

    BAIL_IF_MACRO(f == NULL, NULL, 0);
    *fileExists = __PHYSFS_platformExists(f);
    if (*fileExists)
        retval = __PHYSFS_platformIsSymLink(f);
    free(f);
    return(retval);
} /* DIR_isSymLink */


static PHYSFS_sint64 DIR_getLastModTime(void *opaque,
                                        const char *name,
                                        int *fileExists)
{
    char *d = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    PHYSFS_sint64 retval = -1;

    BAIL_IF_MACRO(d == NULL, NULL, 0);
    *fileExists = __PHYSFS_platformExists(d);
    if (*fileExists)
        retval = __PHYSFS_platformGetLastModTime(d);
    free(d);
    return(retval);
} /* DIR_getLastModTime */


static FileHandle *doOpen(void *opaque, const char *name,
                          void *(*openFunc)(const char *filename),
                          int *fileExists, const FileFunctions *fileFuncs)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    void *rc;
    FileHandle *retval;

    BAIL_IF_MACRO(f == NULL, NULL, NULL);

    if (fileExists != NULL)
    {
        *fileExists = __PHYSFS_platformExists(f);
        if (!(*fileExists))
        {
            free(f);
            return(NULL);
        } /* if */
    } /* if */

    retval = (FileHandle *) malloc(sizeof (FileHandle));
    if (!retval)
    {
        free(f);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    rc = openFunc(f);
    free(f);

    if (!rc)
    {
        free(retval);
        return(NULL);
    } /* if */

    retval->opaque = (void *) rc;
    retval->funcs = fileFuncs;

    return(retval);
} /* doOpen */


static FileHandle *DIR_openRead(void *opaque, const char *fnm, int *exist)
{
    return(doOpen(opaque, fnm, __PHYSFS_platformOpenRead, exist,
                  &__PHYSFS_FileFunctions_DIR));
} /* DIR_openRead */


static FileHandle *DIR_openWrite(void *opaque, const char *filename)
{
    return(doOpen(opaque, filename, __PHYSFS_platformOpenWrite, NULL,
                  &__PHYSFS_FileFunctions_DIRW));
} /* DIR_openWrite */


static FileHandle *DIR_openAppend(void *opaque, const char *filename)
{
    return(doOpen(opaque, filename, __PHYSFS_platformOpenAppend, NULL,
                  &__PHYSFS_FileFunctions_DIRW));
} /* DIR_openAppend */


static int DIR_remove(void *opaque, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);
    retval = __PHYSFS_platformDelete(f);
    free(f);
    return(retval);
} /* DIR_remove */


static int DIR_mkdir(void *opaque, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *) opaque, name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);
    retval = __PHYSFS_platformMkDir(f);
    free(f);
    return(retval);
} /* DIR_mkdir */


static void DIR_dirClose(void *opaque)
{
    free(opaque);
} /* DIR_dirClose */

/* end of dir.c ... */

