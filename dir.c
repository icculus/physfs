/*
 * Standard directory I/O support routines for PhysicsFS.
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
const FileHandle __PHYSFS_FileHandle_DIR =
{
    NULL,       /* opaque         */
    NULL,       /* dirReader      */
    DIR_read,   /* read() method  */
    NULL,       /* write() method */
    DIR_eof,    /* eof() method   */
    DIR_tell,   /* tell() method  */
    DIR_seek,   /* seek() method  */
    DIR_close,  /* close() method */
};

/* template for directories. */
const DirReader __PHYSFS_DirReader_DIR =
{
    NULL,              /* opaque                  */
    DIR_enumerate,     /* enumerateFiles() method */
    DIR_isDirectory,   /* isDirectory() method    */
    DIR_isSymLink,     /* isSymLink() method      */
    DIR_isOpenable,    /* isOpenable() method     */
    DIR_openRead,      /* openRead() method       */
    DIR_dirClose,      /* close() method          */
};


/* This doesn't get listed, since it's technically not an archive... */
#if 0
const __PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_DIR =
{
    "DIR",
    "non-archive directory I/O"
};
#endif

/* end of dir.c ... */

