/*
 * HOG support routines for PhysicsFS.
 *
 * This driver handles Descent I/II HOG archives.
 *
 * The format is very simple:
 *
 *   The file always starts with the 3-byte signature "DHF" (Descent
 *   HOG file). After that the files of a HOG are just attached after
 *   another, divided by a 17 bytes header, which specifies the name
 *   and length (in bytes) of the forthcoming file! So you just read
 *   the header with its information of how big the following file is,
 *   and then skip exact that number of bytes to get to the next file
 *   in that HOG.
 *
 *    char sig[3] = {'D', 'H', 'F'}; // "DHF"=Descent HOG File
 *
 *    struct {
 *     char file_name[13]; // Filename, padded to 13 bytes with 0s
 *     int file_size; // filesize in bytes
 *     char data[file_size]; // The file data
 *    } FILE_STRUCT; // Repeated until the end of the file.
 *
 * (That info is from http://www.descent2.com/ddn/specs/hog/)
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Bradley Bell.
 *  Based on grp.c by Ryan C. Gordon.
 */

#if (defined PHYSFS_SUPPORTS_HOG)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

/*
 * One HOGentry is kept for each file in an open HOG archive.
 */
typedef struct
{
    char name[13];
    PHYSFS_uint32 startPos;
    PHYSFS_uint32 size;
} HOGentry;

/*
 * One HOGinfo is kept for each open HOG archive.
 */
typedef struct
{
    PHYSFS_Io *io;
    PHYSFS_uint32 entryCount;
    HOGentry *entries;
} HOGinfo;

/*
 * One HOGfileinfo is kept for each open file in a HOG archive.
 */
typedef struct
{
    PHYSFS_Io *io;
    HOGentry *entry;
    PHYSFS_uint32 curPos;
} HOGfileinfo;


static inline int readAll(PHYSFS_Io *io, void *buf, const PHYSFS_uint64 len)
{
    return (io->read(io, buf, len) == len);
} /* readAll */


static void HOG_dirClose(dvoid *opaque)
{
    HOGinfo *info = ((HOGinfo *) opaque);
    info->io->destroy(info->io);
    allocator.Free(info->entries);
    allocator.Free(info);
} /* HOG_dirClose */


static PHYSFS_sint64 HOG_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len)
{
    HOGfileinfo *finfo = (HOGfileinfo *) io->opaque;
    const HOGentry *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size-finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) rc;

    return rc;
} /* HOG_read */


static PHYSFS_sint64 HOG_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* HOG_write */


static PHYSFS_sint64 HOG_tell(PHYSFS_Io *io)
{
    return ((HOGfileinfo *) io->opaque)->curPos;
} /* HOG_tell */


static int HOG_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    HOGfileinfo *finfo = (HOGfileinfo *) io->opaque;
    const HOGentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset >= entry->size, ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return rc;
} /* HOG_seek */


static PHYSFS_sint64 HOG_length(PHYSFS_Io *io)
{
    const HOGfileinfo *finfo = (HOGfileinfo *) io->opaque;
    return ((PHYSFS_sint64) finfo->entry->size);
} /* HOG_length */


