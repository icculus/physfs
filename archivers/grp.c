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
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined PHYSFS_SUPPORTS_GRP)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

/*
 * When sorting the grp entries in an archive, we use a modified QuickSort.
 *  When there are less then GRP_QUICKSORT_THRESHOLD entries left to sort,
 *  we switch over to an InsertionSort for the remainder. Tweak to taste.
 */
#define GRP_QUICKSORT_THRESHOLD 4

typedef struct
{
    char name[13];
    PHYSFS_uint64 startPos;
    PHYSFS_uint64 size;
} GRPentry;

typedef struct
{
    char *filename;
    PHYSFS_sint64 last_mod_time;
    PHYSFS_uint32 entryCount;
    GRPentry *entries;
} GRPinfo;

typedef struct
{
    void *handle;
    GRPentry *entry;
    PHYSFS_sint64 curPos;
} GRPfileinfo;


static void GRP_dirClose(DirHandle *h);
static PHYSFS_sint64 GRP_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static int GRP_eof(FileHandle *handle);
static PHYSFS_sint64 GRP_tell(FileHandle *handle);
static int GRP_seek(FileHandle *handle, PHYSFS_uint64 offset);
static PHYSFS_sint64 GRP_fileLength(FileHandle *handle);
static int GRP_fileClose(FileHandle *handle);
static int GRP_isArchive(const char *filename, int forWriting);
static DirHandle *GRP_openArchive(const char *name, int forWriting);
static LinkedStringList *GRP_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks);
static int GRP_exists(DirHandle *h, const char *name);
static int GRP_isDirectory(DirHandle *h, const char *name);
static int GRP_isSymLink(DirHandle *h, const char *name);
static PHYSFS_sint64 GRP_getLastModTime(DirHandle *h, const char *name);
static FileHandle *GRP_openRead(DirHandle *h, const char *name);

static const FileFunctions __PHYSFS_FileFunctions_GRP =
{
    GRP_read,       /* read() method       */
    NULL,           /* write() method      */
    GRP_eof,        /* eof() method        */
    GRP_tell,       /* tell() method       */
    GRP_seek,       /* seek() method       */
    GRP_fileLength, /* fileLength() method */
    GRP_fileClose   /* fileClose() method  */
};


const DirFunctions __PHYSFS_DirFunctions_GRP =
{
    GRP_isArchive,          /* isArchive() method      */
    GRP_openArchive,        /* openArchive() method    */
    GRP_enumerateFiles,     /* enumerateFiles() method */
    GRP_exists,             /* exists() method         */
    GRP_isDirectory,        /* isDirectory() method    */
    GRP_isSymLink,          /* isSymLink() method      */
    GRP_getLastModTime,     /* getLastModTime() method */
    GRP_openRead,           /* openRead() method       */
    NULL,                   /* openWrite() method      */
    NULL,                   /* openAppend() method     */
    NULL,                   /* remove() method         */
    NULL,                   /* mkdir() method          */
    GRP_dirClose            /* dirClose() method       */
};

const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_GRP =
{
    "GRP",
    "Build engine Groupfile format",
    "Ryan C. Gordon <icculus@clutteredmind.org>",
    "http://www.icculus.org/physfs/",
};



static void GRP_dirClose(DirHandle *h)
{
    GRPinfo *info = ((GRPinfo *) h->opaque);
    free(info->filename);
    free(info->entries);
    free(info);
    free(h);
} /* GRP_dirClose */


static PHYSFS_sint64 GRP_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    GRPentry *entry = finfo->entry;
    PHYSFS_uint64 bytesLeft = entry->size - finfo->curPos;
    PHYSFS_uint64 objsLeft = (bytesLeft / objSize);
    PHYSFS_sint64 rc;

    if (objsLeft < objCount)
        objCount = (PHYSFS_uint32) objsLeft;

    rc = __PHYSFS_platformRead(finfo->handle, buffer, objSize, objCount);
    if (rc > 0)
        finfo->curPos += (rc * objSize);

    return(rc);
} /* GRP_read */


static int GRP_eof(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    GRPentry *entry = finfo->entry;
    return(finfo->curPos >= (PHYSFS_sint64) entry->size);
} /* GRP_eof */


static PHYSFS_sint64 GRP_tell(FileHandle *handle)
{
    return(((GRPfileinfo *) (handle->opaque))->curPos);
} /* GRP_tell */


static int GRP_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    GRPentry *entry = finfo->entry;
    PHYSFS_uint64 newPos = (entry->startPos + offset);
    int rc;

    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(newPos > entry->startPos + entry->size, ERR_PAST_EOF, 0);
    rc = __PHYSFS_platformSeek(finfo->handle, newPos);
    if (rc)
        finfo->curPos = offset;

    return(rc);
} /* GRP_seek */


