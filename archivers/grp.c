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
 * As it was never a concern in the DOS-based Duke Nukem days, I treat these
 *  archives as CASE-INSENSITIVE. Opening "myfile.txt" and "MYFILE.TXT" both
 *  work, and get the same file.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if (!defined PHYSFS_SUPPORTS_GRP)
#error PHYSFS_SUPPORTS_GRP must be defined.
#endif

typedef struct
{
    void *handle;
    char *filename;
    PHYSFS_uint32 totalEntries;
} GRPinfo;

typedef struct
{
    void *handle;
    PHYSFS_uint64 startPos;
    PHYSFS_uint64 size;
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
    __PHYSFS_platformClose( ((GRPinfo *) h->opaque)->handle );
    free(((GRPinfo *) h->opaque)->filename);
    free(h->opaque);
    free(h);
} /* GRP_dirClose */


static PHYSFS_sint64 GRP_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    void *fh = finfo->handle;
    PHYSFS_sint64 curPos = __PHYSFS_platformTell(fh);
    PHYSFS_uint64 bytesLeft = (finfo->startPos + finfo->size) - curPos;
    PHYSFS_uint32 objsLeft = bytesLeft / objSize;

    if (objsLeft < objCount)
        objCount = objsLeft;

    return(__PHYSFS_platformRead(fh, buffer, objSize, objCount));
} /* GRP_read */


static int GRP_eof(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    void *fh = finfo->handle;
    return(__PHYSFS_platformTell(fh) >= finfo->startPos + finfo->size);
} /* GRP_eof */


static PHYSFS_sint64 GRP_tell(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    return(__PHYSFS_platformTell(finfo->handle) - finfo->startPos);
} /* GRP_tell */


static int GRP_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    int newPos = finfo->startPos + offset;

    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(newPos > finfo->startPos + finfo->size, ERR_PAST_EOF, 0);
    return(__PHYSFS_platformSeek(finfo->handle, newPos));
} /* GRP_seek */


static PHYSFS_sint64 GRP_fileLength(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    return(finfo->size);
} /* GRP_fileLength */


static int GRP_fileClose(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    BAIL_IF_MACRO(__PHYSFS_platformClose(finfo->handle), NULL, 0);

    free(handle->opaque);
    free(handle);
    return(1);
} /* GRP_fileClose */


static int openGrp(const char *filename, int forWriting,
                    void **fh, PHYSFS_sint32 *count)
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

    if (__PHYSFS_platformRead(*fh, count, sizeof (PHYSFS_sint32), 1) != 1)
        goto openGrp_failed;

    return(1);

openGrp_failed:
    if (*fh != NULL)
        __PHYSFS_platformClose(*fh);

    *count = -1;
    *fh = NULL;
    return(0);
} /* openGrp */


static int GRP_isArchive(const char *filename, int forWriting)
{
    void *fh;
    int fileCount;
    int retval = openGrp(filename, forWriting, &fh, &fileCount);

    if (fh != NULL)
        __PHYSFS_platformClose(fh);

    return(retval);
} /* GRP_isArchive */


static DirHandle *GRP_openArchive(const char *name, int forWriting)
{
    void *fh = NULL;
    int fileCount;
    DirHandle *retval = malloc(sizeof (DirHandle));

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    retval->opaque = malloc(sizeof (GRPinfo));
    if (retval->opaque == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto GRP_openArchive_failed;
    } /* if */

    ((GRPinfo *) retval->opaque)->filename = (char *) malloc(strlen(name) + 1);
    if (((GRPinfo *) retval->opaque)->filename == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto GRP_openArchive_failed;
    } /* if */

    if (!openGrp(name, forWriting, &fh, &fileCount))
        goto GRP_openArchive_failed;

    strcpy(((GRPinfo *) retval->opaque)->filename, name);
    ((GRPinfo *) retval->opaque)->handle = fh;
    ((GRPinfo *) retval->opaque)->totalEntries = fileCount;
    retval->funcs = &__PHYSFS_DirFunctions_GRP;
    return(retval);

GRP_openArchive_failed:
    if (retval != NULL)
    {
        if (retval->opaque != NULL)
        {
            if ( ((GRPinfo *) retval->opaque)->filename != NULL )
                free( ((GRPinfo *) retval->opaque)->filename );
            free(retval->opaque);
        } /* if */
        free(retval);
    } /* if */

    if (fh != NULL)
        __PHYSFS_platformClose(fh);

    return(NULL);
} /* GRP_openArchive */


