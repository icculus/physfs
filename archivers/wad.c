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
 * Please see the file LICENSE in the source's root directory.
 *
 * This file written by Travis Wells, based on the GRP archiver by
 *  Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

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
    char *filename;
    PHYSFS_sint64 last_mod_time;
    PHYSFS_uint32 entryCount;
    PHYSFS_uint32 entryOffset;
    WADentry *entries;
} WADinfo;

typedef struct
{
    void *handle;
    WADentry *entry;
    PHYSFS_uint32 curPos;
} WADfileinfo;


static void WAD_dirClose(DirHandle *h);
static PHYSFS_sint64 WAD_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static PHYSFS_sint64 WAD_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static int WAD_eof(FileHandle *handle);
static PHYSFS_sint64 WAD_tell(FileHandle *handle);
static int WAD_seek(FileHandle *handle, PHYSFS_uint64 offset);
static PHYSFS_sint64 WAD_fileLength(FileHandle *handle);
static int WAD_fileClose(FileHandle *handle);
static int WAD_isArchive(const char *filename, int forWriting);
static DirHandle *WAD_openArchive(const char *name, int forWriting);
static LinkedStringList *WAD_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks);
static int WAD_exists(DirHandle *h, const char *name);
static int WAD_isDirectory(DirHandle *h, const char *name, int *fileExists);
static int WAD_isSymLink(DirHandle *h, const char *name, int *fileExists);
static PHYSFS_sint64 WAD_getLastModTime(DirHandle *h, const char *n, int *e);
static FileHandle *WAD_openRead(DirHandle *h, const char *name, int *exist);
static FileHandle *WAD_openWrite(DirHandle *h, const char *name);
static FileHandle *WAD_openAppend(DirHandle *h, const char *name);
static int WAD_remove(DirHandle *h, const char *name);
static int WAD_mkdir(DirHandle *h, const char *name);

const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_WAD =
{
    "WAD",
    /* !!! FIXME WAD_ARCHIVE_DESCRIPTION, */ "DOOM engine format",
    "Travis Wells <traviswells@mchsi.com>",
    "http://www.3dmm2.com/doom/",
};


static const FileFunctions __PHYSFS_FileFunctions_WAD =
{
    WAD_read,       /* read() method       */
    WAD_write,      /* write() method      */
    WAD_eof,        /* eof() method        */
    WAD_tell,       /* tell() method       */
    WAD_seek,       /* seek() method       */
    WAD_fileLength, /* fileLength() method */
    WAD_fileClose   /* fileClose() method  */
};


const DirFunctions __PHYSFS_DirFunctions_WAD =
{
    &__PHYSFS_ArchiveInfo_WAD,
    WAD_isArchive,          /* isArchive() method      */
    WAD_openArchive,        /* openArchive() method    */
    WAD_enumerateFiles,     /* enumerateFiles() method */
    WAD_exists,             /* exists() method         */
    WAD_isDirectory,        /* isDirectory() method    */
    WAD_isSymLink,          /* isSymLink() method      */
    WAD_getLastModTime,     /* getLastModTime() method */
    WAD_openRead,           /* openRead() method       */
    WAD_openWrite,          /* openWrite() method      */
    WAD_openAppend,         /* openAppend() method     */
    WAD_remove,             /* remove() method         */
    WAD_mkdir,              /* mkdir() method          */
    WAD_dirClose            /* dirClose() method       */
};



static void WAD_dirClose(DirHandle *h)
{
    WADinfo *info = ((WADinfo *) h->opaque);
    free(info->filename);
    free(info->entries);
    free(info);
    free(h);
} /* WAD_dirClose */


static PHYSFS_sint64 WAD_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    WADfileinfo *finfo = (WADfileinfo *) (handle->opaque);
    WADentry *entry = finfo->entry;
    PHYSFS_uint32 bytesLeft = entry->size - finfo->curPos;
    PHYSFS_uint32 objsLeft = (bytesLeft / objSize);
    PHYSFS_sint64 rc;

    if (objsLeft < objCount)
        objCount = objsLeft;

    rc = __PHYSFS_platformRead(finfo->handle, buffer, objSize, objCount);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) (rc * objSize);

    return(rc);
} /* WAD_read */


static PHYSFS_sint64 WAD_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* WAD_write */


static int WAD_eof(FileHandle *handle)
{
    WADfileinfo *finfo = (WADfileinfo *) (handle->opaque);
    WADentry *entry = finfo->entry;
    return(finfo->curPos >= entry->size);
} /* WAD_eof */


static PHYSFS_sint64 WAD_tell(FileHandle *handle)
{
    return(((WADfileinfo *) (handle->opaque))->curPos);
} /* WAD_tell */


