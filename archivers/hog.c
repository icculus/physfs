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
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Bradley Bell.
 *  Based on grp.c by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

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
    char *filename;
    PHYSFS_sint64 last_mod_time;
    PHYSFS_uint32 entryCount;
    HOGentry *entries;
} HOGinfo;

/*
 * One HOGfileinfo is kept for each open file in a HOG archive.
 */
typedef struct
{
    void *handle;
    HOGentry *entry;
    PHYSFS_uint32 curPos;
} HOGfileinfo;


static void HOG_dirClose(DirHandle *h);
static PHYSFS_sint64 HOG_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static PHYSFS_sint64 HOG_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static int HOG_eof(FileHandle *handle);
static PHYSFS_sint64 HOG_tell(FileHandle *handle);
static int HOG_seek(FileHandle *handle, PHYSFS_uint64 offset);
static PHYSFS_sint64 HOG_fileLength(FileHandle *handle);
static int HOG_fileClose(FileHandle *handle);
static int HOG_isArchive(const char *filename, int forWriting);
static DirHandle *HOG_openArchive(const char *name, int forWriting);
static LinkedStringList *HOG_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks);
static int HOG_exists(DirHandle *h, const char *name);
static int HOG_isDirectory(DirHandle *h, const char *name, int *fileExists);
static int HOG_isSymLink(DirHandle *h, const char *name, int *fileExists);
static PHYSFS_sint64 HOG_getLastModTime(DirHandle *h, const char *n, int *e);
static FileHandle *HOG_openRead(DirHandle *h, const char *name, int *exist);
static FileHandle *HOG_openWrite(DirHandle *h, const char *name);
static FileHandle *HOG_openAppend(DirHandle *h, const char *name);
static int HOG_remove(DirHandle *h, const char *name);
static int HOG_mkdir(DirHandle *h, const char *name);

const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_HOG =
{
    "HOG",
    HOG_ARCHIVE_DESCRIPTION,
    "Bradley Bell <btb@icculus.org>",
    "http://icculus.org/physfs/",
};


static const FileFunctions __PHYSFS_FileFunctions_HOG =
{
    HOG_read,       /* read() method       */
    HOG_write,      /* write() method      */
    HOG_eof,        /* eof() method        */
    HOG_tell,       /* tell() method       */
    HOG_seek,       /* seek() method       */
    HOG_fileLength, /* fileLength() method */
    HOG_fileClose   /* fileClose() method  */
};


const DirFunctions __PHYSFS_DirFunctions_HOG =
{
    &__PHYSFS_ArchiveInfo_HOG,
    HOG_isArchive,          /* isArchive() method      */
    HOG_openArchive,        /* openArchive() method    */
    HOG_enumerateFiles,     /* enumerateFiles() method */
    HOG_exists,             /* exists() method         */
    HOG_isDirectory,        /* isDirectory() method    */
    HOG_isSymLink,          /* isSymLink() method      */
    HOG_getLastModTime,     /* getLastModTime() method */
    HOG_openRead,           /* openRead() method       */
    HOG_openWrite,          /* openWrite() method      */
    HOG_openAppend,         /* openAppend() method     */
    HOG_remove,             /* remove() method         */
    HOG_mkdir,              /* mkdir() method          */
    HOG_dirClose            /* dirClose() method       */
};



static void HOG_dirClose(DirHandle *h)
{
    HOGinfo *info = ((HOGinfo *) h->opaque);
    free(info->filename);
    free(info->entries);
    free(info);
    free(h);
} /* HOG_dirClose */


static PHYSFS_sint64 HOG_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    HOGfileinfo *finfo = (HOGfileinfo *) (handle->opaque);
    HOGentry *entry = finfo->entry;
    PHYSFS_uint32 bytesLeft = entry->size - finfo->curPos;
    PHYSFS_uint32 objsLeft = (bytesLeft / objSize);
    PHYSFS_sint64 rc;

    if (objsLeft < objCount)
        objCount = objsLeft;

    rc = __PHYSFS_platformRead(finfo->handle, buffer, objSize, objCount);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32) (rc * objSize);

    return(rc);
} /* HOG_read */