static LinkedStringList *GRP_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks)
{
    PHYSFS_uint8 buf[16];
    GRPinfo *g = (GRPinfo *) (h->opaque);
    void *fh = g->handle;
    int i;
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;

    /* !!! FIXME: Does this consider "/" ? */
    if (*dirname != '\0')   /* no directories in GRP files. */
        return(NULL);

        /* jump to first file entry... */
    BAIL_IF_MACRO(!__PHYSFS_platformSeek(fh, 16), NULL, NULL);

    for (i = 0; i < g->totalEntries; i++)
    {
        BAIL_IF_MACRO(__PHYSFS_platformRead(fh, buf, 16, 1) != 1, NULL, retval);

        buf[12] = '\0';  /* FILENAME.EXT is all you get. */

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l == NULL)
            break;

        l->str = (char *) malloc(strlen((const char *) buf) + 1);
        if (l->str == NULL)
        {
            free(l);
            break;
        } /* if */

        strcpy(l->str, (const char *) buf);

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* for */

    return(retval);
} /* GRP_enumerateFiles */


static PHYSFS_sint32 getFileEntry(DirHandle *h, const char *name,
                                  PHYSFS_uint32 *size)
{
    PHYSFS_uint8 buf[16];
    GRPinfo *g = (GRPinfo *) (h->opaque);
    void *fh = g->handle;
    int i;
    char *ptr;
    int retval = (g->totalEntries + 1) * 16; /* offset of raw file data */

    /* Rule out filenames to avoid unneeded file i/o... */
    if (strchr(name, '/') != NULL)  /* no directories in groupfiles. */
        return(-1);

    ptr = strchr(name, '.');
    if ((ptr) && (strlen(ptr) > 4))   /* 3 char extension at most. */
        return(-1);

    if (strlen(name) > 12)
        return(-1);

    /* jump to first file entry... */
    BAIL_IF_MACRO(!__PHYSFS_platformSeek(fh, 16), NULL, -1);

    for (i = 0; i < g->totalEntries; i++)
    {
        PHYSFS_sint32 l = 0;

        BAIL_IF_MACRO(__PHYSFS_platformRead(fh, buf, 12, 1) != 1, NULL, -1);
        BAIL_IF_MACRO(__PHYSFS_platformRead(fh, &l, sizeof (l), 1) != 1, NULL, -1);

        buf[12] = '\0';  /* FILENAME.EXT is all you get. */

        if (__PHYSFS_platformStricmp((const char *) buf, name) == 0)
        {
            if (size != NULL)
                *size = l;
            return(retval);
        } /* if */

        retval += l;
    } /* for */

    return(-1);  /* not found. */
} /* getFileEntry */


static int GRP_exists(DirHandle *h, const char *name)
{
    return(getFileEntry(h, name, NULL) != -1);
} /* GRP_exists */


static int GRP_isDirectory(DirHandle *h, const char *name)
{
    return(0);  /* never directories in a groupfile. */
} /* GRP_isDirectory */


static int GRP_isSymLink(DirHandle *h, const char *name)
{
    return(0);  /* never symlinks in a groupfile. */
} /* GRP_isSymLink */


static FileHandle *GRP_openRead(DirHandle *h, const char *name)
{
    const char *filename = ((GRPinfo *) h->opaque)->filename;
    FileHandle *retval;
    GRPfileinfo *finfo;
    PHYSFS_uint32 size;
    PHYSFS_sint32 offset;

    offset = getFileEntry(h, name, &size);
    BAIL_IF_MACRO(offset == -1, ERR_NO_SUCH_FILE, NULL);

    retval = (FileHandle *) malloc(sizeof (FileHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    finfo = (GRPfileinfo *) malloc(sizeof (GRPfileinfo));
    if (finfo == NULL)
    {
        free(retval);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    finfo->handle = __PHYSFS_platformOpenRead(filename);
    if ( (finfo->handle == NULL) ||
         (!__PHYSFS_platformSeek(finfo->handle, offset)) )
    {
        free(finfo);
        free(retval);
        return(NULL);
    } /* if */

    finfo->startPos = offset;
    finfo->size = size;
    retval->opaque = (void *) finfo;
    retval->funcs = &__PHYSFS_FileFunctions_GRP;
    retval->dirHandle = h;
    return(retval);
} /* GRP_openRead */

/* end of grp.c ... */

