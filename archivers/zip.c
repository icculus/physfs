/*
 * ZIP support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "physfs.h"
#include "unzip.h"


#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if (!defined PHYSFS_SUPPORTS_ZIP)
#error PHYSFS_SUPPORTS_ZIP must be defined.
#endif


typedef struct
{
    unzFile handle;
    uLong totalEntries;
} ZIPinfo;

typedef struct
{
} ZIPfileinfo;


extern const DirFunctions __PHYSFS_DirFunctions_ZIP;
static const FileFunctions __PHYSFS_FileFunctions_ZIP;


static int ZIP_read(FileHandle *handle, void *buffer,
                    unsigned int objSize, unsigned int objCount)
{
} /* ZIP_read */


static int ZIP_eof(FileHandle *handle)
{
} /* ZIP_eof */


static int ZIP_tell(FileHandle *handle)
{
} /* ZIP_tell */


static int ZIP_seek(FileHandle *handle, int offset)
{
} /* ZIP_seek */


static int ZIP_fileLength(FileHandle *handle)
{
} /* ZIP_fileLength */


static int ZIP_fileClose(FileHandle *handle)
{
} /* ZIP_fileClose */


static int ZIP_isArchive(const char *filename, int forWriting)
{
    int retval = 0;
    unzFile unz = unzOpen(name);
    unz_global_info global;

    if (unz != NULL)
    {
        if (unzGetGlobalInfo(unz, &global) == UNZ_OK)
            retval = 1;
        unzClose(unz);
    } /* if */

    return(retval);
} /* ZIP_isArchive */


static DirHandle *ZIP_openArchive(const char *name, int forWriting)
{
    unzFile unz = NULL;
    DirHandle *retval = NULL;
    unz_global_info global;

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, NULL);

    errno = 0;
    BAIL_IF_MACRO(access(name, R_OK) != 0, strerror(errno), NULL);

    retval = malloc(sizeof (DirHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    unz = unzOpen(name);
    if ((unz == NULL) || (unzGetGlobalInfo(unz, &global) != UNZ_OK))
    {
        if (unz)
            unzClose(unz);
        free(retval);
        BAIL_IF_MACRO(1, ERR_UNSUPPORTED_ARCHIVE, NULL);
    } /* if */

    retval->opaque = malloc(sizeof (ZIPinfo));
    if (retval->opaque == NULL)
    {
        free(retval);
        unzClose(unz);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    ((ZIPinfo *) (retval->opaque))->handle = unz;
    ((ZIPinfo *) (retval->opaque))->totalEntries = global.number_entry;

    return(retval);
} /* ZIP_openArchive */


static LinkedStringList *ZIP_enumerateFiles(DirHandle *h, const char *dirname)
{
    ZIPinfo *zi = (ZIPinfo *) (h->opaque);
    unzFile fh = zi->handle;
    int i;
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;
    char buffer[256];
    char *d;

        /* jump to first file entry... */
    BAIL_IF_MACRO(unzGoToFirstFile(fh) != UNZ_OK, ERR_IO_ERROR, NULL);

    i = strlen(dirname);
    d = malloc(i + 1);
    strcpy(d, dirname);
    if ((i > 0) && (d[i - 1] == '/'))   /* no trailing slash. */
        d[i - 1] = '\0';

    for (i = 0; i < zi->totalEntries; i++)
    {
        char *ptr;
        unzGetCurrentFileInfo(fh, NULL, buf, sizeof (buf), NULL, 0, NULL, 0);
        ptr = strrchr(p, '/');  /* !!! check this! */
        if (ptr == NULL)
        {
            if (*d != '\0')
                continue; /* not for this dir; skip it. */
            ptr = buf;
        else
        {
            *ptr = '\0';
            ptr++;
            if (strcmp(buf, d) != 0)
                continue; /* not for this dir; skip it. */
        } /* else */

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l != NULL)
            break;

        l->str = (char *) malloc(strlen(ptr) + 1);
        if (l->str == NULL)
        {
            free(l);
            break;
        } /* if */

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* for */

    free(d);
    return(retval);
} /* ZIP_enumerateFiles */


static int ZIP_exists(DirHandle *h, const char *name)
{
} /* ZIP_exists */


static int ZIP_isDirectory(DirHandle *h, const char *name)
{
} /* ZIP_isDirectory */


static int ZIP_isSymLink(DirHandle *h, const char *name)
{
} /* ZIP_isSymLink */


static FileHandle *ZIP_openRead(DirHandle *h, const char *filename)
{
} /* ZIP_openRead */


static void ZIP_dirClose(DirHandle *h)
{
} /* ZIP_dirClose */


static const FileFunctions __PHYSFS_FileFunctions_ZIP =
{
    ZIP_read,       /* read() method       */
    NULL,           /* write() method      */
    ZIP_eof,        /* eof() method        */
    ZIP_tell,       /* tell() method       */
    ZIP_seek,       /* seek() method       */
    ZIP_fileLength, /* fileLength() method */
    ZIP_fileClose   /* fileClose() method  */
};


const DirFunctions __PHYSFS_DirFunctions_ZIP =
{
    ZIP_isArchive,          /* isArchive() method      */
    ZIP_openArchive,        /* openArchive() method    */
    ZIP_enumerateFiles,     /* enumerateFiles() method */
    ZIP_exists,             /* exists() method         */
    ZIP_isDirectory,        /* isDirectory() method    */
    ZIP_isSymLink,          /* isSymLink() method      */
    ZIP_openRead,           /* openRead() method       */
    NULL,                   /* openWrite() method      */
    NULL,                   /* openAppend() method     */
    NULL,                   /* remove() method         */
    NULL,                   /* mkdir() method          */
    ZIP_dirClose            /* dirClose() method       */
};

const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_ZIP =
{
    "ZIP",
    "PkZip/WinZip/Info-Zip compatible",
    "Ryan C. Gordon (icculus@linuxgames.com)",
    "http://www.icculus.org/~icculus/",
};

/* end of zip.c ... */