static PHYSFS_sint64 HOG_write(FileHandle *handle, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* HOG_write */


static int HOG_eof(FileHandle *handle)
{
    HOGfileinfo *finfo = (HOGfileinfo *) (handle->opaque);
    HOGentry *entry = finfo->entry;
    return(finfo->curPos >= entry->size);
} /* HOG_eof */


static PHYSFS_sint64 HOG_tell(FileHandle *handle)
{
    return(((HOGfileinfo *) (handle->opaque))->curPos);
} /* HOG_tell */


static int HOG_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    HOGfileinfo *finfo = (HOGfileinfo *) (handle->opaque);
    HOGentry *entry = finfo->entry;
    int rc;

    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(offset >= entry->size, ERR_PAST_EOF, 0);
    rc = __PHYSFS_platformSeek(finfo->handle, entry->startPos + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32) offset;

    return(rc);
} /* HOG_seek */


static PHYSFS_sint64 HOG_fileLength(FileHandle *handle)
{
    HOGfileinfo *finfo = ((HOGfileinfo *) handle->opaque);
    return((PHYSFS_sint64) finfo->entry->size);
} /* HOG_fileLength */


static int HOG_fileClose(FileHandle *handle)
{
    HOGfileinfo *finfo = ((HOGfileinfo *) handle->opaque);
    BAIL_IF_MACRO(!__PHYSFS_platformClose(finfo->handle), NULL, 0);
    free(finfo);
    free(handle);
    return(1);
} /* HOG_fileClose */


static int hog_open(const char *filename, int forWriting,
                    void **fh, PHYSFS_uint32 *count)
{
    PHYSFS_uint8 buf[13];
    PHYSFS_uint32 size;
    PHYSFS_sint64 pos;

    *count = 0;

    *fh = NULL;
    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    *fh = __PHYSFS_platformOpenRead(filename);
    BAIL_IF_MACRO(*fh == NULL, NULL, 0);

    if (__PHYSFS_platformRead(*fh, buf, 3, 1) != 1)
        goto openHog_failed;

    if (memcmp(buf, "DHF", 3) != 0)
    {
        __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
        goto openHog_failed;
    } /* if */

    while( 1 )
    {
        if (__PHYSFS_platformRead(*fh, buf, 13, 1) != 1)
            break;             //eof here is ok

        if (__PHYSFS_platformRead(*fh, &size, 4, 1) != 1)
            goto openHog_failed;

        size = PHYSFS_swapULE32(size);

        (*count)++;

        // Skip over entry
        pos = __PHYSFS_platformTell(*fh);
        if (pos == -1)
            goto openHog_failed;
        if (!__PHYSFS_platformSeek(*fh, pos + size))
            goto openHog_failed;
    }

    // Rewind to start of entries
    if (!__PHYSFS_platformSeek(*fh, 3))
        goto openHog_failed;

    return(1);

openHog_failed:
    if (*fh != NULL)
        __PHYSFS_platformClose(*fh);

    *count = -1;
    *fh = NULL;
    return(0);
} /* hog_open */


static int HOG_isArchive(const char *filename, int forWriting)
{
    void *fh;
    PHYSFS_uint32 fileCount;
    int retval = hog_open(filename, forWriting, &fh, &fileCount);

    if (fh != NULL)
        __PHYSFS_platformClose(fh);

    return(retval);
} /* HOG_isArchive */


static int hog_entry_cmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    HOGentry *a = (HOGentry *) _a;
    return(strcmp(a[one].name, a[two].name));
} /* hog_entry_cmp */


static void hog_entry_swap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    HOGentry tmp;
    HOGentry *first = &(((HOGentry *) _a)[one]);
    HOGentry *second = &(((HOGentry *) _a)[two]);
    memcpy(&tmp, first, sizeof (HOGentry));
    memcpy(first, second, sizeof (HOGentry));
    memcpy(second, &tmp, sizeof (HOGentry));
} /* hog_entry_swap */


