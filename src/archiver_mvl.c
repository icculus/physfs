/*
 * MVL support routines for PhysicsFS.
 *
 * This driver handles Descent II Movielib archives.
 *
 * The file format of MVL is quite easy...
 *
 *   //MVL File format - Written by Heiko Herrmann
 *   char sig[4] = {'D','M', 'V', 'L'}; // "DMVL"=Descent MoVie Library
 *
 *   int num_files; // the number of files in this MVL
 *
 *   struct {
 *    char file_name[13]; // Filename, padded to 13 bytes with 0s
 *    int file_size; // filesize in bytes
 *   }DIR_STRUCT[num_files];
 *
 *   struct {
 *    char data[file_size]; // The file data
 *   }FILE_STRUCT[num_files];
 *
 * (That info is from http://www.descent2.com/ddn/specs/mvl/)
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Bradley Bell.
 *  Based on grp.c by Ryan C. Gordon.
 */

#if (defined PHYSFS_SUPPORTS_MVL)

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

static UNPKentry *mvlLoadEntries(PHYSFS_Io *io, PHYSFS_uint32 fileCount)
{
    PHYSFS_uint32 location = 8;  /* sizeof sig. */
    UNPKentry *entries = NULL;
    UNPKentry *entry = NULL;

    entries = (UNPKentry *) allocator.Malloc(sizeof (UNPKentry) * fileCount);
    BAIL_IF_MACRO(entries == NULL, ERR_OUT_OF_MEMORY, NULL);

    location += (17 * fileCount);

    for (entry = entries; fileCount > 0; fileCount--, entry++)
    {
        GOTO_IF_MACRO(!__PHYSFS_readAll(io, &entry->name, 13), NULL, failed);
        GOTO_IF_MACRO(!__PHYSFS_readAll(io, &entry->size, 4), NULL, failed);
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = location;
        location += entry->size;
    } /* for */

    return entries;

failed:
    allocator.Free(entries);
    return NULL;
} /* mvlLoadEntries */


static void *MVL_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 buf[4];
    PHYSFS_uint32 count = 0;
    UNPKentry *entries = NULL;

    assert(io != NULL);  /* shouldn't ever happen. */
    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, buf, 4), NULL, NULL);
    BAIL_IF_MACRO(memcmp(buf, "DMVL", 4) != 0, ERR_NOT_AN_ARCHIVE, NULL);
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &count, sizeof (count)), NULL, NULL);

    count = PHYSFS_swapULE32(count);
    entries = mvlLoadEntries(io, count);
    BAIL_IF_MACRO(entries == NULL, NULL, NULL);
    return UNPK_openArchive(io, entries, count);
} /* MVL_openArchive */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_MVL =
{
    "MVL",
    MVL_ARCHIVE_DESCRIPTION,
    "Bradley Bell <btb@icculus.org>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_MVL =
{
    &__PHYSFS_ArchiveInfo_MVL,
    MVL_openArchive,        /* openArchive() method    */
    UNPK_enumerateFiles,     /* enumerateFiles() method */
    UNPK_openRead,           /* openRead() method       */
    UNPK_openWrite,          /* openWrite() method      */
    UNPK_openAppend,         /* openAppend() method     */
    UNPK_remove,             /* remove() method         */
    UNPK_mkdir,              /* mkdir() method          */
    UNPK_dirClose,           /* dirClose() method       */
    UNPK_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_MVL */

/* end of mvl.c ... */

