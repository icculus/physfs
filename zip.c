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

/* template for filehandles. */
const FileHandle __PHYSFS_FileHandle_ZIP =
{
    NULL,       /* opaque         */
    NULL,       /* dirReader      */
    ZIP_read,   /* read() method  */
    NULL,       /* write() method */
    ZIP_eof,    /* eof() method   */
    ZIP_tell,   /* tell() method  */
    ZIP_seek,   /* seek() method  */
    ZIP_close,  /* close() method */
};

/* template for directories. */
const DirReader __PHYSFS_DirReader_ZIP =
{
    NULL,              /* opaque                  */
    ZIP_enumerate,     /* enumerateFiles() method */
    ZIP_isDirectory,   /* isDirectory() method    */
    ZIP_isSymLink,     /* isSymLink() method      */
    ZIP_isOpenable,    /* isOpenable() method     */
    ZIP_openRead,      /* openRead() method       */
    ZIP_dirClose,      /* close() method          */
};

const __PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_ZIP =
{
    "ZIP",
    "PkZip/WinZip/Info-Zip compatible"
};

/* end of zip.c ... */

