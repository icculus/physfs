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

#if (defined PHYSFS_SUPPORTS_WAD)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct
{
    char name[18];
    PHYSFS_uint32 startPos;
    PHYSFS_uint32 size;
} WADentry;

typedef struct
{
    PHYSFS_Io *io;
    PHYSFS_uint32 entryCount;
    WADentry *entries;
} WADinfo;

typedef struct
{
    PHYSFS_Io *io;
    WADentry *entry;
    PHYSFS_uint32 curPos;
} WADfileinfo;


static inline int readAll(PHYSFS_Io *io, void *buf, const PHYSFS_uint64 len)
{
    return (io->read(io, buf, len) == len);
} /* readAll */


static void WAD_dirClose(dvoid *opaque)
{
    WADinfo *info = ((WADinfo *) opaque);
    info->io->destroy(info->io);
    allocator.Free(info->entries);
    allocator.Free(info);
} /* WAD_dirClose */


static PHYSFS_sint64 WAD_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len)
{
    WADfileinfo *finfo = (WADfileinfo *) io->opaque;
    const WADentry *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size-finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) rc;

    return rc;
} /* WAD_read */


static PHYSFS_sint64 WAD_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* WAD_write */


static PHYSFS_sint64 WAD_tell(PHYSFS_Io *io)
{
    return ((WADfileinfo *) io->opaque)->curPos;
} /* WAD_tell */


static int WAD_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    WADfileinfo *finfo = (WADfileinfo *) io->opaque;
    const WADentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset >= entry->size, ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return rc;
} /* WAD_seek */


static PHYSFS_sint64 WAD_length(PHYSFS_Io *io)
{
    const WADfileinfo *finfo = (WADfileinfo *) io->opaque;
    return ((PHYSFS_sint64) finfo->entry->size);
} /* WAD_length */


static PHYSFS_Io *WAD_duplicate(PHYSFS_Io *_io)
{
    WADfileinfo *origfinfo = (WADfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    WADfileinfo *finfo = (WADfileinfo *) allocator.Malloc(sizeof (WADfileinfo));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, WAD_duplicate_failed);
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, WAD_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    GOTO_IF_MACRO(io == NULL, NULL, WAD_duplicate_failed);
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

WAD_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* WAD_duplicate */

static int WAD_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void WAD_destroy(PHYSFS_Io *io)
{
    WADfileinfo *finfo = (WADfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* WAD_destroy */


static const PHYSFS_Io WAD_Io =
{
    WAD_read,
    WAD_write,
    WAD_seek,
    WAD_tell,
    WAD_length,
    WAD_duplicate,
    WAD_flush,
    WAD_destroy,
    NULL
};



static int wadEntryCmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        const WADentry *a = (const WADentry *) _a;
        return __PHYSFS_stricmpASCII(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* wadEntryCmp */


static void wadEntrySwap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        WADentry tmp;
        WADentry *first = &(((WADentry *) _a)[one]);
        WADentry *second = &(((WADentry *) _a)[two]);
        memcpy(&tmp, first, sizeof (WADentry));
        memcpy(first, second, sizeof (WADentry));
        memcpy(second, &tmp, sizeof (WADentry));
    } /* if */
} /* wadEntrySwap */


static int wad_load_entries(PHYSFS_Io *io, WADinfo *info)
{
    PHYSFS_uint32 fileCount = info->entryCount;
    PHYSFS_uint32 directoryOffset;
    WADentry *entry;

    BAIL_IF_MACRO(!readAll(io, &directoryOffset, 4), NULL, 0);
    directoryOffset = PHYSFS_swapULE32(directoryOffset);

    info->entries = (WADentry *) allocator.Malloc(sizeof(WADentry)*fileCount);
    BAIL_IF_MACRO(info->entries == NULL, ERR_OUT_OF_MEMORY, 0);
    BAIL_IF_MACRO(!io->seek(io, directoryOffset), NULL, 0);

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        BAIL_IF_MACRO(!readAll(io, &entry->startPos, 4), NULL, 0);
        BAIL_IF_MACRO(!readAll(io, &entry->size, 4), NULL, 0);
        BAIL_IF_MACRO(!readAll(io, &entry->name, 8), NULL, 0);

        entry->name[8] = '\0'; /* name might not be null-terminated in file. */
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = PHYSFS_swapULE32(entry->startPos);
    } /* for */

    __PHYSFS_sort(info->entries, info->entryCount, wadEntryCmp, wadEntrySwap);
    return 1;
} /* wad_load_entries */


static void *WAD_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 buf[4];
    WADinfo *info = NULL;
    PHYSFS_uint32 val = 0;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    BAIL_IF_MACRO(!readAll(io, buf, sizeof (buf)), NULL, NULL);
    if ((memcmp(buf, "IWAD", 4) != 0) && (memcmp(buf, "PWAD", 4) != 0))
        GOTO_MACRO(ERR_NOT_AN_ARCHIVE, WAD_openArchive_failed);

    info = (WADinfo *) allocator.Malloc(sizeof (WADinfo));
    GOTO_IF_MACRO(info == NULL, ERR_OUT_OF_MEMORY, WAD_openArchive_failed);
    memset(info, '\0', sizeof (WADinfo));
    info->io = io;

    GOTO_IF_MACRO(!readAll(io,&val,sizeof(val)), NULL, WAD_openArchive_failed);
    info->entryCount = PHYSFS_swapULE32(val);

    GOTO_IF_MACRO(!wad_load_entries(io, info), NULL, WAD_openArchive_failed);

    return info;

WAD_openArchive_failed:
    if (info != NULL)
    {
        if (info->entries != NULL)
            allocator.Free(info->entries);
        allocator.Free(info);
    } /* if */

    return NULL;
} /* WAD_openArchive */


