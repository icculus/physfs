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

#if (defined PHYSFS_SUPPORTS_QPAK)

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

/* !!! FIXME: what is this here for? */
#if 1  /* Make this case insensitive? */
#define QPAK_strcmp(x, y) __PHYSFS_stricmpASCII(x, y)
#define QPAK_strncmp(x, y, z) __PHYSFS_strnicmpASCII(x, y, z)
#else
#define QPAK_strcmp(x, y) strcmp(x, y)
#define QPAK_strncmp(x, y, z) strncmp(x, y, z)
#endif


typedef struct
{
    char name[56];
    PHYSFS_uint32 startPos;
    PHYSFS_uint32 size;
} QPAKentry;

typedef struct
{
    PHYSFS_Io *io;
    PHYSFS_uint32 entryCount;
    QPAKentry *entries;
} QPAKinfo;

typedef struct
{
    PHYSFS_Io *io;
    QPAKentry *entry;
    PHYSFS_uint32 curPos;
} QPAKfileinfo;

/* Magic numbers... */
#define QPAK_SIG 0x4b434150   /* "PACK" in ASCII. */


static void QPAK_dirClose(dvoid *opaque)
{
    QPAKinfo *info = ((QPAKinfo *) opaque);
    info->io->destroy(info->io);
    allocator.Free(info->entries);
    allocator.Free(info);
} /* QPAK_dirClose */


static PHYSFS_sint64 QPAK_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) io->opaque;
    const QPAKentry *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size-finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) rc;

    return rc;
} /* QPAK_read */


static PHYSFS_sint64 QPAK_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, -1);
} /* QPAK_write */


static PHYSFS_sint64 QPAK_tell(PHYSFS_Io *io)
{
    return ((QPAKfileinfo *) io->opaque)->curPos;
} /* QPAK_tell */


static int QPAK_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) io->opaque;
    const QPAKentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset >= entry->size, PHYSFS_ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return rc;
} /* QPAK_seek */


static PHYSFS_sint64 QPAK_length(PHYSFS_Io *io)
{
    const QPAKfileinfo *finfo = (QPAKfileinfo *) io->opaque;
    return ((PHYSFS_sint64) finfo->entry->size);
} /* QPAK_length */


static PHYSFS_Io *QPAK_duplicate(PHYSFS_Io *_io)
{
    QPAKfileinfo *origfinfo = (QPAKfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    QPAKfileinfo *finfo = (QPAKfileinfo *) allocator.Malloc(sizeof (QPAKfileinfo));
    GOTO_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, QPAK_duplicate_failed);
    GOTO_IF_MACRO(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, QPAK_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    GOTO_IF_MACRO(!io, ERRPASS, QPAK_duplicate_failed);
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

QPAK_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* QPAK_duplicate */

static int QPAK_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }


static void QPAK_destroy(PHYSFS_Io *io)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* QPAK_destroy */


static const PHYSFS_Io QPAK_Io =
{
    QPAK_read,
    QPAK_write,
    QPAK_seek,
    QPAK_tell,
    QPAK_length,
    QPAK_duplicate,
    QPAK_flush,
    QPAK_destroy,
    NULL
};



static int qpakEntryCmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        const QPAKentry *a = (const QPAKentry *) _a;
        return QPAK_strcmp(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* qpakEntryCmp */


static void qpakEntrySwap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    if (one != two)
    {
        QPAKentry tmp;
        QPAKentry *first = &(((QPAKentry *) _a)[one]);
        QPAKentry *second = &(((QPAKentry *) _a)[two]);
        memcpy(&tmp, first, sizeof (QPAKentry));
        memcpy(first, second, sizeof (QPAKentry));
        memcpy(second, &tmp, sizeof (QPAKentry));
    } /* if */
} /* qpakEntrySwap */


static int qpak_load_entries(QPAKinfo *info)
{
    PHYSFS_Io *io = info->io;
    PHYSFS_uint32 fileCount = info->entryCount;
    QPAKentry *entry;

    info->entries = (QPAKentry*) allocator.Malloc(sizeof(QPAKentry)*fileCount);
    BAIL_IF_MACRO(info->entries == NULL, PHYSFS_ERR_OUT_OF_MEMORY, 0);

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        BAIL_IF_MACRO(!__PHYSFS_readAll(io, &entry->name, 56), ERRPASS, 0);
        BAIL_IF_MACRO(!__PHYSFS_readAll(io, &entry->startPos, 4), ERRPASS, 0);
        BAIL_IF_MACRO(!__PHYSFS_readAll(io, &entry->size, 4), ERRPASS, 0);
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = PHYSFS_swapULE32(entry->startPos);
    } /* for */

    __PHYSFS_sort(info->entries, info->entryCount, qpakEntryCmp, qpakEntrySwap);
    return 1;
} /* qpak_load_entries */


