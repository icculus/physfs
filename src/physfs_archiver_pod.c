/*
 * POD support routines for PhysicsFS.
 *
 * This driver handles Terminal Velocity POD archives.
 *
 * The POD format is as follows:
 *
 *   While there is no known signature, the file starts with an
 *   unsigned 32-bit integer with the file count, followed by
 *   an 80-character, null-padded string with a description of
 *   the pod file. The file entries follow, and are as such:
 *
 *    struct {
 *     char file_path[32]; // Full file path, IE: "ART\VGA.ACT"
 *     uint32_t file_size; // filesize in bytes
 *     uint32_t offset; // The offset of the data.
 *    } fileEntry_t; // Once per file. The file data itself is at offset after all entries.
 *
 * (This info is from: https://moddingwiki.shikadi.net/wiki/POD_Format)
 *
 * There are other POD file formats, but they haven't been implemented yet.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 * This file written by Jordon Moss.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_POD

static int readui32(PHYSFS_Io *io, PHYSFS_uint32 *val)
{
    PHYSFS_uint32 v;
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &v, sizeof (v)), 0);
    *val = PHYSFS_swapULE32(v);
    return 1;
} /* readui32 */

static void replaceChar(char* str, char oldChar, char newChar)
{
    int i = 0;
    while (str[i] != '\0')
    { // Iterate until the null terminator is found
        if (str[i] == oldChar)
        {
            str[i] = newChar; // Replace the character
        }
        i++;
    }
}

static int pod1LoadEntries(PHYSFS_Io *io, void *arc)
{
    PHYSFS_uint32 numfiles;
    PHYSFS_uint32 i;

    BAIL_IF_ERRPASS(!readui32(io, &numfiles), 0);
    BAIL_IF_ERRPASS(!io->seek(io, 84), 0);  /* skip past description */

    for (i = 0; i < numfiles; i++) {
        char name[33];
        PHYSFS_uint32 size;
        PHYSFS_uint32 offset;
        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, name, 32), 0);
        BAIL_IF_ERRPASS(!readui32(io, &size), 0);
        BAIL_IF_ERRPASS(!readui32(io, &offset), 0);
        name[32] = '\0';  /* just in case */

        replaceChar(name, '\\', '/');

        BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 0, -1, -1, offset, size), 0);
    }

    return 1;
} /* pod1LoadEntries */


static void *POD_openArchive(PHYSFS_Io *io, const char *name,
                             int forWriting, int *claimed)
{
    void *unpkarc = NULL;
    PHYSFS_uint32 dummy;
    char description[80];

    assert(io != NULL);  /* shouldn't ever happen. */
    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    BAIL_IF_ERRPASS(!readui32(io, &dummy), 0);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, description, 80), NULL);
    if ((description[0] == 0) || (description[79] != 0)) // Check if we're lacking a description or last char isn't a null terminator.
        return NULL;

    io->seek(io, 0);

    *claimed = 1;

    unpkarc = UNPK_openArchive(io, 0, 1);
    BAIL_IF_ERRPASS(!unpkarc, NULL);

    if (!(pod1LoadEntries(io, unpkarc)))
    {
        UNPK_abandonArchive(unpkarc);
        return NULL;
    } /* if */

    return unpkarc;
} /* POD_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_POD =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "POD",
        "Terminal Reality POD file format",
        "Jordon Moss <mossj32@gmail.com>",
        "https://icculus.org/physfs/",
        0,  /* supportsSymlinks */
    },
    POD_openArchive,
    UNPK_enumerate,
    UNPK_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
    UNPK_stat,
    UNPK_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_POD */

/* end of physfs_archiver_pod.c ... */

