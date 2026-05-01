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
 *    (b) a uint32 which is the number of lumps in the wad
 *    (c) a uint32 which is the file offset to the start of
 *    the directory
 *
 *  The directory has one 16-byte entry for every lump. Each entry consists
 *  of three parts:
 *
 *    (a) a uint32, the file offset to the start of the lump
 *    (b) a uint32, the size of the lump in bytes
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

const char *map_lumps[] = {
    "BEHAVIOR",     /* Hexen */
    "BLOCKMAP",
    "LINEDEFS",
    "NODES",
    "REJECT",
    "SECTORS",
    "SEGS",
    "SIDEDEFS",
    "SSECTORS",
    "THINGS",
    "VERTEXES",
    NULL,
};

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_map_lump(const char *name) {
    const char **lump;
    
    for (lump = map_lumps; *lump; lump++) {
        if (strcmp(name, *lump) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_doom_map_name(const char *name) {
    size_t len = strlen(name);

    if (len == 4 && name[0] == 'E' && is_digit(name[1]) && name[2] == 'M' && is_digit(name [3])) {
        /* ExMy */
        return 1;
    }
    if (len == 5 && strncmp(name, "MAP", 3) == 0 && is_digit(name[3]) && is_digit(name[4])) {
        /* MAPxx */
        return 1;
    }
    return 0;
}

static int wadLoadEntries(PHYSFS_Io *io, const PHYSFS_uint32 count, void *arc)
{
    char path[8 + 1 + 8 + 1];
    size_t parent_pos = 0;
    PHYSFS_uint32 i;

    path[0] = '\0';

    for (i = 0; i < count; i++)
    {
        PHYSFS_uint32 pos;
        PHYSFS_uint32 size;
        int is_directory;
        char name[9];

        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &pos, 4), 0);
        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &size, 4), 0);
        BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, name, 8), 0);

        name[8] = '\0'; /* name might not be null-terminated in file. */
        size = PHYSFS_swapULE32(size);
        pos = PHYSFS_swapULE32(pos);
        is_directory = 0;

        if (size == 0) {
            if (is_doom_map_name(name)) {
                strcpy(path, name);
                parent_pos = strlen(path);
                is_directory = 1;
            } else {
                /* Ignore _START and _END tags */
                continue;
            }
        } else {
            if (parent_pos) {
                if (is_map_lump(name)) {
                    strcat(path, "/");
                    strcpy(path + 1 + parent_pos, name);
                } else {
                    parent_pos = 0;
                    strcpy(path, name);
                }
            } else {
                strcpy(path, name);
            }
        }
        BAIL_IF_ERRPASS(!UNPK_addEntry(arc, path, is_directory, -1, -1, pos, size), 0);
    } /* for */

    return 1;
} /* wadLoadEntries */


static void *WAD_openArchive(PHYSFS_Io *io, const char *name,
                             int forWriting, int *claimed)
{
    PHYSFS_uint8 buf[4];
    PHYSFS_uint32 count;
    PHYSFS_uint32 directoryOffset;
    void *unpkarc;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf, sizeof (buf)), NULL);
    if ((memcmp(buf, "IWAD", 4) != 0) && (memcmp(buf, "PWAD", 4) != 0))
        BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);

    *claimed = 1;

    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &count, sizeof (count)), NULL);
    count = PHYSFS_swapULE32(count);

    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &directoryOffset, 4), NULL);
    directoryOffset = PHYSFS_swapULE32(directoryOffset);

    BAIL_IF_ERRPASS(!io->seek(io, directoryOffset), NULL);

    unpkarc = UNPK_openArchive(io, 0, 1);
    BAIL_IF_ERRPASS(!unpkarc, NULL);

    if (!wadLoadEntries(io, count, unpkarc))
    {
        UNPK_abandonArchive(unpkarc);
        return NULL;
    } /* if */

    return unpkarc;
} /* WAD_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_WAD =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "WAD",
        "DOOM engine format",
        "Travis Wells <traviswells@mchsi.com>",
        "http://www.3dmm2.com/doom/",
        0,  /* supportsSymlinks */
    },
    WAD_openArchive,
    UNPK_enumerate,
    UNPK_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
    UNPK_stat,
    UNPK_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_WAD */

/* end of physfs_archiver_wad.c ... */

