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
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined PHYSFS_SUPPORTS_QPAK)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct
{
    char name[56];
    PHYSFS_uint32 startPos;
    PHYSFS_uint32 size;
} QPAKentry;

typedef struct
{
    char *filename;
    PHYSFS_sint64 last_mod_time;
    PHYSFS_uint32 entryCount;
    QPAKentry *entries;
} QPAKinfo;

typedef struct
{
    void *handle;
    QPAKentry *entry;
    PHYSFS_uint32 curPos;
} QPAKfileinfo;

/* Magic numbers... */
#define QPAK_SIGNATURE 0x4b434150   /* "PACK" in ASCII. */


static void QPAK_dirClose(DirHandle *h);
static PHYSFS_sint64 QPAK_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static PHYSFS_sint64 QPAK_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static int QPAK_eof(FileHandle *handle);
static PHYSFS_sint64 QPAK_tell(FileHandle *handle);
static int QPAK_seek(FileHandle *handle, PHYSFS_uint64 offset);
static PHYSFS_sint64 QPAK_fileLength(FileHandle *handle);
static int QPAK_fileClose(FileHandle *handle);
static int QPAK_isArchive(const char *filename, int forWriting);
static DirHandle *QPAK_openArchive(const char *name, int forWriting);
static LinkedStringList *QPAK_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks);
static int QPAK_exists(DirHandle *h, const char *name);
static int QPAK_isDirectory(DirHandle *h, const char *name, int *fileExists);
static int QPAK_isSymLink(DirHandle *h, const char *name, int *fileExists);
static PHYSFS_sint64 QPAK_getLastModTime(DirHandle *h, const char *n, int *e);
static FileHandle *QPAK_openRead(DirHandle *h, const char *name, int *exist);
static FileHandle *QPAK_openWrite(DirHandle *h, const char *name);
static FileHandle *QPAK_openAppend(DirHandle *h, const char *name);
static int QPAK_remove(DirHandle *h, const char *name);
static int QPAK_mkdir(DirHandle *h, const char *name);

const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_QPAK =
{
    "PAK",
    QPAK_ARCHIVE_DESCRIPTION,
    "Ryan C. Gordon <icculus@clutteredmind.org>",
    "http://icculus.org/physfs/",
};


static const FileFunctions __PHYSFS_FileFunctions_QPAK =
{
    QPAK_read,       /* read() method       */
    QPAK_write,      /* write() method      */
    QPAK_eof,        /* eof() method        */
    QPAK_tell,       /* tell() method       */
    QPAK_seek,       /* seek() method       */
    QPAK_fileLength, /* fileLength() method */
    QPAK_fileClose   /* fileClose() method  */
};


const DirFunctions __PHYSFS_DirFunctions_QPAK =
{
    &__PHYSFS_ArchiveInfo_QPAK,
    QPAK_isArchive,          /* isArchive() method      */
    QPAK_openArchive,        /* openArchive() method    */
    QPAK_enumerateFiles,     /* enumerateFiles() method */
    QPAK_exists,             /* exists() method         */
    QPAK_isDirectory,        /* isDirectory() method    */
    QPAK_isSymLink,          /* isSymLink() method      */
    QPAK_getLastModTime,     /* getLastModTime() method */
    QPAK_openRead,           /* openRead() method       */
    QPAK_openWrite,          /* openWrite() method      */
    QPAK_openAppend,         /* openAppend() method     */
    QPAK_remove,             /* remove() method         */
    QPAK_mkdir,              /* mkdir() method          */
    QPAK_dirClose            /* dirClose() method       */
};



static void QPAK_dirClose(DirHandle *h)
{
    QPAKinfo *info = ((QPAKinfo *) h->opaque);
    free(info->filename);
    free(info->entries);
    free(info);
    free(h);
} /* QPAK_dirClose */


static PHYSFS_sint64 QPAK_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) (handle->opaque);
    QPAKentry *entry = finfo->entry;
    PHYSFS_uint32 bytesLeft = entry->size - finfo->curPos;
    PHYSFS_uint32 objsLeft = (bytesLeft / objSize);
    PHYSFS_sint64 rc;

    if (objsLeft < objCount)
        objCount = objsLeft;

    rc = __PHYSFS_platformRead(finfo->handle, buffer, objSize, objCount);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) (rc * objSize);

    return(rc);
} /* QPAK_read */


