/*
 * GRP support routines for PhysicsFS.
 *
 * This driver handles BUILD engine archives ("groupfiles"). This format
 *  (but not this driver) was put together by Ken Silverman.
 *
 * The format is simple enough. In Ken's words:
 *
 *    What's the .GRP file format?
 *
 *     The ".grp" file format is just a collection of a lot of files stored
 *     into 1 big one. I tried to make the format as simple as possible: The
 *     first 12 bytes contains my name, "KenSilverman". The next 4 bytes is
 *     the number of files that were compacted into the group file. Then for
 *     each file, there is a 16 byte structure, where the first 12 bytes are
 *     the filename, and the last 4 bytes are the file's size. The rest of
 *     the group file is just the raw data packed one after the other in the
 *     same order as the list of files.
 *
 * (That info is from http://www.advsys.net/ken/build.htm ...)
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if (defined PHYSFS_SUPPORTS_GRP)

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
} GRPentry;

typedef struct
{
    PHYSFS_Io *io;
    PHYSFS_uint32 entryCount;
    GRPentry *entries;
} GRPinfo;

typedef struct
{
    PHYSFS_Io *io;
    GRPentry *entry;
    PHYSFS_uint32 curPos;
} GRPfileinfo;


static inline int readAll(PHYSFS_Io *io, void *buf, const PHYSFS_uint64 len)
{
    return (io->read(io, buf, len) == len);
} /* readAll */


static void GRP_dirClose(dvoid *opaque)
{
    GRPinfo *info = ((GRPinfo *) opaque);
    info->io->destroy(info->io);
    allocator.Free(info->entries);
    allocator.Free(info);
} /* GRP_dirClose */


static PHYSFS_sint64 GRP_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len)
{
    GRPfileinfo *finfo = (GRPfileinfo *) io->opaque;
    const GRPentry *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size-finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) rc;

    return rc;
} /* GRP_read */


static PHYSFS_sint64 GRP_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* GRP_write */


static PHYSFS_sint64 GRP_tell(PHYSFS_Io *io)
{
    return ((GRPfileinfo *) io->opaque)->curPos;
} /* GRP_tell */


static int GRP_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    GRPfileinfo *finfo = (GRPfileinfo *) io->opaque;
    const GRPentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset >= entry->size, ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return rc;
} /* GRP_seek */


static PHYSFS_sint64 GRP_length(PHYSFS_Io *io)
{
    const GRPfileinfo *finfo = (GRPfileinfo *) io->opaque;
    return ((PHYSFS_sint64) finfo->entry->size);
} /* GRP_length */


static PHYSFS_Io *GRP_duplicate(PHYSFS_Io *_io)
{
    GRPfileinfo *origfinfo = (GRPfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GRPfileinfo *finfo = (GRPfileinfo *) allocator.Malloc(sizeof (GRPfileinfo));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, GRP_duplicate_failed);
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, GRP_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    GOTO_IF_MACRO(io == NULL, NULL, GRP_duplicate_failed);
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

GRP_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* GRP_duplicate */

static int GRP_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void GRP_destroy(PHYSFS_Io *io)
{
    GRPfileinfo *finfo = (GRPfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* GRP_destroy */


static const PHYSFS_Io GRP_Io =
{
    GRP_read,
    GRP_write,
    GRP_seek,
    GRP_tell,
    GRP_length,
    GRP_duplicate,
    GRP_flush,
    GRP_destroy,
    NULL
};



static int grpEntryCmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        const GRPentry *a = (const GRPentry *) _a;
        return __PHYSFS_stricmpASCII(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* grpEntryCmp */


static void grpEntrySwap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        GRPentry tmp;
        GRPentry *first = &(((GRPentry *) _a)[one]);
        GRPentry *second = &(((GRPentry *) _a)[two]);
        memcpy(&tmp, first, sizeof (GRPentry));
        memcpy(first, second, sizeof (GRPentry));
        memcpy(second, &tmp, sizeof (GRPentry));
    } /* if */
} /* grpEntrySwap */


static int grp_load_entries(PHYSFS_Io *io, GRPinfo *info)
{
    PHYSFS_uint32 fileCount = info->entryCount;
    PHYSFS_uint32 location = 16;  /* sizeof sig. */
    GRPentry *entry;
    char *ptr;

    info->entries = (GRPentry *) allocator.Malloc(sizeof(GRPentry)*fileCount);
    BAIL_IF_MACRO(info->entries == NULL, ERR_OUT_OF_MEMORY, 0);

    location += (16 * fileCount);

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        BAIL_IF_MACRO(!readAll(io, &entry->name, 12), NULL, 0);
        BAIL_IF_MACRO(!readAll(io, &entry->size, 4), NULL, 0);
        entry->name[12] = '\0';  /* name isn't null-terminated in file. */
        if ((ptr = strchr(entry->name, ' ')) != NULL)
            *ptr = '\0';  /* trim extra spaces. */

        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = location;
        location += entry->size;
    } /* for */

    __PHYSFS_sort(info->entries, info->entryCount, grpEntryCmp, grpEntrySwap);
    return 1;
} /* grp_load_entries */


static void *GRP_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 buf[12];
    GRPinfo *info = NULL;
    PHYSFS_uint32 val = 0;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    BAIL_IF_MACRO(!readAll(io, buf, sizeof (buf)), NULL, NULL);
    if (memcmp(buf, "KenSilverman", sizeof (buf)) != 0)
        GOTO_MACRO(ERR_NOT_AN_ARCHIVE, GRP_openArchive_failed);

    info = (GRPinfo *) allocator.Malloc(sizeof (GRPinfo));
    GOTO_IF_MACRO(info == NULL, ERR_OUT_OF_MEMORY, GRP_openArchive_failed);
    memset(info, '\0', sizeof (GRPinfo));
    info->io = io;

    GOTO_IF_MACRO(!readAll(io,&val,sizeof(val)), NULL, GRP_openArchive_failed);
    info->entryCount = PHYSFS_swapULE32(val);

    GOTO_IF_MACRO(!grp_load_entries(io, info), NULL, GRP_openArchive_failed);

    return info;

GRP_openArchive_failed:
    if (info != NULL)
    {
        if (info->entries != NULL)
            allocator.Free(info->entries);
        allocator.Free(info);
    } /* if */

    return NULL;
} /* GRP_openArchive */


