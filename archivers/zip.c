/*
 * ZIP support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if (!defined PHYSFS_SUPPORTS_ZIP)
#error PHYSFS_SUPPORTS_ZIP must be defined.
#endif


static const FileFunctions __PHYSFS_FileHandle_ZIP =
{
    ZIP_read,   /* read() method  */
    NULL,       /* write() method */
    ZIP_eof,    /* eof() method   */
    ZIP_tell,   /* tell() method  */
    ZIP_seek,   /* seek() method  */
    ZIP_close,  /* close() method */
};


const DirFunctions __PHYSFS_DirFunctions_ZIP =
{
    ZIP_isArchive,     /* isArchive() method      */
    ZIP_openArchive,   /* openArchive() method    */
    ZIP_enumerate,     /* enumerateFiles() method */
    ZIP_exists,        /* exists() method         */
    ZIP_isDirectory,   /* isDirectory() method    */
    ZIP_isSymLink,     /* isSymLink() method      */
    ZIP_openRead,      /* openRead() method       */
    NULL,              /* openWrite() method      */
    NULL,              /* openAppend() method     */
    NULL,              /* remove() method         */
    NULL,              /* mkdir() method          */
    ZIP_close,         /* close() method          */
};

const __PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_ZIP =
{
    "ZIP",
    "PkZip/WinZip/Info-Zip compatible"
};

/* end of zip.c ... */