static void WAD_enumerateFiles(dvoid *opaque, const char *dname,
                               int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    /* no directories in WAD files. */
    if (*dname == '\0')
    {
        WADinfo *info = (WADinfo *) opaque;
        WADentry *entry = info->entries;
        PHYSFS_uint32 max = info->entryCount;
        PHYSFS_uint32 i;

        for (i = 0; i < max; i++, entry++)
            cb(callbackdata, origdir, entry->name);
    } /* if */
} /* WAD_enumerateFiles */


static WADentry *wad_find_entry(const WADinfo *info, const char *name)
{
    char *ptr = strchr(name, '.');
    WADentry *a = info->entries;
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    int rc;

    /*
     * Rule out filenames to avoid unneeded processing...no dirs,
     *   big filenames, or extensions.
     */
    BAIL_IF_MACRO(ptr != NULL, ERR_NO_SUCH_FILE, NULL);
    BAIL_IF_MACRO(strlen(name) > 8, ERR_NO_SUCH_FILE, NULL);
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
} /* wad_find_entry */


static int WAD_exists(dvoid *opaque, const char *name)
{
    return (wad_find_entry((WADinfo *) opaque, name) != NULL);
} /* WAD_exists */


static int WAD_isDirectory(dvoid *opaque, const char *name, int *fileExists)
{
    WADentry *entry = wad_find_entry(((WADinfo *) opaque), name);
    const int exists = (entry != NULL);
    *fileExists = exists;
    if (exists)
    {
        char *n;

        /* Can't be a directory if it's a subdirectory. */
        if (strchr(entry->name, '/') != NULL)
            return 0;

        /* !!! FIXME: this isn't really something we should do. */
        /* !!! FIXME: I probably broke enumeration up there, too. */
        /* Check if it matches "MAP??" or "E?M?" ... */
        n = entry->name;
        if ((n[0] == 'E' && n[2] == 'M') ||
            (n[0] == 'M' && n[1] == 'A' && n[2] == 'P' && n[6] == 0))
        {
            return 1;
        } /* if */
    } /* if */

    return 0;
} /* WAD_isDirectory */


static int WAD_isSymLink(dvoid *opaque, const char *name, int *fileExists)
{
    *fileExists = WAD_exists(opaque, name);
    return 0;  /* never symlinks in a wad. */
} /* WAD_isSymLink */


static PHYSFS_Io *WAD_openRead(dvoid *opaque, const char *fnm, int *fileExists)
{
    PHYSFS_Io *retval = NULL;
    WADinfo *info = (WADinfo *) opaque;
    WADfileinfo *finfo;
    WADentry *entry;

    entry = wad_find_entry(info, fnm);
    *fileExists = (entry != NULL);
    GOTO_IF_MACRO(entry == NULL, NULL, WAD_openRead_failed);

    retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, WAD_openRead_failed);

    finfo = (WADfileinfo *) allocator.Malloc(sizeof (WADfileinfo));
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, WAD_openRead_failed);

    finfo->io = info->io->duplicate(info->io);
    GOTO_IF_MACRO(finfo->io == NULL, NULL, WAD_openRead_failed);

    if (!finfo->io->seek(finfo->io, entry->startPos))
        GOTO_MACRO(NULL, WAD_openRead_failed);

    finfo->curPos = 0;
    finfo->entry = entry;

    memcpy(retval, &WAD_Io, sizeof (*retval));
    retval->opaque = finfo;
    return retval;

WAD_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* WAD_openRead */


static PHYSFS_Io *WAD_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* WAD_openWrite */


static PHYSFS_Io *WAD_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* WAD_openAppend */


static int WAD_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* WAD_remove */


static int WAD_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* WAD_mkdir */


static int WAD_stat(dvoid *opaque, const char *filename, int *exists,
                    PHYSFS_Stat *stat)
{
    const WADinfo *info = (const WADinfo *) opaque;
    const WADentry *entry = wad_find_entry(info, filename);

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
} /* WAD_stat */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_WAD =
{
    "WAD",
    WAD_ARCHIVE_DESCRIPTION,
    "Travis Wells <traviswells@mchsi.com>",
    "http://www.3dmm2.com/doom/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_WAD =
{
    &__PHYSFS_ArchiveInfo_WAD,
    WAD_openArchive,        /* openArchive() method    */
    WAD_enumerateFiles,     /* enumerateFiles() method */
    WAD_exists,             /* exists() method         */
    WAD_isDirectory,        /* isDirectory() method    */
    WAD_isSymLink,          /* isSymLink() method      */
    WAD_openRead,           /* openRead() method       */
    WAD_openWrite,          /* openWrite() method      */
    WAD_openAppend,         /* openAppend() method     */
    WAD_remove,             /* remove() method         */
    WAD_mkdir,              /* mkdir() method          */
    WAD_dirClose,           /* dirClose() method       */
    WAD_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_WAD */

/* end of wad.c ... */

