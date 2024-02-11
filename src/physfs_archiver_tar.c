/*
 * tar support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 * based on code by Uli Köhler: https://techoverflow.net/2013/03/29/reading-tar-files-in-c/
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_TAR
#include "physfs_tar.h"

static bool TAR_loadEntries(PHYSFS_Io *io, void *arc)
{
	union block zero_block;
	union block current_block;
	PHYSFS_uint64 count = 0;
	bool long_name = false;

	memset(zero_block.buffer, 0, sizeof(BLOCKSIZE));
	memset(current_block.buffer, 0, sizeof(BLOCKSIZE));

	/* read header block until zero-only terminated block */
	for(; __PHYSFS_readAll(io, current_block.buffer, BLOCKSIZE); count++)
	{
		if( memcmp(current_block.buffer, zero_block.buffer, BLOCKSIZE) == 0 )
			return true;

		/* verify magic */
		switch(TAR_magic(&current_block))
		{
			case POSIX_FORMAT:
				TAR_posix_block(io, arc, &current_block, &count, &long_name);
				break;
			case OLDGNU_FORMAT:
				break;
			case GNU_FORMAT:
				break;
			case V7_FORMAT:
				break;
			case USTAR_FORMAT:
				break;
			case STAR_FORMAT:
				break;
			default:
				break;
		}
	}

	return false;
}

static void *TAR_openArchive(PHYSFS_Io *io, const char *name,
                              int forWriting, int *claimed)
{
    void *unpkarc = NULL;
    union block first;
    enum archive_format format;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, first.buffer, BLOCKSIZE), NULL);
    format = TAR_magic(&first);
    io->seek(io, 0);
    *claimed = format == DEFAULT_FORMAT ? 0 : 1;

    unpkarc = UNPK_openArchive(io, 0, 1);
    BAIL_IF_ERRPASS(!unpkarc, NULL);

    if (!TAR_loadEntries(io, unpkarc))
    {
        UNPK_abandonArchive(unpkarc);
        return NULL;
    } /* if */


    return unpkarc;
} /* TAR_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_TAR =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "TAR",
        "POSIX tar archives / Chasm: the Rift Remastered",
        "Jon Daniel <jondaniel879@gmail.com>",
	"http://www.gnu.org/software/tar/",
        0,
    },
    TAR_openArchive,
    UNPK_enumerate,
    UNPK_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
    UNPK_stat,
    UNPK_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_TAR */

/* end of physfs_archiver_tar.c ... */

