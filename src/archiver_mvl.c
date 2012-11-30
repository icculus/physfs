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

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_MVL

static UNPKentry *mvlLoadEntries(PHYSFS_Io *io, PHYSFS_uint32 fileCount)
{
    PHYSFS_uint32 location = 8;  /* sizeof sig. */
    UNPKentry *entries = NULL;
    UNPKentry *entry = NULL;

    entries = (UNPKentry *) allocator.Malloc(sizeof (UNPKentry) * fileCount);
    BAIL_IF_MACRO(entries == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    location += (17 * fileCount);

    for (entry = entries; fileCount > 0; fileCount--, entry++)
    {
        if (!__PHYSFS_readAll(io, &entry->name, 13)) goto failed;
        if (!__PHYSFS_readAll(io, &entry->size, 4)) goto failed;
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
    BAIL_IF_MACRO(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, buf, 4), ERRPASS, NULL);
    BAIL_IF_MACRO(memcmp(buf, "DMVL", 4) != 0, PHYSFS_ERR_UNSUPPORTED, NULL);
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &count, sizeof(count)), ERRPASS, NULL);

    count = PHYSFS_swapULE32(count);
    entries = mvlLoadEntries(io, count);
    return (!entries) ? NULL : UNPK_openArchive(io, entries, count);
} /* MVL_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_MVL =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "MVL",
        "Descent II Movielib format",
        "Bradley Bell <btb@icculus.org>",
        "http://icculus.org/physfs/",
        0,  /* supportsSymlinks */
    },
    MVL_openArchive,
    UNPK_enumerateFiles,
    UNPK_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
    UNPK_closeArchive,
    UNPK_stat
};

#endif  /* defined PHYSFS_SUPPORTS_MVL */

/* end of archiver_mvl.c ... */