static PHYSFS_Io *HOG_duplicate(PHYSFS_Io *_io)
{
    HOGfileinfo *origfinfo = (HOGfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    HOGfileinfo *finfo = (HOGfileinfo *) allocator.Malloc(sizeof (HOGfileinfo));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, HOG_duplicate_failed);
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, HOG_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    GOTO_IF_MACRO(io == NULL, NULL, HOG_duplicate_failed);
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

HOG_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* HOG_duplicate */

static int HOG_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void HOG_destroy(PHYSFS_Io *io)
{
    HOGfileinfo *finfo = (HOGfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* HOG_destroy */


static const PHYSFS_Io HOG_Io =
{
    HOG_read,
    HOG_write,
    HOG_seek,
    HOG_tell,
    HOG_length,
    HOG_duplicate,
    HOG_flush,
    HOG_destroy,
    NULL
};



static int hogEntryCmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        const HOGentry *a = (const HOGentry *) _a;
        return __PHYSFS_stricmpASCII(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* hogEntryCmp */


static void hogEntrySwap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        HOGentry tmp;
        HOGentry *first = &(((HOGentry *) _a)[one]);
        HOGentry *second = &(((HOGentry *) _a)[two]);
        memcpy(&tmp, first, sizeof (HOGentry));
        memcpy(first, second, sizeof (HOGentry));
        memcpy(second, &tmp, sizeof (HOGentry));
    } /* if */
} /* hogEntrySwap */


static int hog_load_entries(PHYSFS_Io *io, HOGinfo *info)
{
    const PHYSFS_uint64 iolen = io->length(io);
    PHYSFS_uint32 entCount = 0;
    void *ptr = NULL;
    HOGentry *entry = NULL;
    PHYSFS_uint32 size = 0;
    PHYSFS_uint32 pos = 3;

    while (pos < iolen)
    {
        entCount++;
        ptr = allocator.Realloc(ptr, sizeof (HOGentry) * entCount);
        BAIL_IF_MACRO(ptr == NULL, ERR_OUT_OF_MEMORY, 0);
        info->entries = (HOGentry *) ptr;
        entry = &info->entries[entCount-1];

        BAIL_IF_MACRO(!readAll(io, &entry->name, 13), NULL, 0);
        pos += 13;
        BAIL_IF_MACRO(!readAll(io, &size, 4), NULL, 0);
        pos += 4;

        entry->size = PHYSFS_swapULE32(size);
        entry->startPos = pos;
        pos += size;

        BAIL_IF_MACRO(!io->seek(io, pos), NULL, 0);  /* skip over entry */
    } /* for */

    info->entryCount = entCount;

    __PHYSFS_sort(info->entries, entCount, hogEntryCmp, hogEntrySwap);
    return 1;
} /* hog_load_entries */


static void *HOG_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 buf[3];
    HOGinfo *info = NULL;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    BAIL_IF_MACRO(!readAll(io, buf, 3), NULL, 0);
    if (memcmp(buf, "DHF", 3) != 0)
        GOTO_MACRO(ERR_NOT_AN_ARCHIVE, HOG_openArchive_failed);

    info = (HOGinfo *) allocator.Malloc(sizeof (HOGinfo));
    GOTO_IF_MACRO(info == NULL, ERR_OUT_OF_MEMORY, HOG_openArchive_failed);
    memset(info, '\0', sizeof (HOGinfo));
    info->io = io;

    GOTO_IF_MACRO(!hog_load_entries(io, info), NULL, HOG_openArchive_failed);

    return info;

HOG_openArchive_failed:
    if (info != NULL)
    {
        if (info->entries != NULL)
            allocator.Free(info->entries);
        allocator.Free(info);
    } /* if */

    return NULL;
} /* HOG_openArchive */


static void HOG_enumerateFiles(dvoid *opaque, const char *dname,
                               int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    /* no directories in HOG files. */
    if (*dname == '\0')
    {
        HOGinfo *info = (HOGinfo *) opaque;
        HOGentry *entry = info->entries;
        PHYSFS_uint32 max = info->entryCount;
        PHYSFS_uint32 i;

        for (i = 0; i < max; i++, entry++)
            cb(callbackdata, origdir, entry->name);
    } /* if */
} /* HOG_enumerateFiles */


static HOGentry *hog_find_entry(const HOGinfo *info, const char *name)
{
    char *ptr = strchr(name, '.');
    HOGentry *a = info->entries;
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    int rc;

    /*
     * Rule out filenames to avoid unneeded processing...no dirs,
     *   big filenames, or extensions > 3 chars.
     */
    BAIL_IF_MACRO((ptr) && (strlen(ptr) > 4), ERR_NO_SUCH_FILE, NULL);
    BAIL_IF_MACRO(strlen(name) > 12, ERR_NO_SUCH_FILE, NULL);
    BAIL_IF_MACRO(strchr(name, '/') != NULL, ERR_NO_SUCH_FILE, NULL);

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        rc = __PHYSFS_stricmpASCII(name, a[middle].name);
        if (rc == 0)  /* found it! */
            return &a[middle];
        else if (rc > 0)
            lo = middle + 1;
        else
            hi = middle - 1;
    } /* while */

    BAIL_MACRO(ERR_NO_SUCH_FILE, NULL);
} /* hog_find_entry */