static void GRP_enumerateFiles(dvoid *opaque, const char *dname,
                               int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    /* no directories in GRP files. */
    if (*dname == '\0')
    {
        GRPinfo *info = (GRPinfo *) opaque;
        GRPentry *entry = info->entries;
        PHYSFS_uint32 max = info->entryCount;
        PHYSFS_uint32 i;

        for (i = 0; i < max; i++, entry++)
            cb(callbackdata, origdir, entry->name);
    } /* if */
} /* GRP_enumerateFiles */


static GRPentry *grp_find_entry(const GRPinfo *info, const char *name)
{
    char *ptr = strchr(name, '.');
    GRPentry *a = info->entries;
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
} /* grp_find_entry */


static PHYSFS_Io *GRP_openRead(dvoid *opaque, const char *fnm, int *fileExists)
{
    PHYSFS_Io *retval = NULL;
    GRPinfo *info = (GRPinfo *) opaque;
    GRPfileinfo *finfo;
    GRPentry *entry;

    entry = grp_find_entry(info, fnm);
    *fileExists = (entry != NULL);
    GOTO_IF_MACRO(entry == NULL, NULL, GRP_openRead_failed);

    retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, GRP_openRead_failed);

    finfo = (GRPfileinfo *) allocator.Malloc(sizeof (GRPfileinfo));
    GOTO_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, GRP_openRead_failed);

    finfo->io = info->io->duplicate(info->io);
    GOTO_IF_MACRO(finfo->io == NULL, NULL, GRP_openRead_failed);

    if (!finfo->io->seek(finfo->io, entry->startPos))
        GOTO_MACRO(NULL, GRP_openRead_failed);

    finfo->curPos = 0;
    finfo->entry = entry;

    memcpy(retval, &GRP_Io, sizeof (*retval));
    retval->opaque = finfo;
    return retval;

GRP_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* GRP_openRead */


static PHYSFS_Io *GRP_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* GRP_openWrite */


static PHYSFS_Io *GRP_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* GRP_openAppend */


static int GRP_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* GRP_remove */


static int GRP_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* GRP_mkdir */


static int GRP_stat(dvoid *opaque, const char *filename, int *exists,
                    PHYSFS_Stat *stat)
{
    const GRPinfo *info = (const GRPinfo *) opaque;
    const GRPentry *entry = grp_find_entry(info, filename);

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
} /* GRP_stat */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_GRP =
{
    "GRP",
    GRP_ARCHIVE_DESCRIPTION,
    "Ryan C. Gordon <icculus@icculus.org>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_GRP =
{
    &__PHYSFS_ArchiveInfo_GRP,
    GRP_openArchive,        /* openArchive() method    */
    GRP_enumerateFiles,     /* enumerateFiles() method */
    GRP_openRead,           /* openRead() method       */
    GRP_openWrite,          /* openWrite() method      */
    GRP_openAppend,         /* openAppend() method     */
    GRP_remove,             /* remove() method         */
    GRP_mkdir,              /* mkdir() method          */
    GRP_dirClose,           /* dirClose() method       */
    GRP_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_GRP */

/* end of grp.c ... */

