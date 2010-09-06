/*
 * GRP support routines for PhysicsFS.
 *
 * This driver handles BUILD engine archives ("groupfiles"). This format
 *  (but not this driver) was put together by Ken Silverman.
 *
 * The format is simple enough. In Ken's words:
 *
 *    What's the .GRP file format?
 *
 *     The ".grp" file format is just a collection of a lot of files stored
 *     into 1 big one. I tried to make the format as simple as possible: The
 *     first 12 bytes contains my name, "KenSilverman". The next 4 bytes is
 *     the number of files that were compacted into the group file. Then for
 *     each file, there is a 16 byte structure, where the first 12 bytes are
 *     the filename, and the last 4 bytes are the file's size. The rest of
 *     the group file is just the raw data packed one after the other in the
 *     same order as the list of files.
 *
 * (That info is from http://www.advsys.net/ken/build.htm ...)
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if (defined PHYSFS_SUPPORTS_GRP)

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

static inline int readAll(PHYSFS_Io *io, void *buf, const PHYSFS_uint64 len)
{
    return (io->read(io, buf, len) == len);
} /* readAll */

static UNPKentry *grpLoadEntries(PHYSFS_Io *io, PHYSFS_uint32 fileCount)
{
    PHYSFS_uint32 location = 16;  /* sizeof sig. */
    UNPKentry *entries = NULL;
    UNPKentry *entry = NULL;
    char *ptr = NULL;

    entries = (UNPKentry *) allocator.Malloc(sizeof (UNPKentry) * fileCount);
    BAIL_IF_MACRO(entries == NULL, ERR_OUT_OF_MEMORY, NULL);

    location += (16 * fileCount);

    for (entry = entries; fileCount > 0; fileCount--, entry++)
    {
        GOTO_IF_MACRO(!readAll(io, &entry->name, 12), NULL, grpLoad_failed);
        GOTO_IF_MACRO(!readAll(io, &entry->size, 4), NULL, grpLoad_failed);
        entry->name[12] = '\0';  /* name isn't null-terminated in file. */
        if ((ptr = strchr(entry->name, ' ')) != NULL)
            *ptr = '\0';  /* trim extra spaces. */

        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = location;
        location += entry->size;
    } /* for */

    return entries;

grpLoad_failed:
    allocator.Free(entries);
    return NULL;
} /* grpLoadEntries */


static void *GRP_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 buf[12];
    PHYSFS_uint32 entryCount = 0;
    UNPKentry *entries = NULL;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    BAIL_IF_MACRO(!readAll(io, buf, sizeof (buf)), NULL, NULL);
    if (memcmp(buf, "KenSilverman", sizeof (buf)) != 0)
        BAIL_MACRO(ERR_NOT_AN_ARCHIVE, NULL);

    BAIL_IF_MACRO(!readAll(io, &entryCount, sizeof (entryCount)), NULL, NULL);
    entryCount = PHYSFS_swapULE32(entryCount);

    entries = grpLoadEntries(io, entryCount);
    BAIL_IF_MACRO(entries == NULL, NULL, NULL);
    return UNPK_openArchive(io, entries, entryCount);
} /* GRP_openArchive */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_GRP =
{
    "GRP",
    GRP_ARCHIVE_DESCRIPTION,
    "Ryan C. Gordon <icculus@icculus.org>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_GRP =
{
    &__PHYSFS_ArchiveInfo_GRP,
    GRP_openArchive,        /* openArchive() method    */
    UNPK_enumerateFiles,    /* enumerateFiles() method */
    UNPK_openRead,          /* openRead() method       */
    UNPK_openWrite,         /* openWrite() method      */
    UNPK_openAppend,        /* openAppend() method     */
    UNPK_remove,            /* remove() method         */
    UNPK_mkdir,             /* mkdir() method          */
    UNPK_dirClose,          /* dirClose() method       */
    UNPK_stat               /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_GRP */

/* end of grp.c ... */

