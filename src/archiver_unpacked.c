/*
 * High-level PhysicsFS archiver for simple unpacked file formats.
 *
 * This is a framework that basic archivers build on top of. It's for simple
 *  formats that can just hand back a list of files and the offsets of their
 *  uncompressed data. There are an alarming number of formats like this.
 *
 * RULES: Archive entries must be uncompressed, must not have separate subdir
 *  entries (but can have subdirs), must be case insensitive LOW ASCII
 *  filenames <= 64 bytes. No symlinks, etc. We can relax some of these rules
 *  as necessary.
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


void UNPK_closeArchive(PHYSFS_Dir *opaque)
{
    UNPKinfo *info = ((UNPKinfo *) opaque);
    info->io->destroy(info->io);
    allocator.Free(info->entries);
    allocator.Free(info);
} /* UNPK_closeArchive */


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
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, -1);
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
    CURRENT_PHYSFS_IO_API_VERSION, NULL,
    UNPK_read,
    UNPK_write,
    UNPK_seek,
    UNPK_tell,
    UNPK_length,
    UNPK_duplicate,
    UNPK_flush,
    UNPK_destroy
};


static int entryCmp(void *_a, size_t one, size_t two)
{
    if (one != two)
    {
        const UNPKentry *a = (const UNPKentry *) _a;
        return __PHYSFS_stricmpASCII(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* entryCmp */


static void entrySwap(void *_a, size_t one, size_t two)
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


static PHYSFS_sint32 findStartOfDir(UNPKinfo *info, const char *path,
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
        rc = __PHYSFS_strnicmpASCII(path, name, dlen);
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
} /* findStartOfDir */


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


void UNPK_enumerateFiles(PHYSFS_Dir *opaque, const char *dname,
                         int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                         const char *origdir, void *callbackdata)
{
    UNPKinfo *info = ((UNPKinfo *) opaque);
    PHYSFS_sint32 dlen, dlen_inc, max, i;

    i = findStartOfDir(info, dname, 0);
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
        if ((dlen) &&
            ((__PHYSFS_strnicmpASCII(e, dname, dlen)) || (e[dlen] != '/')))
        {
            break;  /* past end of this dir; we're done. */
        } /* if */

        add = e + dlen_inc;
        ptr = strchr(add, '/');
        ln = (PHYSFS_sint32) ((ptr) ? ptr-add : strlen(add));
        doEnumCallback(cb, callbackdata, origdir, add, ln);
        ln += dlen_inc;  /* point past entry to children... */

        /* increment counter and skip children of subdirs... */
        while ((++i < max) && (ptr != NULL))
        {
            char *e_new = info->entries[i].name;
            if ((__PHYSFS_strnicmpASCII(e, e_new, ln) != 0) ||
                (e_new[ln] != '/'))
            {
                break;
            } /* if */
        } /* while */
    } /* while */
} /* UNPK_enumerateFiles */


/*
 * This will find the UNPKentry associated with a path in platform-independent
 *  notation. Directories don't have UNPKentries associated with them, but 
 *  (*isDir) will be set to non-zero if a dir was hit.
 */
static UNPKentry *findEntry(const UNPKinfo *info, const char *path, int *isDir)
{
    UNPKentry *a = info->entries;
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
        rc = __PHYSFS_strnicmpASCII(path, thispath, pathlen);

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
} /* findEntry */


PHYSFS_Io *UNPK_openRead(PHYSFS_Dir *opaque, const char *fnm, int *fileExists)
{
    PHYSFS_Io *retval = NULL;
    UNPKinfo *info = (UNPKinfo *) opaque;
    UNPKfileinfo *finfo = NULL;
    int isdir = 0;
    UNPKentry *entry = findEntry(info, fnm, &isdir);

    *fileExists = (entry != NULL);
    GOTO_IF_MACRO(isdir, PHYSFS_ERR_NOT_A_FILE, UNPK_openRead_failed);
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


PHYSFS_Io *UNPK_openWrite(PHYSFS_Dir *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* UNPK_openWrite */


PHYSFS_Io *UNPK_openAppend(PHYSFS_Dir *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* UNPK_openAppend */


int UNPK_remove(PHYSFS_Dir *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* UNPK_remove */


int UNPK_mkdir(PHYSFS_Dir *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* UNPK_mkdir */


int UNPK_stat(PHYSFS_Dir *opaque, const char *filename,
              int *exists, PHYSFS_Stat *stat)
{
    int isDir = 0;
    const UNPKinfo *info = (const UNPKinfo *) opaque;
    const UNPKentry *entry = findEntry(info, filename, &isDir);

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
} /* UNPK_stat */


PHYSFS_Dir *UNPK_openArchive(PHYSFS_Io *io, UNPKentry *e,
                             const PHYSFS_uint32 num)
{
    UNPKinfo *info = (UNPKinfo *) allocator.Malloc(sizeof (UNPKinfo));
    if (info == NULL)
    {
        allocator.Free(e);
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    __PHYSFS_sort(e, (size_t) num, entryCmp, entrySwap);
    info->io = io;
    info->entryCount = num;
    info->entries = e;

    return info;
} /* UNPK_openArchive */

/* end of archiver_unpacked.c ... */

