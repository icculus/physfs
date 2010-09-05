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

#if (defined PHYSFS_SUPPORTS_MVL)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct
{
    char name[13];
    PHYSFS_uint32 startPos;
    PHYSFS_uint32 size;
} MVLentry;

typedef struct
{
    PHYSFS_Io *io;
    PHYSFS_uint32 entryCount;
    MVLentry *entries;
} MVLinfo;

typedef struct
{
    PHYSFS_Io *io;
    MVLentry *entry;
    PHYSFS_uint32 curPos;
} MVLfileinfo;


static inline int readAll(PHYSFS_Io *io, void *buf, const PHYSFS_uint64 len)
{
    return (io->read(io, buf, len) == len);
} /* readAll */


static void MVL_dirClose(dvoid *opaque)
{
    MVLinfo *info = ((MVLinfo *) opaque);
    info->io->destroy(info->io);
    allocator.Free(info->entries);
    allocator.Free(info);
} /* MVL_dirClose */


static PHYSFS_sint64 MVL_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len)
{
    MVLfileinfo *finfo = (MVLfileinfo *) io->opaque;
    const MVLentry *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size-finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) rc;

    return rc;
} /* MVL_read */


static PHYSFS_sint64 MVL_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* MVL_write */


static PHYSFS_sint64 MVL_tell(PHYSFS_Io *io)
{
    return ((MVLfileinfo *) io->opaque)->curPos;
} /* MVL_tell */


static int MVL_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    MVLfileinfo *finfo = (MVLfileinfo *) io->opaque;
    const MVLentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset >= entry->size, ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return rc;
} /* MVL_seek */


static PHYSFS_sint64 MVL_length(PHYSFS_Io *io)
{
    const MVLfileinfo *finfo = (MVLfileinfo *) io->opaque;
    return ((PHYSFS_sint64) finfo->entry->size);
} /* MVL_length */


static PHYSFS_Io *MVL_duplicate(PHYSFS_Io *_io)
{
    MVLfileinfo *origfinfo = (MVLfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    MVLfileinfo *finfo = (MVLfileinfo *) allocator.Malloc(sizeof (MVLfileinfo));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, MVL_duplicate_failed);
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, MVL_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    GOTO_IF_MACRO(io == NULL, NULL, MVL_duplicate_failed);
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

MVL_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* MVL_duplicate */

static int MVL_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void MVL_destroy(PHYSFS_Io *io)
{
    MVLfileinfo *finfo = (MVLfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* MVL_destroy */


static const PHYSFS_Io MVL_Io =
{
    MVL_read,
    MVL_write,
    MVL_seek,
    MVL_tell,
    MVL_length,
    MVL_duplicate,
    MVL_flush,
    MVL_destroy,
    NULL
};


static int mvlEntryCmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        const MVLentry *a = (const MVLentry *) _a;
        return __PHYSFS_stricmpASCII(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* mvlEntryCmp */


static void mvlEntrySwap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        MVLentry tmp;
        MVLentry *first = &(((MVLentry *) _a)[one]);
        MVLentry *second = &(((MVLentry *) _a)[two]);
        memcpy(&tmp, first, sizeof (MVLentry));
        memcpy(first, second, sizeof (MVLentry));
        memcpy(second, &tmp, sizeof (MVLentry));
    } /* if */
} /* mvlEntrySwap */


static int mvl_load_entries(PHYSFS_Io *io, MVLinfo *info)
{
    PHYSFS_uint32 fileCount = info->entryCount;
    PHYSFS_uint32 location = 8;  /* sizeof sig. */
    MVLentry *entry;

    info->entries = (MVLentry *) allocator.Malloc(sizeof(MVLentry)*fileCount);
    BAIL_IF_MACRO(info->entries == NULL, ERR_OUT_OF_MEMORY, 0);

    location += (17 * fileCount);

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        BAIL_IF_MACRO(!readAll(io, &entry->name, 13), NULL, 0);
        BAIL_IF_MACRO(!readAll(io, &entry->size, 4), NULL, 0);
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = location;
        location += entry->size;
    } /* for */

    __PHYSFS_sort(info->entries, info->entryCount, mvlEntryCmp, mvlEntrySwap);
    return 1;
} /* mvl_load_entries */


static void *MVL_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 buf[4];
    MVLinfo *info = NULL;
    PHYSFS_uint32 val = 0;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    BAIL_IF_MACRO(!readAll(io, buf, 4), NULL, NULL);
    if (memcmp(buf, "DMVL", 4) != 0)
        GOTO_MACRO(ERR_NOT_AN_ARCHIVE, MVL_openArchive_failed);

    info = (MVLinfo *) allocator.Malloc(sizeof (MVLinfo));
    GOTO_IF_MACRO(info == NULL, ERR_OUT_OF_MEMORY, MVL_openArchive_failed);
    memset(info, '\0', sizeof (MVLinfo));
    info->io = io;

    GOTO_IF_MACRO(!readAll(io,&val,sizeof(val)), NULL, MVL_openArchive_failed);
    info->entryCount = PHYSFS_swapULE32(val);

    GOTO_IF_MACRO(!mvl_load_entries(io, info), NULL, MVL_openArchive_failed);

    return info;

MVL_openArchive_failed:
    if (info != NULL)
    {
        if (info->entries != NULL)
            allocator.Free(info->entries);
        allocator.Free(info);
    } /* if */

    return NULL;
} /* MVL_openArchive */


