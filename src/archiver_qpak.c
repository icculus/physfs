/*
 * QPAK support routines for PhysicsFS.
 *
 *  This archiver handles the archive format utilized by Quake 1 and 2.
 *  Quake3-based games use the PkZip/Info-Zip format (which our zip.c
 *  archiver handles).
 *
 *  ========================================================================
 *
 *  This format info (in more detail) comes from:
 *     http://debian.fmi.uni-sofia.bg/~sergei/cgsr/docs/pak.txt
 *
 *  Quake PAK Format
 *
 *  Header
 *   (4 bytes)  signature = 'PACK'
 *   (4 bytes)  directory offset
 *   (4 bytes)  directory length
 *
 *  Directory
 *   (56 bytes) file name
 *   (4 bytes)  file position
 *   (4 bytes)  file length
 *
 *  ========================================================================
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_QPAK

#define QPAK_SIG 0x4B434150   /* "PACK" in ASCII. */

static UNPKentry *qpakLoadEntries(PHYSFS_Io *io, PHYSFS_uint32 fileCount)
{
    UNPKentry *entries = NULL;
    UNPKentry *entry = NULL;

    entries = (UNPKentry *) allocator.Malloc(sizeof (UNPKentry) * fileCount);
    BAIL_IF_MACRO(entries == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    for (entry = entries; fileCount > 0; fileCount--, entry++)
    {
        if (!__PHYSFS_readAll(io, &entry->name, 56)) goto failed;
        if (!__PHYSFS_readAll(io, &entry->startPos, 4)) goto failed;
        if (!__PHYSFS_readAll(io, &entry->size, 4)) goto failed;
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = PHYSFS_swapULE32(entry->startPos);
    } /* for */

    return entries;

failed:
    allocator.Free(entries);
    return NULL;
} /* qpakLoadEntries */


static void *QPAK_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    UNPKentry *entries = NULL;
    PHYSFS_uint32 val = 0;
    PHYSFS_uint32 pos = 0;
    PHYSFS_uint32 count = 0;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &val, 4), ERRPASS, NULL);
    if (PHYSFS_swapULE32(val) != QPAK_SIG)
        BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);

    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &val, 4), ERRPASS, NULL);
    pos = PHYSFS_swapULE32(val);  /* directory table offset. */

    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &val, 4), ERRPASS, NULL);
    count = PHYSFS_swapULE32(val);

    /* corrupted archive? */
    BAIL_IF_MACRO((count % 64) != 0, PHYSFS_ERR_CORRUPT, NULL);
    count /= 64;

    BAIL_IF_MACRO(!io->seek(io, pos), ERRPASS, NULL);

    entries = qpakLoadEntries(io, count);
    BAIL_IF_MACRO(!entries, ERRPASS, NULL);
    return UNPK_openArchive(io, entries, count);
} /* QPAK_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_QPAK =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "PAK",
        "Quake I/II format",
        "Ryan C. Gordon <icculus@icculus.org>",
        "http://icculus.org/physfs/",
    },
    0,  /* supportsSymlinks */
    QPAK_openArchive,       /* openArchive() method    */
    UNPK_enumerateFiles,    /* enumerateFiles() method */
    UNPK_openRead,          /* openRead() method       */
    UNPK_openWrite,         /* openWrite() method      */
    UNPK_openAppend,        /* openAppend() method     */
    UNPK_remove,            /* remove() method         */
    UNPK_mkdir,             /* mkdir() method          */
    UNPK_closeArchive,      /* closeArchive() method   */
    UNPK_stat               /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_QPAK */

/* end of archiver_qpak.c ... */