static void *QPAK_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    QPAKinfo *info = NULL;
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

    info = (QPAKinfo *) allocator.Malloc(sizeof (QPAKinfo));
    BAIL_IF_MACRO(info == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    memset(info, '\0', sizeof (QPAKinfo));
    info->io = io;
    info->entryCount = count;

    if (!qpak_load_entries(info))
    {
        if (info->entries != NULL)
            allocator.Free(info->entries);
        allocator.Free(info);
        return NULL;
    } /* if */

    return info;
} /* QPAK_openArchive */


static PHYSFS_sint32 qpak_find_start_of_dir(QPAKinfo *info, const char *path,
                                            int stop_on_first_find)
{
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    PHYSFS_uint32 dlen = (PHYSFS_uint32) strlen(path);
    PHYSFS_sint32 retval = -1;
    const char *name;
    int rc;

    if (*path == '\0')  /* root dir? */
        return 0;

    if ((dlen > 0) && (path[dlen - 1] == '/')) /* ignore trailing slash. */
        dlen--;

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        name = info->entries[middle].name;
        rc = QPAK_strncmp(path, name, dlen);
        if (rc == 0)
        {
            char ch = name[dlen];
            if (ch < '/') /* make sure this isn't just a substr match. */
                rc = -1;
            else if (ch > '/')
                rc = 1;
            else 
            {
                if (stop_on_first_find) /* Just checking dir's existance? */
                    return middle;

                if (name[dlen + 1] == '\0') /* Skip initial dir entry. */
                    return (middle + 1);

                /* there might be more entries earlier in the list. */
                retval = middle;
                hi = middle - 1;
            } /* else */
        } /* if */

        if (rc > 0)
            lo = middle + 1;
        else
            hi = middle - 1;
    } /* while */

    return retval;
} /* qpak_find_start_of_dir */


/*
 * Moved to seperate function so we can use alloca then immediately throw
 *  away the allocated stack space...
 */
static void doEnumCallback(PHYSFS_EnumFilesCallback cb, void *callbackdata,
                           const char *odir, const char *str, PHYSFS_sint32 ln)
{
    char *newstr = __PHYSFS_smallAlloc(ln + 1);
    if (newstr == NULL)
        return;

    memcpy(newstr, str, ln);
    newstr[ln] = '\0';
    cb(callbackdata, odir, newstr);
    __PHYSFS_smallFree(newstr);
} /* doEnumCallback */


static void QPAK_enumerateFiles(dvoid *opaque, const char *dname,
                                int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                                const char *origdir, void *callbackdata)
{
    QPAKinfo *info = ((QPAKinfo *) opaque);
    PHYSFS_sint32 dlen, dlen_inc, max, i;

    i = qpak_find_start_of_dir(info, dname, 0);
    if (i == -1)  /* no such directory. */
        return;

    dlen = (PHYSFS_sint32) strlen(dname);
    if ((dlen > 0) && (dname[dlen - 1] == '/')) /* ignore trailing slash. */
        dlen--;

    dlen_inc = ((dlen > 0) ? 1 : 0) + dlen;
    max = (PHYSFS_sint32) info->entryCount;
    while (i < max)
    {
        char *add;
        char *ptr;
        PHYSFS_sint32 ln;
        char *e = info->entries[i].name;
        if ((dlen) && ((QPAK_strncmp(e, dname, dlen)) || (e[dlen] != '/')))
            break;  /* past end of this dir; we're done. */

        add = e + dlen_inc;
        ptr = strchr(add, '/');
        ln = (PHYSFS_sint32) ((ptr) ? ptr-add : strlen(add));
        doEnumCallback(cb, callbackdata, origdir, add, ln);
        ln += dlen_inc;  /* point past entry to children... */

        /* increment counter and skip children of subdirs... */
        while ((++i < max) && (ptr != NULL))
        {
            char *e_new = info->entries[i].name;
            if ((QPAK_strncmp(e, e_new, ln) != 0) || (e_new[ln] != '/'))
                break;
        } /* while */
    } /* while */
} /* QPAK_enumerateFiles */


/*
 * This will find the QPAKentry associated with a path in platform-independent
 *  notation. Directories don't have QPAKentries associated with them, but 
 *  (*isDir) will be set to non-zero if a dir was hit.
 */