static int HOG_exists(dvoid *opaque, const char *name)
{
    return (hog_find_entry((HOGinfo *) opaque, name) != NULL);
} /* HOG_exists */


static int HOG_isDirectory(dvoid *opaque, const char *name, int *fileExists)
{
    *fileExists = HOG_exists(opaque, name);
    return 0;  /* never directories in a groupfile. */
} /* HOG_isDirectory */


static int HOG_isSymLink(dvoid *opaque, const char *name, int *fileExists)
{
    *fileExists = HOG_exists(opaque, name);
    return 0;  /* never symlinks in a groupfile. */
} /* HOG_isSymLink */


static PHYSFS_Io *HOG_openRead(dvoid *opaque, const char *fnm, int *fileExists)
{
    PHYSFS_Io *retval = NULL;
    HOGinfo *info = (HOGinfo *) opaque;
    HOGfileinfo *finfo;
    HOGentry *entry;

    entry = hog_find_entry(info, fnm);
    *fileExists = (entry != NULL);
    GOTO_IF_MACRO(entry == NULL, NULL, HOG_openRead_failed);

    retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, HOG_openRead_failed);

    finfo = (HOGfileinfo *) allocator.Malloc(sizeof (HOGfileinfo));
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, HOG_openRead_failed);

    finfo->io = info->io->duplicate(info->io);
    GOTO_IF_MACRO(finfo->io == NULL, NULL, HOG_openRead_failed);

    if (!finfo->io->seek(finfo->io, entry->startPos))
        GOTO_MACRO(NULL, HOG_openRead_failed);

    finfo->curPos = 0;
    finfo->entry = entry;

    memcpy(retval, &HOG_Io, sizeof (*retval));
    retval->opaque = finfo;
    return retval;

HOG_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* HOG_openRead */


static PHYSFS_Io *HOG_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* HOG_openWrite */


static PHYSFS_Io *HOG_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* HOG_openAppend */


static int HOG_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* HOG_remove */


static int HOG_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* HOG_mkdir */


static int HOG_stat(dvoid *opaque, const char *filename, int *exists,
                    PHYSFS_Stat *stat)
{
    const HOGinfo *info = (const HOGinfo *) opaque;
    const HOGentry *entry = hog_find_entry(info, filename);

    *exists = (entry != 0);
    if (!entry)
        return 0;

    stat->filesize = entry->size;
    stat->filetype = PHYSFS_FILETYPE_REGULAR;
    stat->modtime = -1;
    stat->createtime = -1;
    stat->accesstime = -1;
    stat->readonly = 1;

    return 1;
} /* HOG_stat */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_HOG =
{
    "HOG",
    HOG_ARCHIVE_DESCRIPTION,
    "Bradley Bell <btb@icculus.org>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_HOG =
{
    &__PHYSFS_ArchiveInfo_HOG,
    HOG_openArchive,        /* openArchive() method    */
    HOG_enumerateFiles,     /* enumerateFiles() method */
    HOG_exists,             /* exists() method         */
    HOG_isDirectory,        /* isDirectory() method    */
    HOG_isSymLink,          /* isSymLink() method      */
    HOG_openRead,           /* openRead() method       */
    HOG_openWrite,          /* openWrite() method      */
    HOG_openAppend,         /* openAppend() method     */
    HOG_remove,             /* remove() method         */
    HOG_mkdir,              /* mkdir() method          */
    HOG_dirClose,           /* dirClose() method       */
    HOG_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_HOG */

/* end of hog.c ... */