static int WAD_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    WADfileinfo *finfo = (WADfileinfo *) (handle->opaque);
    WADentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(offset >= entry->size, ERR_PAST_EOF, 0);
    rc = __PHYSFS_platformSeek(finfo->handle, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return(rc);
} /* WAD_seek */


static PHYSFS_sint64 WAD_fileLength(FileHandle *handle)
{
    WADfileinfo *finfo = ((WADfileinfo *) handle->opaque);
    return((PHYSFS_sint64) finfo->entry->size);
} /* WAD_fileLength */


static int WAD_fileClose(FileHandle *handle)
{
    WADfileinfo *finfo = ((WADfileinfo *) handle->opaque);
    BAIL_IF_MACRO(!__PHYSFS_platformClose(finfo->handle), NULL, 0);
    free(finfo);
    free(handle);
    return(1);
} /* WAD_fileClose */


static int wad_open(const char *filename, int forWriting,
                    void **fh, PHYSFS_uint32 *count,PHYSFS_uint32 *offset)
{
    PHYSFS_uint8 buf[4];

    *fh = NULL;
    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    *fh = __PHYSFS_platformOpenRead(filename);
    BAIL_IF_MACRO(*fh == NULL, NULL, 0);
    
    if (__PHYSFS_platformRead(*fh, buf, 4, 1) != 1)
        goto openWad_failed;

    if (memcmp(buf, "IWAD", 4) != 0 && memcmp(buf, "PWAD", 4) != 0)
    {
        __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
        goto openWad_failed;
    } /* if */

    if (__PHYSFS_platformRead(*fh, count, sizeof (PHYSFS_uint32), 1) != 1)
        goto openWad_failed;

    *count = PHYSFS_swapULE32(*count);

    if (__PHYSFS_platformRead(*fh, offset, sizeof (PHYSFS_uint32), 1) != 1)
        goto openWad_failed;

    *offset = PHYSFS_swapULE32(*offset);

    return(1);

openWad_failed:
    if (*fh != NULL)
        __PHYSFS_platformClose(*fh);

    *count = -1;
    *fh = NULL;
    return(0);
} /* wad_open */


static int WAD_isArchive(const char *filename, int forWriting)
{
    void *fh;
    PHYSFS_uint32 fileCount,offset;
    int retval = wad_open(filename, forWriting, &fh, &fileCount,&offset);

    if (fh != NULL)
        __PHYSFS_platformClose(fh);

    return(retval);
} /* WAD_isArchive */


static int wad_entry_cmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    WADentry *a = (WADentry *) _a;
    return(strcmp(a[one].name, a[two].name));
} /* wad_entry_cmp */


static void wad_entry_swap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    WADentry tmp;
    WADentry *first = &(((WADentry *) _a)[one]);
    WADentry *second = &(((WADentry *) _a)[two]);
    memcpy(&tmp, first, sizeof (WADentry));
    memcpy(first, second, sizeof (WADentry));
    memcpy(second, &tmp, sizeof (WADentry));
} /* wad_entry_swap */