static QPAKentry *qpak_find_entry(const QPAKinfo *info, const char *path,
                                  int *isDir)
{
    QPAKentry *a = info->entries;
    PHYSFS_sint32 pathlen = (PHYSFS_sint32) strlen(path);
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    const char *thispath = NULL;
    int rc;

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        thispath = a[middle].name;
        rc = QPAK_strncmp(path, thispath, pathlen);

        if (rc > 0)
            lo = middle + 1;

        else if (rc < 0)
            hi = middle - 1;

        else /* substring match...might be dir or entry or nothing. */
        {
            if (isDir != NULL)
            {
                *isDir = (thispath[pathlen] == '/');
                if (*isDir)
                    return NULL;
            } /* if */

            if (thispath[pathlen] == '\0') /* found entry? */
                return &a[middle];
            /* adjust search params, try again. */
            else if (thispath[pathlen] > '/')
                hi = middle - 1;
            else
                lo = middle + 1;
        } /* if */
    } /* while */

    if (isDir != NULL)
        *isDir = 0;

    BAIL_MACRO(PHYSFS_ERR_NO_SUCH_PATH, NULL);
} /* qpak_find_entry */


static PHYSFS_Io *QPAK_openRead(dvoid *opaque, const char *fnm, int *fileExists)
{
    PHYSFS_Io *io = NULL;
    QPAKinfo *info = ((QPAKinfo *) opaque);
    QPAKfileinfo *finfo;
    QPAKentry *entry;
    int isDir;

    entry = qpak_find_entry(info, fnm, &isDir);
    *fileExists = ((entry != NULL) || (isDir));
    BAIL_IF_MACRO(isDir, PHYSFS_ERR_NOT_A_FILE, NULL);
    BAIL_IF_MACRO(entry == NULL, PHYSFS_ERR_NO_SUCH_PATH, NULL);

    finfo = (QPAKfileinfo *) allocator.Malloc(sizeof (QPAKfileinfo));
    BAIL_IF_MACRO(finfo == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    finfo->io = info->io->duplicate(info->io);
    GOTO_IF_MACRO(finfo->io == NULL, ERRPASS, QPAK_openRead_failed);
    if (!finfo->io->seek(finfo->io, entry->startPos))
        goto QPAK_openRead_failed;

    finfo->curPos = 0;
    finfo->entry = entry;
    io = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF_MACRO(io == NULL, PHYSFS_ERR_OUT_OF_MEMORY, QPAK_openRead_failed);
    memcpy(io, &QPAK_Io, sizeof (PHYSFS_Io));
    io->opaque = finfo;

    return io;

QPAK_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (io != NULL)
        allocator.Free(io);

    return NULL;
} /* QPAK_openRead */


static PHYSFS_Io *QPAK_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* QPAK_openWrite */


static PHYSFS_Io *QPAK_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* QPAK_openAppend */


static int QPAK_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* QPAK_remove */


static int QPAK_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* QPAK_mkdir */


static int QPAK_stat(dvoid *opaque, const char *filename, int *exists,
                     PHYSFS_Stat *stat)
{
    int isDir = 0;
    const QPAKinfo *info = (const QPAKinfo *) opaque;
    const QPAKentry *entry = qpak_find_entry(info, filename, &isDir);

    if (isDir)
    {
        *exists = 1;
        stat->filetype = PHYSFS_FILETYPE_DIRECTORY;
        stat->filesize = 0;
    } /* if */
    else if (entry != NULL)
    {
        *exists = 1;
        stat->filetype = PHYSFS_FILETYPE_REGULAR;
        stat->filesize = entry->size;
    } /* else if */
    else
    {
        *exists = 0;
        return 0;
    } /* else */

    stat->modtime = -1;
    stat->createtime = -1;
    stat->accesstime = -1;
    stat->readonly = 1;

    return 1;
} /* QPAK_stat */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_QPAK =
{
    "PAK",
    QPAK_ARCHIVE_DESCRIPTION,
    "Ryan C. Gordon <icculus@icculus.org>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_QPAK =
{
    &__PHYSFS_ArchiveInfo_QPAK,
    QPAK_openArchive,        /* openArchive() method    */
    QPAK_enumerateFiles,     /* enumerateFiles() method */
    QPAK_openRead,           /* openRead() method       */
    QPAK_openWrite,          /* openWrite() method      */
    QPAK_openAppend,         /* openAppend() method     */
    QPAK_remove,             /* remove() method         */
    QPAK_mkdir,              /* mkdir() method          */
    QPAK_dirClose,           /* dirClose() method       */
    QPAK_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_QPAK */

/* end of qpak.c ... */

