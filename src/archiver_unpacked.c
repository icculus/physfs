/*
 * High-level PhysicsFS archiver for simple unpacked file formats.
 *
 * This is a framework that basic archivers build on top of. It's for simple
 *  formats that can just hand back a list of files and the offsets of their
 *  uncompressed data. There are an alarming number of formats like this.
 *
 * RULES: Archive entries must be uncompressed, must not have subdirs,
 *  must be case insensitive filenames < 32 chars. We can relax some of these
 *  rules as necessary.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct
{
    PHYSFS_Io *io;
    PHYSFS_uint32 entryCount;
    UNPKentry *entries;
} UNPKinfo;


typedef struct
{
    PHYSFS_Io *io;
    UNPKentry *entry;
    PHYSFS_uint32 curPos;
} UNPKfileinfo;


void UNPK_dirClose(dvoid *opaque)
{
    UNPKinfo *info = ((UNPKinfo *) opaque);
    info->io->destroy(info->io);
    allocator.Free(info->entries);
    allocator.Free(info);
} /* UNPK_dirClose */


static PHYSFS_sint64 UNPK_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len)
{
    UNPKfileinfo *finfo = (UNPKfileinfo *) io->opaque;
    const UNPKentry *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size-finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) rc;

    return rc;
} /* UNPK_read */


static PHYSFS_sint64 UNPK_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, -1);
} /* UNPK_write */


static PHYSFS_sint64 UNPK_tell(PHYSFS_Io *io)
{
    return ((UNPKfileinfo *) io->opaque)->curPos;
} /* UNPK_tell */


static int UNPK_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    UNPKfileinfo *finfo = (UNPKfileinfo *) io->opaque;
    const UNPKentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset >= entry->size, PHYSFS_ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return rc;
} /* UNPK_seek */


static PHYSFS_sint64 UNPK_length(PHYSFS_Io *io)
{
    const UNPKfileinfo *finfo = (UNPKfileinfo *) io->opaque;
    return ((PHYSFS_sint64) finfo->entry->size);
} /* UNPK_length */


static PHYSFS_Io *UNPK_duplicate(PHYSFS_Io *_io)
{
    UNPKfileinfo *origfinfo = (UNPKfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    UNPKfileinfo *finfo = (UNPKfileinfo *) allocator.Malloc(sizeof (UNPKfileinfo));
    GOTO_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, UNPK_duplicate_failed);
    GOTO_IF_MACRO(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, UNPK_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    if (!io) goto UNPK_duplicate_failed;
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

UNPK_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* UNPK_duplicate */

static int UNPK_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void UNPK_destroy(PHYSFS_Io *io)
{
    UNPKfileinfo *finfo = (UNPKfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* UNPK_destroy */


static const PHYSFS_Io UNPK_Io =
{
    UNPK_read,
    UNPK_write,
    UNPK_seek,
    UNPK_tell,
    UNPK_length,
    UNPK_duplicate,
    UNPK_flush,
    UNPK_destroy,
    NULL
};


static int entryCmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        const UNPKentry *a = (const UNPKentry *) _a;
        return strcmp(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* entryCmp */


static void entrySwap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        UNPKentry tmp;
        UNPKentry *first = &(((UNPKentry *) _a)[one]);
        UNPKentry *second = &(((UNPKentry *) _a)[two]);
        memcpy(&tmp, first, sizeof (UNPKentry));
        memcpy(first, second, sizeof (UNPKentry));
        memcpy(second, &tmp, sizeof (UNPKentry));
    } /* if */
} /* entrySwap */


void UNPK_enumerateFiles(dvoid *opaque, const char *dname,
                         int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                         const char *origdir, void *callbackdata)
{
    /* no directories in UNPK files. */
    if (*dname == '\0')
    {
        UNPKinfo *info = (UNPKinfo *) opaque;
        UNPKentry *entry = info->entries;
        PHYSFS_uint32 max = info->entryCount;
        PHYSFS_uint32 i;

        for (i = 0; i < max; i++, entry++)
            cb(callbackdata, origdir, entry->name);
    } /* if */
} /* UNPK_enumerateFiles */


static UNPKentry *findEntry(const UNPKinfo *info, const char *name)
{
    UNPKentry *a = info->entries;
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    int rc;

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        rc = __PHYSFS_utf8strcasecmp(name, a[middle].name);
        if (rc == 0)  /* found it! */
            return &a[middle];
        else if (rc > 0)
            lo = middle + 1;
        else
            hi = middle - 1;
    } /* while */

    BAIL_MACRO(PHYSFS_ERR_NO_SUCH_PATH, NULL);
} /* findEntry */


PHYSFS_Io *UNPK_openRead(dvoid *opaque, const char *fnm, int *fileExists)
{
    PHYSFS_Io *retval = NULL;
    UNPKinfo *info = (UNPKinfo *) opaque;
    UNPKfileinfo *finfo = NULL;
    UNPKentry *entry;

    entry = findEntry(info, fnm);
    *fileExists = (entry != NULL);
    GOTO_IF_MACRO(!entry, ERRPASS, UNPK_openRead_failed);

    retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, UNPK_openRead_failed);

    finfo = (UNPKfileinfo *) allocator.Malloc(sizeof (UNPKfileinfo));
    GOTO_IF_MACRO(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, UNPK_openRead_failed);

    finfo->io = info->io->duplicate(info->io);
    GOTO_IF_MACRO(!finfo->io, ERRPASS, UNPK_openRead_failed);

    if (!finfo->io->seek(finfo->io, entry->startPos))
        goto UNPK_openRead_failed;

    finfo->curPos = 0;
    finfo->entry = entry;

    memcpy(retval, &UNPK_Io, sizeof (*retval));
    retval->opaque = finfo;
    return retval;

UNPK_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* UNPK_openRead */


PHYSFS_Io *UNPK_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);
} /* UNPK_openWrite */


PHYSFS_Io *UNPK_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);
} /* UNPK_openAppend */


int UNPK_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, 0);
} /* UNPK_remove */


int UNPK_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, 0);
} /* UNPK_mkdir */


int UNPK_stat(dvoid *opaque, const char *filename, int *exists,
              PHYSFS_Stat *stat)
{
    const UNPKinfo *info = (const UNPKinfo *) opaque;
    const UNPKentry *entry = findEntry(info, filename);

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
} /* UNPK_stat */


dvoid *UNPK_openArchive(PHYSFS_Io *io, UNPKentry *e, const PHYSFS_uint32 num)
{
    UNPKinfo *info = (UNPKinfo *) allocator.Malloc(sizeof (UNPKinfo));
    if (info == NULL)
    {
        allocator.Free(e);
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    __PHYSFS_sort(e, num, entryCmp, entrySwap);
    info->io = io;
    info->entryCount = num;
    info->entries = e;

    return info;
} /* UNPK_openArchive */

/* end of archiver_unpacked.c ... */