static int wad_load_entries(const char *name, int forWriting, WADinfo *info)
{
    void *fh = NULL;
    PHYSFS_uint32 fileCount;
    PHYSFS_uint32 directoryOffset;
    WADentry *entry;
    char lastDirectory[9];

    lastDirectory[8] = 0; /* Make sure lastDirectory stays null-terminated. */

    BAIL_IF_MACRO(!wad_open(name, forWriting, &fh, &fileCount,&directoryOffset), NULL, 0);
    info->entryCount = fileCount;
    info->entries = (WADentry *) malloc(sizeof (WADentry) * fileCount);
    if (info->entries == NULL)
    {
        __PHYSFS_platformClose(fh);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, 0);
    } /* if */

    __PHYSFS_platformSeek(fh,directoryOffset);

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        if (__PHYSFS_platformRead(fh, &entry->startPos, 4, 1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */
        
        if (__PHYSFS_platformRead(fh, &entry->size, 4, 1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        if (__PHYSFS_platformRead(fh, &entry->name, 8, 1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        entry->name[8] = '\0'; /* name might not be null-terminated in file. */
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = PHYSFS_swapULE32(entry->startPos);
    } /* for */

    __PHYSFS_platformClose(fh);

    __PHYSFS_sort(info->entries, info->entryCount,
                  wad_entry_cmp, wad_entry_swap);
    return(1);
} /* wad_load_entries */


static DirHandle *WAD_openArchive(const char *name, int forWriting)
{
    WADinfo *info;
    DirHandle *retval = malloc(sizeof (DirHandle));
    PHYSFS_sint64 modtime = __PHYSFS_platformGetLastModTime(name);

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    info = retval->opaque = malloc(sizeof (WADinfo));
    if (info == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto WAD_openArchive_failed;
    } /* if */

    memset(info, '\0', sizeof (WADinfo));

    info->filename = (char *) malloc(strlen(name) + 1);
    if (info->filename == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto WAD_openArchive_failed;
    } /* if */

    if (!wad_load_entries(name, forWriting, info))
        goto WAD_openArchive_failed;

    strcpy(info->filename, name);
    info->last_mod_time = modtime;
    retval->funcs = &__PHYSFS_DirFunctions_WAD;
    return(retval);

WAD_openArchive_failed:
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
} /* WAD_openArchive */


static LinkedStringList *WAD_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks)
{
    WADinfo *info = ((WADinfo *) h->opaque);
    WADentry *entry = info->entries;
    LinkedStringList *retval = NULL, *p = NULL;
    PHYSFS_uint32 max = info->entryCount;
    PHYSFS_uint32 i;
    char *sep;

    if (dirname[0] == 0)
    {
        for (i = 0; i < max; i++, entry++)
        {
            if (strchr(entry->name, '/') == NULL)
            {
                retval = __PHYSFS_addToLinkedStringList(retval, &p,
                                                        entry->name, -1);
            } /* if */
        } /* for */
    } /* if */
    else
    {
        for (i = 0; i < max; i++, entry++)
        {
            sep = strchr(entry->name, '/');
            if (sep != NULL)
            {
                if (strncmp(dirname, entry->name, (sep-entry->name)) == 0)
                {
                    retval = __PHYSFS_addToLinkedStringList(retval, &p,
                                                            sep + 1, -1);
                } /* if */
            } /* if */
        } /* for */
    } /* else */
    
    return(retval);
} /* WAD_enumerateFiles */


static WADentry *wad_find_entry(WADinfo *info, const char *name)
{
    WADentry *a = info->entries;
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (info->entryCount - 1);
    PHYSFS_sint32 middle;
    int rc;

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        rc = strcmp(name, a[middle].name);
        if (rc == 0)  /* found it! */
            return(&a[middle]);
        else if (rc > 0)
            lo = middle + 1;
        else
            hi = middle - 1;
    } /* while */

    BAIL_MACRO(ERR_NO_SUCH_FILE, NULL);
} /* wad_find_entry */


static int WAD_exists(DirHandle *h, const char *name)
{
    return(wad_find_entry(((WADinfo *) h->opaque), name) != NULL);
} /* WAD_exists */


static int WAD_isDirectory(DirHandle *h, const char *name, int *fileExists)
{
    WADentry *entry = wad_find_entry(((WADinfo *) h->opaque), name);
    if (entry != NULL)
    {
        char *n;

        *fileExists = 1;

        /* Can't be a directory if it's a subdirectory. */
        if (strchr(entry->name, '/') != NULL)
            return(0);

        /* Check if it matches "MAP??" or "E?M?" ... */
        n = entry->name;
        if ((n[0] == 'E' && n[2] == 'M') ||
            (n[0] == 'M' && n[1] == 'A' && n[2] == 'P' && n[6] == 0))
        {
            return(1);
        } /* if */
        return(0);
    } /* if */
    else
    {
        *fileExists = 0;
        return(0);
    } /* else */
} /* WAD_isDirectory */


static int WAD_isSymLink(DirHandle *h, const char *name, int *fileExists)
{
    *fileExists = WAD_exists(h, name);
    return(0);  /* never symlinks in a wad. */
} /* WAD_isSymLink */


static PHYSFS_sint64 WAD_getLastModTime(DirHandle *h,
                                        const char *name,
                                        int *fileExists)
{
    WADinfo *info = ((WADinfo *) h->opaque);
    PHYSFS_sint64 retval = -1;

    *fileExists = (wad_find_entry(info, name) != NULL);
    if (*fileExists)  /* use time of WAD itself in the physical filesystem. */
        retval = ((WADinfo *) h->opaque)->last_mod_time;

    return(retval);
} /* WAD_getLastModTime */


static FileHandle *WAD_openRead(DirHandle *h, const char *fnm, int *fileExists)
{
    WADinfo *info = ((WADinfo *) h->opaque);
    FileHandle *retval;
    WADfileinfo *finfo;
    WADentry *entry;

    entry = wad_find_entry(info, fnm);
    *fileExists = (entry != NULL);
    BAIL_IF_MACRO(entry == NULL, NULL, NULL);

    retval = (FileHandle *) malloc(sizeof (FileHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    finfo = (WADfileinfo *) malloc(sizeof (WADfileinfo));
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
    retval->funcs = &__PHYSFS_FileFunctions_WAD;
    retval->dirHandle = h;
    return(retval);
} /* WAD_openRead */


static FileHandle *WAD_openWrite(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* WAD_openWrite */


static FileHandle *WAD_openAppend(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* WAD_openAppend */


static int WAD_remove(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* WAD_remove */


static int WAD_mkdir(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* WAD_mkdir */

#endif  /* defined PHYSFS_SUPPORTS_WAD */

/* end of wad.c ... */