static PHYSFS_sint64 GRP_fileLength(FileHandle *handle)
{
    return(((GRPfileinfo *) handle->opaque)->entry->size);
} /* GRP_fileLength */


static int GRP_fileClose(FileHandle *handle)
{
    GRPfileinfo *finfo = ((GRPfileinfo *) handle->opaque);
    BAIL_IF_MACRO(!__PHYSFS_platformClose(finfo->handle), NULL, 0);
    free(finfo);
    free(handle);
    return(1);
} /* GRP_fileClose */


static int grp_open(const char *filename, int forWriting,
                    void **fh, PHYSFS_uint32 *count)
{
    PHYSFS_uint8 buf[12];

    *fh = NULL;
    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    *fh = __PHYSFS_platformOpenRead(filename);
    BAIL_IF_MACRO(*fh == NULL, NULL, 0);
    
    if (__PHYSFS_platformRead(*fh, buf, 12, 1) != 1)
        goto openGrp_failed;

    if (memcmp(buf, "KenSilverman", 12) != 0)
    {
        __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
        goto openGrp_failed;
    } /* if */

    if (__PHYSFS_platformRead(*fh, count, sizeof (PHYSFS_uint32), 1) != 1)
        goto openGrp_failed;

    *count = PHYSFS_swapULE32(*count);

    return(1);

openGrp_failed:
    if (*fh != NULL)
        __PHYSFS_platformClose(*fh);

    *count = -1;
    *fh = NULL;
    return(0);
} /* grp_open */


static int GRP_isArchive(const char *filename, int forWriting)
{
    void *fh;
    PHYSFS_uint32 fileCount;
    int retval = grp_open(filename, forWriting, &fh, &fileCount);

    if (fh != NULL)
        __PHYSFS_platformClose(fh);

    return(retval);
} /* GRP_isArchive */


static void grp_entry_swap(GRPentry *a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    GRPentry tmp;
    memcpy(&tmp, &a[one], sizeof (GRPentry));
    memcpy(&a[one], &a[two], sizeof (GRPentry));
    memcpy(&a[two], &tmp, sizeof (GRPentry));
} /* grp_entry_swap */


static void grp_quick_sort(GRPentry *a, PHYSFS_uint32 lo, PHYSFS_uint32 hi)
{
    PHYSFS_uint32 i;
    PHYSFS_uint32 j;
    GRPentry *v;

	if ((hi - lo) > GRP_QUICKSORT_THRESHOLD)
	{
		i = (hi + lo) / 2;

		if (strcmp(a[lo].name, a[i].name) > 0) grp_entry_swap(a, lo, i);
		if (strcmp(a[lo].name, a[hi].name) > 0) grp_entry_swap(a, lo, hi);
		if (strcmp(a[i].name, a[hi].name) > 0) grp_entry_swap(a, i, hi);

		j = hi - 1;
		grp_entry_swap(a, i, j);
		i = lo;
		v = &a[j];
		while (1)
		{
			while(strcmp(a[++i].name, v->name) < 0) {}
			while(strcmp(a[--j].name, v->name) > 0) {}
			if (j < i)
                break;
			grp_entry_swap(a, i, j);
		} /* while */
		grp_entry_swap(a, i, hi-1);
		grp_quick_sort(a, lo, j);
		grp_quick_sort(a, i+1, hi);
	} /* if */
} /* grp_quick_sort */


static void grp_insertion_sort(GRPentry *a, PHYSFS_uint32 lo, PHYSFS_uint32 hi)
{
    PHYSFS_uint32 i;
    PHYSFS_uint32 j;
    GRPentry tmp;

    for (i = lo + 1; i <= hi; i++)
    {
        memcpy(&tmp, &a[i], sizeof (GRPentry));
        j = i;
        while ((j > lo) && (strcmp(a[j - 1].name, tmp.name) > 0))
        {
            memcpy(&a[j], &a[j - 1], sizeof (GRPentry));
            j--;
        } /* while */
        memcpy(&a[j], &tmp, sizeof (GRPentry));
    } /* for */
} /* grp_insertion_sort */


static void grp_sort_entries(GRPentry *entries, PHYSFS_uint32 max)
{
    /*
     * Fast Quicksort algorithm inspired by code from here:
     *   http://www.cs.ubc.ca/spider/harrison/Java/sorting-demo.html
     */
    if (max <= GRP_QUICKSORT_THRESHOLD)
        grp_quick_sort(entries, 0, max - 1);
	grp_insertion_sort(entries, 0, max - 1);
} /* grp_sort_entries */