static void MVL_enumerateFiles(dvoid *opaque, const char *dname,
                               int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    /* no directories in MVL files. */
    if (*dname == '\0')
    {
        MVLinfo *info = ((MVLinfo *) opaque);
        MVLentry *entry = info->entries;
        PHYSFS_uint32 max = info->entryCount;
        PHYSFS_uint32 i;

        for (i = 0; i < max; i++, entry++)
            cb(callbackdata, origdir, entry->name);
    } /* if */
} /* MVL_enumerateFiles */


static MVLentry *mvl_find_entry(const MVLinfo *info, const char *name)
{
    char *ptr = strchr(name, '.');
    MVLentry *a = info->entries;
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
} /* mvl_find_entry */


static PHYSFS_Io *MVL_openRead(dvoid *opaque, const char *fnm, int *fileExists)
{
    PHYSFS_Io *retval = NULL;
    MVLinfo *info = (MVLinfo *) opaque;
    MVLfileinfo *finfo;
    MVLentry *entry;

    entry = mvl_find_entry(info, fnm);
    *fileExists = (entry != NULL);
    GOTO_IF_MACRO(entry == NULL, NULL, MVL_openRead_failed);

    retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, MVL_openRead_failed);

    finfo = (MVLfileinfo *) allocator.Malloc(sizeof (MVLfileinfo));
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, MVL_openRead_failed);

    finfo->io = info->io->duplicate(info->io);
    GOTO_IF_MACRO(finfo->io == NULL, NULL, MVL_openRead_failed);

    if (!finfo->io->seek(finfo->io, entry->startPos))
        GOTO_MACRO(NULL, MVL_openRead_failed);

    finfo->curPos = 0;
    finfo->entry = entry;

    memcpy(retval, &MVL_Io, sizeof (*retval));
    retval->opaque = finfo;
    return retval;

MVL_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* MVL_openRead */


static PHYSFS_Io *MVL_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* MVL_openWrite */


static PHYSFS_Io *MVL_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* MVL_openAppend */


static int MVL_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* MVL_remove */


static int MVL_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* MVL_mkdir */


static int MVL_stat(dvoid *opaque, const char *filename, int *exists,
                    PHYSFS_Stat *stat)
{
    const MVLinfo *info = (const MVLinfo *) opaque;
    const MVLentry *entry = mvl_find_entry(info, filename);

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
} /* MVL_stat */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_MVL =
{
    "MVL",
    MVL_ARCHIVE_DESCRIPTION,
    "Bradley Bell <btb@icculus.org>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_MVL =
{
    &__PHYSFS_ArchiveInfo_MVL,
    MVL_openArchive,        /* openArchive() method    */
    MVL_enumerateFiles,     /* enumerateFiles() method */
    MVL_openRead,           /* openRead() method       */
    MVL_openWrite,          /* openWrite() method      */
    MVL_openAppend,         /* openAppend() method     */
    MVL_remove,             /* remove() method         */
    MVL_mkdir,              /* mkdir() method          */
    MVL_dirClose,           /* dirClose() method       */
    MVL_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_MVL */

/* end of mvl.c ... */