static int hog_load_entries(const char *name, int forWriting, HOGinfo *info)
{
    void *fh = NULL;
    PHYSFS_uint32 fileCount;
    HOGentry *entry;

    BAIL_IF_MACRO(!hog_open(name, forWriting, &fh, &fileCount), NULL, 0);
    info->entryCount = fileCount;
    info->entries = (HOGentry *) malloc(sizeof (HOGentry) * fileCount);
    if (info->entries == NULL)
    {
        __PHYSFS_platformClose(fh);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, 0);
    } /* if */

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        if (__PHYSFS_platformRead(fh, &entry->name, 13, 1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        if (__PHYSFS_platformRead(fh, &entry->size, 4, 1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = (unsigned int) __PHYSFS_platformTell(fh);
        if (entry->startPos == -1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        }

        // Skip over entry
        if (!__PHYSFS_platformSeek(fh, entry->startPos + entry->size))
        {
            __PHYSFS_platformClose(fh);
            return(0);
        }
    } /* for */

    __PHYSFS_platformClose(fh);

    __PHYSFS_sort(info->entries, info->entryCount,
                  hog_entry_cmp, hog_entry_swap);
    return(1);
} /* hog_load_entries */


static DirHandle *HOG_openArchive(const char *name, int forWriting)
{
    HOGinfo *info;
    DirHandle *retval = malloc(sizeof (DirHandle));
    PHYSFS_sint64 modtime = __PHYSFS_platformGetLastModTime(name);

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    info = retval->opaque = malloc(sizeof (HOGinfo));
    if (info == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto HOG_openArchive_failed;
    } /* if */

    memset(info, '\0', sizeof (HOGinfo));

    info->filename = (char *) malloc(strlen(name) + 1);
    if (info->filename == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto HOG_openArchive_failed;
    } /* if */

    if (!hog_load_entries(name, forWriting, info))
        goto HOG_openArchive_failed;

    strcpy(info->filename, name);
    info->last_mod_time = modtime;
    retval->funcs = &__PHYSFS_DirFunctions_HOG;
    return(retval);

HOG_openArchive_failed:
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
} /* HOG_openArchive */


static LinkedStringList *HOG_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks)
{
    HOGinfo *info = ((HOGinfo *) h->opaque);
    HOGentry *entry = info->entries;
    LinkedStringList *retval = NULL, *p = NULL;
    PHYSFS_uint32 max = info->entryCount;
    PHYSFS_uint32 i;

    /* no directories in HOG files. */
    BAIL_IF_MACRO(*dirname != '\0', ERR_NOT_A_DIR, NULL);

    for (i = 0; i < max; i++, entry++)
        retval = __PHYSFS_addToLinkedStringList(retval, &p, entry->name, -1);

    return(retval);
} /* HOG_enumerateFiles */


static HOGentry *hog_find_entry(HOGinfo *info, const char *name)
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
        rc = __PHYSFS_platformStricmp(name, a[middle].name);
        if (rc == 0)  /* found it! */
            return(&a[middle]);
        else if (rc > 0)
            lo = middle + 1;
        else
            hi = middle - 1;
    } /* while */

    BAIL_MACRO(ERR_NO_SUCH_FILE, NULL);
} /* hog_find_entry */


static int HOG_exists(DirHandle *h, const char *name)
{
    return(hog_find_entry(((HOGinfo *) h->opaque), name) != NULL);
} /* HOG_exists */


static int HOG_isDirectory(DirHandle *h, const char *name, int *fileExists)
{
    *fileExists = HOG_exists(h, name);
    return(0);  /* never directories in a groupfile. */
} /* HOG_isDirectory */


static int HOG_isSymLink(DirHandle *h, const char *name, int *fileExists)
{
    *fileExists = HOG_exists(h, name);
    return(0);  /* never symlinks in a groupfile. */
} /* HOG_isSymLink */


static PHYSFS_sint64 HOG_getLastModTime(DirHandle *h,
                                        const char *name,
                                        int *fileExists)
{
    HOGinfo *info = ((HOGinfo *) h->opaque);
    PHYSFS_sint64 retval = -1;

    *fileExists = (hog_find_entry(info, name) != NULL);
    if (*fileExists)  /* use time of HOG itself in the physical filesystem. */
        retval = ((HOGinfo *) h->opaque)->last_mod_time;

    return(retval);
} /* HOG_getLastModTime */


static FileHandle *HOG_openRead(DirHandle *h, const char *fnm, int *fileExists)
{
    HOGinfo *info = ((HOGinfo *) h->opaque);
    FileHandle *retval;
    HOGfileinfo *finfo;
    HOGentry *entry;

    entry = hog_find_entry(info, fnm);
    *fileExists = (entry != NULL);
    BAIL_IF_MACRO(entry == NULL, NULL, NULL);

    retval = (FileHandle *) malloc(sizeof (FileHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    finfo = (HOGfileinfo *) malloc(sizeof (HOGfileinfo));
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
    retval->funcs = &__PHYSFS_FileFunctions_HOG;
    retval->dirHandle = h;
    return(retval);
} /* HOG_openRead */


static FileHandle *HOG_openWrite(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* HOG_openWrite */


static FileHandle *HOG_openAppend(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* HOG_openAppend */


static int HOG_remove(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* HOG_remove */


static int HOG_mkdir(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* HOG_mkdir */

#endif  /* defined PHYSFS_SUPPORTS_HOG */

/* end of hog.c ... */