static PHYSFS_sint64 QPAK_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* QPAK_write */


static int QPAK_eof(FileHandle *handle)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) (handle->opaque);
    QPAKentry *entry = finfo->entry;
    return(finfo->curPos >= entry->size);
} /* QPAK_eof */


static PHYSFS_sint64 QPAK_tell(FileHandle *handle)
{
    return(((QPAKfileinfo *) (handle->opaque))->curPos);
} /* QPAK_tell */


static int QPAK_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) (handle->opaque);
    QPAKentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(offset >= entry->size, ERR_PAST_EOF, 0);
    rc = __PHYSFS_platformSeek(finfo->handle, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return(rc);
} /* QPAK_seek */


static PHYSFS_sint64 QPAK_fileLength(FileHandle *handle)
{
    QPAKfileinfo *finfo = ((QPAKfileinfo *) handle->opaque);
    return((PHYSFS_sint64) finfo->entry->size);
} /* QPAK_fileLength */


static int QPAK_fileClose(FileHandle *handle)
{
    QPAKfileinfo *finfo = ((QPAKfileinfo *) handle->opaque);
    BAIL_IF_MACRO(!__PHYSFS_platformClose(finfo->handle), NULL, 0);
    free(finfo);
    free(handle);
    return(1);
} /* QPAK_fileClose */


static int qpak_open(const char *filename, int forWriting,
                    void **fh, PHYSFS_uint32 *count)
{
    PHYSFS_uint32 buf;

    *fh = NULL;
    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    *fh = __PHYSFS_platformOpenRead(filename);
    BAIL_IF_MACRO(*fh == NULL, NULL, 0);
    
    if (__PHYSFS_platformRead(*fh, &buf, sizeof (PHYSFS_uint32), 1) != 1)
        goto openQpak_failed;

    buf = PHYSFS_swapULE32(buf);
    if (buf != QPAK_SIGNATURE)
    {
        __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
        goto openQpak_failed;
    } /* if */

    if (__PHYSFS_platformRead(*fh, &buf, sizeof (PHYSFS_uint32), 1) != 1)
        goto openQpak_failed;

    buf = PHYSFS_swapULE32(buf);  /* directory table offset. */

    if (__PHYSFS_platformRead(*fh, count, sizeof (PHYSFS_uint32), 1) != 1)
        goto openQpak_failed;

    *count = PHYSFS_swapULE32(*count);

    if ((*count % 64) != 0)  /* corrupted archive? */
    {
        __PHYSFS_setError(ERR_CORRUPTED);
        goto openQpak_failed;
    } /* if */

    if (!__PHYSFS_platformSeek(*fh, buf))
        goto openQpak_failed;

    *count /= 64;
    return(1);

openQpak_failed:
    if (*fh != NULL)
        __PHYSFS_platformClose(*fh);

    *count = -1;
    *fh = NULL;
    return(0);
} /* qpak_open */


static int QPAK_isArchive(const char *filename, int forWriting)
{
    void *fh;
    PHYSFS_uint32 fileCount;
    int retval = qpak_open(filename, forWriting, &fh, &fileCount);

    if (fh != NULL)
        __PHYSFS_platformClose(fh);

    return(retval);
} /* QPAK_isArchive */


static int qpak_entry_cmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    QPAKentry *a = (QPAKentry *) _a;
    return(strcmp(a[one].name, a[two].name));
} /* qpak_entry_cmp */


static void qpak_entry_swap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    QPAKentry tmp;
    QPAKentry *first = &(((QPAKentry *) _a)[one]);
    QPAKentry *second = &(((QPAKentry *) _a)[two]);
    memcpy(&tmp, first, sizeof (QPAKentry));
    memcpy(first, second, sizeof (QPAKentry));
    memcpy(second, &tmp, sizeof (QPAKentry));
} /* qpak_entry_swap */