static int grp_load_entries(const char *name, int forWriting, GRPinfo *info)
{
    void *fh = NULL;
    PHYSFS_uint32 fileCount;
    PHYSFS_uint32 location = 32;  /* sizeof sig + sizeof 1st file header. */
    GRPentry *entry;
    char *ptr;

    BAIL_IF_MACRO(!grp_open(name, forWriting, &fh, &fileCount), NULL, 0);
    info->entryCount = fileCount;
    info->entries = (GRPentry *) malloc(sizeof (GRPentry) * fileCount);
    if (info->entries == NULL)
    {
        __PHYSFS_platformClose(fh);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, 0);
    } /* if */

    for (entry = info->entries; fileCount > 0; fileCount--, entry++)
    {
        if (__PHYSFS_platformRead(fh, &entry->name, 12, 1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        entry->name[12] = '\0';  /* name isn't null-terminated in file. */
        if ((ptr = strchr(entry->name, ' ')) != NULL)
            *ptr = '\0';  /* trim extra spaces. */

        if (__PHYSFS_platformRead(fh, &entry->size, 4, 1) != 1)
        {
            __PHYSFS_platformClose(fh);
            return(0);
        } /* if */

        entry->size = PHYSFS_swapULE32(entry->size);
        entry->startPos = location;
        location += entry->size + 16;
    } /* for */

    __PHYSFS_platformClose(fh);

    grp_sort_entries(info->entries, info->entryCount);
    return(1);
} /* grp_load_entries */


static DirHandle *GRP_openArchive(const char *name, int forWriting)
{
    GRPinfo *info;
    DirHandle *retval = malloc(sizeof (DirHandle));
    PHYSFS_sint64 modtime = __PHYSFS_platformGetLastModTime(name);

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    info = retval->opaque = malloc(sizeof (GRPinfo));
    if (info == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto GRP_openArchive_failed;
    } /* if */

    memset(info, '\0', sizeof (GRPinfo));

    info->filename = (char *) malloc(strlen(name) + 1);
    if (info->filename == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto GRP_openArchive_failed;
    } /* if */

    if (!grp_load_entries(name, forWriting, info))
        goto GRP_openArchive_failed;

    strcpy(info->filename, name);
    info->last_mod_time = modtime;
    retval->funcs = &__PHYSFS_DirFunctions_GRP;
    return(retval);

GRP_openArchive_failed:
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
} /* GRP_openArchive */


static LinkedStringList *GRP_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks)
{
    GRPinfo *info = ((GRPinfo *) h->opaque);
    GRPentry *entry = info->entries;
    LinkedStringList *retval = NULL, *p = NULL;
    PHYSFS_uint32 max = info->entryCount;
    PHYSFS_uint32 i;

    /* no directories in GRP files. */
    BAIL_IF_MACRO(*dirname != '\0', ERR_NOT_A_DIR, NULL);

    for (i = 0; i < max; i++, entry++)
        retval = __PHYSFS_addToLinkedStringList(retval, &p, entry->name, -1);

    return(retval);
} /* GRP_enumerateFiles */


static GRPentry *grp_find_entry(GRPinfo *info, const char *name)
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
        rc = strcmp(name, a[middle].name);
        if (rc == 0)  /* found it! */
            return(&a[middle]);
        else if (rc > 0)
            lo = middle + 1;
        else
            hi = middle - 1;
    } /* while */

    BAIL_MACRO(ERR_NO_SUCH_FILE, NULL);
} /* grp_find_entry */


static int GRP_exists(DirHandle *h, const char *name)
{
    return(grp_find_entry(((GRPinfo *) h->opaque), name) != NULL);
} /* GRP_exists */


static int GRP_isDirectory(DirHandle *h, const char *name)
{
    return(0);  /* never directories in a groupfile. */
} /* GRP_isDirectory */


static int GRP_isSymLink(DirHandle *h, const char *name)
{
    return(0);  /* never symlinks in a groupfile. */
} /* GRP_isSymLink */


static PHYSFS_sint64 GRP_getLastModTime(DirHandle *h, const char *name)
{
    /* Just return the time of the GRP itself in the physical filesystem. */
    return(((GRPinfo *) h->opaque)->last_mod_time);
} /* GRP_getLastModTime */


static FileHandle *GRP_openRead(DirHandle *h, const char *name)
{
    GRPinfo *info = ((GRPinfo *) h->opaque);
    FileHandle *retval;
    GRPfileinfo *finfo;
    GRPentry *entry;

    entry = grp_find_entry(info, name);
    BAIL_IF_MACRO(entry == NULL, NULL, NULL);

    retval = (FileHandle *) malloc(sizeof (FileHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    finfo = (GRPfileinfo *) malloc(sizeof (GRPfileinfo));
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
    retval->funcs = &__PHYSFS_FileFunctions_GRP;
    retval->dirHandle = h;
    return(retval);
} /* GRP_openRead */

#endif  /* defined PHYSFS_SUPPORTS_GRP */

/* end of grp.c ... */

