/*
 * ZIP support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if (!defined PHYSFS_SUPPORTS_ZIP)
#error PHYSFS_SUPPORTS_ZIP must be defined.
#endif

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
} /* ZIP_isArchive */


static DirHandle *ZIP_openArchive(const char *name, int forWriting)
{
} /* ZIP_openArchive */


static LinkedStringList *ZIP_enumerateFiles(DirHandle *h, const char *dirname)
{
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
    "Ryan C. Gordon",
    "http://www.icculus.org/",
};

/* end of zip.c ... */