static int qpak_load_entries(const char *name, int forWriting, QPAKinfo *info)
{
    void *fh = NULL;
    PHYSFS_uint32 fileCount;
    QPAKentry *entry;

    BAIL_IF_MACRO(!qpak_open(name, forWriting, &fh, &fileCount), NULL, 0);
    info->entryCount = fileCount;
    info->entries = (QPAKentry *) malloc(sizeof (QPAKentry) * fileCount);
    if (info->entries == NULL)
    {
        __PHYSFS_platformClose(fh);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, 0);
    } /* if */

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        PHYSFS_uint32 loc;

        if (__PHYSFS_platformRead(fh,&entry->name,sizeof(entry->name),1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        if (__PHYSFS_platformRead(fh,&loc,sizeof(loc),1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        if (__PHYSFS_platformRead(fh,&entry->size,sizeof(entry->size),1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = PHYSFS_swapULE32(loc);
    } /* for */

    __PHYSFS_platformClose(fh);

    __PHYSFS_sort(info->entries, info->entryCount,
                  qpak_entry_cmp, qpak_entry_swap);
    return(1);
} /* qpak_load_entries */


static DirHandle *QPAK_openArchive(const char *name, int forWriting)
{
    QPAKinfo *info;
    DirHandle *retval = malloc(sizeof (DirHandle));
    PHYSFS_sint64 modtime = __PHYSFS_platformGetLastModTime(name);

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    info = retval->opaque = malloc(sizeof (QPAKinfo));
    if (info == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto QPAK_openArchive_failed;
    } /* if */

    memset(info, '\0', sizeof (QPAKinfo));

    info->filename = (char *) malloc(strlen(name) + 1);
    if (info->filename == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto QPAK_openArchive_failed;
    } /* if */

    if (!qpak_load_entries(name, forWriting, info))
        goto QPAK_openArchive_failed;

    strcpy(info->filename, name);
    info->last_mod_time = modtime;
    retval->funcs = &__PHYSFS_DirFunctions_QPAK;
    return(retval);

QPAK_openArchive_failed:
    if (retval != NULL)
    {
        if (retval->opaque != NULL)
        {
            if (info->filename != NULL)
                free(info->filename);
            if (info->entries != NULL)
                free(info->entries);
            free(info);
        } /* if */
        free(retval);
    } /* if */

    return(NULL);
} /* QPAK_openArchive */


static PHYSFS_sint32 qpak_find_start_of_dir(QPAKinfo *info, const char *path,
                                            int stop_on_first_find)
{
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    PHYSFS_uint32 dlen = strlen(path);
    PHYSFS_sint32 retval = -1;
    const char *name;
    int rc;

    if (*path == '\0')  /* root dir? */
        return(0);

    if ((dlen > 0) && (path[dlen - 1] == '/')) /* ignore trailing slash. */
        dlen--;

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        name = info->entries[middle].name;
        rc = strncmp(path, name, dlen);
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
                    return(middle);

                if (name[dlen + 1] == '\0') /* Skip initial dir entry. */
                    return(middle + 1);

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

    return(retval);
} /* qpak_find_start_of_dir */


static LinkedStringList *QPAK_enumerateFiles(DirHandle *h,
                                             const char *dirname,
                                             int omitSymLinks)
{
    QPAKinfo *info = ((QPAKinfo *) h->opaque);
    LinkedStringList *retval = NULL, *p = NULL;
    PHYSFS_sint32 dlen, dlen_inc, max, i;

    i = qpak_find_start_of_dir(info, dirname, 0);
    BAIL_IF_MACRO(i == -1, ERR_NO_SUCH_FILE, NULL);

    dlen = strlen(dirname);
    if ((dlen > 0) && (dirname[dlen - 1] == '/')) /* ignore trailing slash. */
        dlen--;

    dlen_inc = ((dlen > 0) ? 1 : 0) + dlen;
    max = (PHYSFS_sint32) info->entryCount;
    while (i < max)
    {
        char *add;
        char *ptr;
        PHYSFS_sint32 ln;
        char *e = info->entries[i].name;
        if ((dlen) && ((strncmp(e, dirname, dlen) != 0) || (e[dlen] != '/')))
            break;  /* past end of this dir; we're done. */

        add = e + dlen_inc;
        ptr = strchr(add, '/');
        ln = (PHYSFS_sint32) ((ptr) ? ptr-add : strlen(add));
        retval = __PHYSFS_addToLinkedStringList(retval, &p, add, ln);
        ln += dlen_inc;  /* point past entry to children... */

        /* increment counter and skip children of subdirs... */
        while ((++i < max) && (ptr != NULL))
        {
            char *e_new = info->entries[i].name;
            if ((strncmp(e, e_new, ln) != 0) || (e_new[ln] != '/'))
                break;
        } /* while */
    } /* while */

    return(retval);
} /* QPAK_enumerateFiles */


/*
 * This will find the QPAKentry associated with a path in platform-independent
 *  notation. Directories don't have QPAKentries associated with them, but 
 *  (*isDir) will be set to non-zero if a dir was hit.
 */
static QPAKentry *qpak_find_entry(QPAKinfo *info, const char *path, int *isDir)
{
    QPAKentry *a = info->entries;
    PHYSFS_sint32 pathlen = strlen(path);
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    const char *thispath = NULL;
    int rc;

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        thispath = a[middle].name;
        rc = strncmp(path, thispath, pathlen);

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
                    return(NULL);
            } /* if */

            if (thispath[pathlen] == '\0') /* found entry? */
                return(&a[middle]);
            else
                hi = middle - 1;  /* adjust search params, try again. */
        } /* if */
    } /* while */

    if (isDir != NULL)
        *isDir = 0;

    BAIL_MACRO(ERR_NO_SUCH_FILE, NULL);
} /* qpak_find_entry */


static int QPAK_exists(DirHandle *h, const char *name)
{
    int isDir;    
    QPAKinfo *info = (QPAKinfo *) h->opaque;
    QPAKentry *entry = qpak_find_entry(info, name, &isDir);
    return((entry != NULL) || (isDir));
} /* QPAK_exists */


static int QPAK_isDirectory(DirHandle *h, const char *name, int *fileExists)
{
    QPAKinfo *info = (QPAKinfo *) h->opaque;
    int isDir;
    QPAKentry *entry = qpak_find_entry(info, name, &isDir);

    *fileExists = ((isDir) || (entry != NULL));
    if (isDir)
        return(1); /* definitely a dir. */

    BAIL_MACRO(ERR_NO_SUCH_FILE, 0);
} /* QPAK_isDirectory */


static int QPAK_isSymLink(DirHandle *h, const char *name, int *fileExists)
{
    *fileExists = QPAK_exists(h, name);
    return(0);  /* never symlinks in a quake pak. */
} /* QPAK_isSymLink */


static PHYSFS_sint64 QPAK_getLastModTime(DirHandle *h,
                                        const char *name,
                                        int *fileExists)
{
    int isDir;
    QPAKinfo *info = ((QPAKinfo *) h->opaque);
    PHYSFS_sint64 retval = -1;
    QPAKentry *entry = qpak_find_entry(info, name, &isDir);

    *fileExists = ((isDir) || (entry != NULL));
    if (*fileExists)  /* use time of QPAK itself in the physical filesystem. */
        retval = info->last_mod_time;

    return(retval);
} /* QPAK_getLastModTime */


static FileHandle *QPAK_openRead(DirHandle *h, const char *fnm, int *fileExists)
{
    QPAKinfo *info = ((QPAKinfo *) h->opaque);
    FileHandle *retval;
    QPAKfileinfo *finfo;
    QPAKentry *entry;
    int isDir;

    entry = qpak_find_entry(info, fnm, &isDir);
    *fileExists = ((entry != NULL) || (isDir));
    BAIL_IF_MACRO(isDir, ERR_NOT_A_FILE, NULL);
    BAIL_IF_MACRO(entry == NULL, ERR_NO_SUCH_FILE, NULL);

    retval = (FileHandle *) malloc(sizeof (FileHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    finfo = (QPAKfileinfo *) malloc(sizeof (QPAKfileinfo));
    if (finfo == NULL)
    {
        free(retval);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    finfo->handle = __PHYSFS_platformOpenRead(info->filename);
    if ( (finfo->handle == NULL) ||
         (!__PHYSFS_platformSeek(finfo->handle, entry->startPos)) )
    {
        free(finfo);
        free(retval);
        return(NULL);
    } /* if */

    finfo->curPos = 0;
    finfo->entry = entry;
    retval->opaque = (void *) finfo;
    retval->funcs = &__PHYSFS_FileFunctions_QPAK;
    retval->dirHandle = h;
    return(retval);
} /* QPAK_openRead */


static FileHandle *QPAK_openWrite(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* QPAK_openWrite */


static FileHandle *QPAK_openAppend(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* QPAK_openAppend */


static int QPAK_remove(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* QPAK_remove */


static int QPAK_mkdir(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* QPAK_mkdir */

#endif  /* defined PHYSFS_SUPPORTS_QPAK */

/* end of qpak.c ... */

