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

static const FileFunctions __PHYSFS_FileHandle_DIR =
{
    DIR_read,   /* read() method  */
    NULL,       /* write() method */
    DIR_eof,    /* eof() method   */
    DIR_tell,   /* tell() method  */
    DIR_seek,   /* seek() method  */
    DIR_close,  /* close() method */
};


static const FileFunctions __PHYSFS_FileHandle_DIRW =
{
    NULL,       /* read() method  */
    DIR_write,  /* write() method */
    DIR_eof,    /* eof() method   */
    DIR_tell,   /* tell() method  */
    DIR_seek,   /* seek() method  */
    DIR_close,  /* close() method */
};


const DirFunctions __PHYSFS_DirFunctions_DIR =
{
    DIR_isArchive,     /* isArchive() method      */
    DIR_openArchive,   /* openArchive() method    */
    DIR_enumerate,     /* enumerateFiles() method */
    DIR_isDirectory,   /* isDirectory() method    */
    DIR_isSymLink,     /* isSymLink() method      */
    DIR_isOpenable,    /* isOpenable() method     */
    DIR_openRead,      /* openRead() method       */
    DIR_openWrite,     /* openWrite() method      */
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

