/*
 * WAD support routines for PhysicsFS.
 *
 * This driver handles DOOM engine archives ("wads"). 
 * This format (but not this driver) was designed by id Software for use
 *  with the DOOM engine.
 * The specs of the format are from the unofficial doom specs v1.666
 * found here: http://www.gamers.org/dhs/helpdocs/dmsp1666.html
 * The format of the archive: (from the specs)
 *
 *  A WAD file has three parts:
 *  (1) a twelve-byte header
 *  (2) one or more "lumps"
 *  (3) a directory or "info table" that contains the names, offsets, and
 *      sizes of all the lumps in the WAD
 *
 *  The header consists of three four-byte parts:
 *    (a) an ASCII string which must be either "IWAD" or "PWAD"
 *    (b) a 4-byte (long) integer which is the number of lumps in the wad
 *    (c) a long integer which is the file offset to the start of
 *    the directory
 *
 *  The directory has one 16-byte entry for every lump. Each entry consists
 *  of three parts:
 *
 *    (a) a long integer, the file offset to the start of the lump
 *    (b) a long integer, the size of the lump in bytes
 *    (c) an 8-byte ASCII string, the name of the lump, padded with zeros.
 *        For example, the "DEMO1" entry in hexadecimal would be
 *        (44 45 4D 4F 31 00 00 00)
 * 
 * Note that there is no way to tell if an opened WAD archive is a
 *  IWAD or PWAD with this archiver.
 * I couldn't think of a way to provide that information, without being too
 *  hacky.
 * I don't think it's really that important though.
 *
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 * This file written by Travis Wells, based on the GRP archiver by
 *  Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_WAD

static UNPKentry *wadLoadEntries(PHYSFS_Io *io, PHYSFS_uint32 fileCount)
{
    PHYSFS_uint32 directoryOffset;
    UNPKentry *entries = NULL;
    UNPKentry *entry = NULL;

    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &directoryOffset, 4), ERRPASS, 0);
    directoryOffset = PHYSFS_swapULE32(directoryOffset);

    BAIL_IF_MACRO(!io->seek(io, directoryOffset), ERRPASS, 0);

    entries = (UNPKentry *) allocator.Malloc(sizeof (UNPKentry) * fileCount);
    BAIL_IF_MACRO(!entries, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    for (entry = entries; fileCount > 0; fileCount--, entry++)
    {
        if (!__PHYSFS_readAll(io, &entry->startPos, 4)) goto failed;
        if (!__PHYSFS_readAll(io, &entry->size, 4)) goto failed;
        if (!__PHYSFS_readAll(io, &entry->name, 8)) goto failed;

        entry->name[8] = '\0'; /* name might not be null-terminated in file. */
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = PHYSFS_swapULE32(entry->startPos);
    } /* for */

    return entries;

failed:
    allocator.Free(entries);
    return NULL;
} /* wadLoadEntries */


static void *WAD_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 buf[4];
    UNPKentry *entries = NULL;
    PHYSFS_uint32 count = 0;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, buf, sizeof (buf)), ERRPASS, NULL);
    if ((memcmp(buf, "IWAD", 4) != 0) && (memcmp(buf, "PWAD", 4) != 0))
        BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);

    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &count, sizeof (count)), ERRPASS, NULL);
    count = PHYSFS_swapULE32(count);

    entries = wadLoadEntries(io, count);
    BAIL_IF_MACRO(!entries, ERRPASS, NULL);
    return UNPK_openArchive(io, entries, count);
} /* WAD_openArchive */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_WAD =
{
    "WAD",
    "DOOM engine format",
    "Travis Wells <traviswells@mchsi.com>",
    "http://www.3dmm2.com/doom/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_WAD =
{
    &__PHYSFS_ArchiveInfo_WAD,
    WAD_openArchive,        /* openArchive() method    */
    UNPK_enumerateFiles,     /* enumerateFiles() method */
    UNPK_openRead,           /* openRead() method       */
    UNPK_openWrite,          /* openWrite() method      */
    UNPK_openAppend,         /* openAppend() method     */
    UNPK_remove,             /* remove() method         */
    UNPK_mkdir,              /* mkdir() method          */
    UNPK_closeArchive,       /* closeArchive() method   */
    UNPK_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_WAD */

/* end of wad.c ... */

