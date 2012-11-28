/*
 * SLB support routines for PhysicsFS.
 *
 * This driver handles SLB archives ("slab files"). This uncompressed format
 * is used in I-War / Independence War and Independence War: Defiance.
 *
 * The format begins with four zero bytes (version?), the file count and the
 * location of the table of contents. Each ToC entry contains a 64-byte buffer
 * containing a zero-terminated filename, the offset of the data, and its size.
 * All the filenames begin with the separator character '\'. 
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 * This file written by Aleksi Nurmi, based on the GRP archiver by
 * Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_SLB

static UNPKentry *slbLoadEntries(PHYSFS_Io *io, PHYSFS_uint32 fileCount)
{
    UNPKentry *entries = NULL;
    UNPKentry *entry = NULL;

    entries = (UNPKentry *) allocator.Malloc(sizeof (UNPKentry) * fileCount);
    BAIL_IF_MACRO(entries == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    for (entry = entries; fileCount > 0; fileCount--, entry++)
    {
        char *ptr;

        /* don't include the '\' in the beginning */
        char backslash;
        GOTO_IF_MACRO(!__PHYSFS_readAll(io, &backslash, 1), ERRPASS, failed);
        GOTO_IF_MACRO(backslash != '\\', ERRPASS, failed);

        /* read the rest of the buffer, 63 bytes */
        GOTO_IF_MACRO(!__PHYSFS_readAll(io, &entry->name, 63), ERRPASS, failed);
        entry->name[63] = '\0'; /* in case the name lacks the null terminator */

        /* convert backslashes */
        for (ptr = entry->name; *ptr; ptr++)
        {
            if (*ptr == '\\')
                *ptr = '/';
        } /* for */

        GOTO_IF_MACRO(!__PHYSFS_readAll(io, &entry->startPos, 4),
                      ERRPASS, failed);
        entry->startPos = PHYSFS_swapULE32(entry->startPos);

        GOTO_IF_MACRO(!__PHYSFS_readAll(io, &entry->size, 4), ERRPASS, failed);
        entry->size = PHYSFS_swapULE32(entry->size);
    } /* for */
    
    return entries;

failed:
    allocator.Free(entries);
    return NULL;

} /* slbLoadEntries */


static void *SLB_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint32 version;
    PHYSFS_uint32 count = 0;
    PHYSFS_uint32 tocPos = 0;
    UNPKentry *entries = NULL;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &version, sizeof(version)),
                  ERRPASS, NULL);
    version = PHYSFS_swapULE32(version);
    BAIL_IF_MACRO(version != 0, ERRPASS, NULL);

    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &count, sizeof(count)), ERRPASS, NULL);
    count = PHYSFS_swapULE32(count);

    /* offset of the table of contents */
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &tocPos, sizeof(tocPos)),
                  ERRPASS, NULL);
    tocPos = PHYSFS_swapULE32(tocPos);
    
    /* seek to the table of contents */
    BAIL_IF_MACRO(!io->seek(io, tocPos), ERRPASS, NULL);

    entries = slbLoadEntries(io, count);
    BAIL_IF_MACRO(!entries, ERRPASS, NULL);

    return UNPK_openArchive(io, entries, count);
} /* SLB_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_SLB =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "SLB",
        "I-War / Independence War Slab file",
        "Aleksi Nurmi <aleksi.nurmi@gmail.com>",
        "http://bitbucket.org/ahnurmi/",
    },
    SLB_openArchive,        /* openArchive() method    */
    UNPK_enumerateFiles,    /* enumerateFiles() method */
    UNPK_openRead,          /* openRead() method       */
    UNPK_openWrite,         /* openWrite() method      */
    UNPK_openAppend,        /* openAppend() method     */
    UNPK_remove,            /* remove() method         */
    UNPK_mkdir,             /* mkdir() method          */
    UNPK_closeArchive,      /* closeArchive() method   */
    UNPK_stat               /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_SLB */

/* end of archiver_slb.c ... */

