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
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if (!defined PHYSFS_SUPPORTS_GRP)
#error PHYSFS_SUPPORTS_GRP must be defined.
#endif


typedef struct
{
    FILE *handle;
    int totalEntries;
} GRPinfo;

typedef struct
{
    int startPos;
    int curPos;
    int size;
} GRPfileinfo;


extern const DirFunctions __PHYSFS_DirFunctions_GRP;
static const FileFunctions __PHYSFS_FileFunctions_GRP;


static int GRP_read(FileHandle *handle, void *buffer,
                    unsigned int objSize, unsigned int objCount)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    FILE *fh = (FILE *) (((GRPinfo *) (handle->dirHandle->opaque))->handle);
    int bytesLeft = (finfo->startPos + finfo->size) - finfo->curPos;
    int objsLeft = bytesLeft / objSize;
    int retval = 0;

    if (objsLeft < objCount)
        objCount = objsLeft;

    errno = 0;
    BAIL_IF_MACRO(fseek(fh,finfo->curPos,SEEK_SET) == -1,strerror(errno),-1);

    errno = 0;
    retval = fread(buffer, objSize, objCount, fh);
    finfo->curPos += (retval * objSize);
    BAIL_IF_MACRO((retval < objCount) && (ferror(fh)),strerror(errno),retval);

    return(retval);
} /* GRP_read */


static int GRP_eof(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    return(finfo->curPos >= finfo->startPos + finfo->size);
} /* GRP_eof */


static int GRP_tell(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    return(finfo->curPos - finfo->startPos);
} /* GRP_tell */


static int GRP_seek(FileHandle *handle, int offset)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    int newPos = finfo->startPos + offset;

    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(newPos > finfo->startPos + finfo->size, ERR_PAST_EOF, 0);
    finfo->curPos = newPos;
    return(1);
} /* GRP_seek */


static int GRP_fileLength(FileHandle *handle)
{
    GRPfileinfo *finfo = (GRPfileinfo *) (handle->opaque);
    return(finfo->size);
} /* GRP_fileLength */


static int GRP_fileClose(FileHandle *handle)
{
    free(handle->opaque);
    free(handle);
    return(1);
} /* GRP_fileClose */


static int openGrp(const char *filename, int forWriting, FILE **fh, int *count)
{
    char buf[12];

    assert(sizeof (int) == 4);

    *fh = NULL;
    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    errno = 0;
    *fh = fopen(filename, "rb");
    BAIL_IF_MACRO(*fh == NULL, strerror(errno), 0);
    
    errno = 0;
    BAIL_IF_MACRO(fread(buf, 12, 1, *fh) != 1, strerror(errno), 0);
    BAIL_IF_MACRO(strncmp(buf, "KenSilverman", 12) != 0,
                    ERR_UNSUPPORTED_ARCHIVE, 0);

    if (fread(count, 4, 1, *fh) != 1)
        *count = 0;

    return(1);
} /* openGrp */


static int GRP_isArchive(const char *filename, int forWriting)
{
    FILE *fh;
    int fileCount;
    int retval = openGrp(filename, forWriting, &fh, &fileCount);

    if (fh != NULL)
        fclose(fh);

    return(retval);
} /* GRP_isArchive */


static DirHandle *GRP_openArchive(const char *name, int forWriting)
{
    FILE *fh;
    int fileCount;
    DirHandle *retval = malloc(sizeof (DirHandle));

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    retval->opaque = malloc(sizeof (GRPinfo));
    if (retval->opaque == NULL)
    {
        free(retval);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    if (!openGrp(name, forWriting, &fh, &fileCount))
    {
        if (fh != NULL)
            fclose(fh);
        free(retval->opaque);
        free(retval);
    } /* if */

    ((GRPinfo *) retval->opaque)->handle = fh;
    ((GRPinfo *) retval->opaque)->totalEntries = fileCount;
    retval->funcs = &__PHYSFS_DirFunctions_GRP;
    return(retval);
} /* GRP_openArchive */


static LinkedStringList *GRP_enumerateFiles(DirHandle *h, const char *dirname)
{
    char buf[16];
    GRPinfo *g = (GRPinfo *) (h->opaque);
    FILE *fh = g->handle;
    int i;
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;

    if (*dirname != '\0')   /* no directories in GRP files. */
        return(NULL);

        /* jump to first file entry... */
    errno = 0;
    BAIL_IF_MACRO(fseek(fh, 16, SEEK_SET) == -1, strerror(errno), NULL);

    for (i = 0; i < g->totalEntries; i++)
    {
        errno = 0;
        BAIL_IF_MACRO(fread(buf, 16, 1, fh) != 1, strerror(errno), retval);

        buf[12] = '\0';  /* FILENAME.EXT is all you get. */

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l != NULL)
            break;

        l->str = (char *) malloc(strlen(buf) + 1);
        if (l->str == NULL)
        {
            free(l);
            break;
        } /* if */

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* for */

    return(retval);
} /* GRP_enumerateFiles */


static int getFileEntry(DirHandle *h, const char *name, int *size)
{
    char buf[16];
    GRPinfo *g = (GRPinfo *) (h->opaque);
    FILE *fh = g->handle;
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
    errno = 0;
    BAIL_IF_MACRO(fseek(fh, 16, SEEK_SET) == -1, strerror(errno), -1);

    for (i = 0; i < g->totalEntries; i++)
    {
        int fsize;

        errno = 0;
        BAIL_IF_MACRO(fread(buf, 16, 1, fh) != 1, strerror(errno), -1);

        fsize = *((int *) (buf + 12));

        buf[12] = '\0';  /* FILENAME.EXT is all you get. */

        if (__PHYSFS_platformStricmp(buf, name) == 0)
        {
            if (size != NULL)
                *size = fsize;
            return(retval);
        } /* if */

        retval += fsize;
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
    FileHandle *retval;
    GRPfileinfo *finfo;
    int size, offset;

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

    finfo->startPos = offset;
    finfo->curPos = offset;
    finfo->size = size;
    retval->opaque = (void *) finfo;
    retval->funcs = &__PHYSFS_FileFunctions_GRP;
    retval->dirHandle = h;
    return(retval);
} /* GRP_openRead */


static void GRP_dirClose(DirHandle *h)
{
    fclose( ((GRPinfo *) h->opaque)->handle );
    free(h->opaque);
    free(h);
} /* GRP_dirClose */


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
    "Ryan C. Gordon",
    "http://www.icculus.org/",
};

/* end of grp.c ... */

