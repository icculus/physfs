/*
 * ZIP support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

/*
 * !!! FIXME: overall design bugs.
 *
 *  It'd be nice if we could remove the thread issues: I/O to individual
 *   files inside an archive are safe, but the searches over the central
 *   directory and the ZIP_openRead() call are race conditions. Basically,
 *   we need to hack something like openDir() into unzip.c, so that directory
 *   reads are separated by handles, and maybe add a openFileByName() call,
 *   or make unzOpenCurrentFile() take a handle, too.
 *
 *  Make unz_file_info.version into two fields of unsigned char. That's what
 *   they are in the zipfile; heavens knows why unzip.c casts it...this causes
 *   a byte ordering headache for me in entry_is_symlink().
 *
 *  Maybe add a seekToStartOfCurrentFile() in unzip.c if complete seek
 *   semantics are impossible.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "physfs.h"
#include "unzip.h"


#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if (!defined PHYSFS_SUPPORTS_ZIP)
#error PHYSFS_SUPPORTS_ZIP must be defined.
#endif

#define MAXZIPENTRYSIZE 256

typedef struct
{
    unzFile handle;
    uLong totalEntries;
    char *archiveName;
} ZIPinfo;

typedef struct
{
    unzFile handle;
} ZIPfileinfo;


extern const DirFunctions __PHYSFS_DirFunctions_ZIP;
static const FileFunctions __PHYSFS_FileFunctions_ZIP;


static int ZIP_read(FileHandle *handle, void *buffer,
                    unsigned int objSize, unsigned int objCount)
{
    unzFile fh = ((ZIPfileinfo *) (handle->opaque))->handle;
    int bytes = objSize * objCount;
    int rc = unzReadCurrentFile(fh, buffer, bytes);

    if (rc < bytes)
        __PHYSFS_setError(ERR_PAST_EOF);
    else if (rc == UNZ_ERRNO)
        __PHYSFS_setError(ERR_IO_ERROR);
    else if (rc < 0)
        __PHYSFS_setError(ERR_COMPRESSION);

    return(rc / objSize);
} /* ZIP_read */


static int ZIP_eof(FileHandle *handle)
{
    return(unzeof(((ZIPfileinfo *) (handle->opaque))->handle));
} /* ZIP_eof */


static int ZIP_tell(FileHandle *handle)
{
    return(unztell(((ZIPfileinfo *) (handle->opaque))->handle));
} /* ZIP_tell */


static int ZIP_fileLength(FileHandle *handle);


static int ZIP_seek(FileHandle *handle, int offset)
{
    /* this blows. */
    unzFile fh = ((ZIPfileinfo *) (handle->opaque))->handle;
    char *buf = NULL;
    int bufsize = 4096 * 2;

    BAIL_IF_MACRO(unztell(fh) == offset, NULL, 1);
    BAIL_IF_MACRO(ZIP_fileLength(handle) <= offset, ERR_PAST_EOF, 0);

        /* reset to the start of the zipfile. */
    unzCloseCurrentFile(fh);
    BAIL_IF_MACRO(unzOpenCurrentFile(fh) != UNZ_OK, ERR_IO_ERROR, 0);

    while ((buf == NULL) && (bufsize >= 512))
    {
        bufsize >>= 1;  /* divides by two. */
        buf = (char *) malloc(bufsize);
    } /* while */
    BAIL_IF_MACRO(buf == NULL, ERR_OUT_OF_MEMORY, 0);

    while (offset > 0)
    {
        int chunk = (offset > bufsize) ? bufsize : offset;
        int rc = unzReadCurrentFile(fh, buf, chunk);
        BAIL_IF_MACRO(rc == 0, ERR_IO_ERROR, 0);  /* shouldn't happen. */
        BAIL_IF_MACRO(rc == UNZ_ERRNO, ERR_IO_ERROR, 0);
        BAIL_IF_MACRO(rc < 0, ERR_COMPRESSION, 0);
        offset -= rc;
    } /* while */

    free(buf);
    return(offset == 0);
} /* ZIP_seek */


static int ZIP_fileLength(FileHandle *handle)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) (handle->opaque);
    unz_file_info info;

    unzGetCurrentFileInfo(finfo->handle, &info, NULL, 0, NULL, 0, NULL, 0);
    return(info.uncompressed_size);
} /* ZIP_fileLength */


static int ZIP_fileClose(FileHandle *handle)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) (handle->opaque);
    unzClose(finfo->handle);
    free(finfo);
    free(handle);
    return(1);
} /* ZIP_fileClose */


static int ZIP_isArchive(const char *filename, int forWriting)
{
    int retval = 0;
    unzFile unz = unzOpen(filename);
    unz_global_info global;

    if (unz != NULL)
    {
        if (unzGetGlobalInfo(unz, &global) == UNZ_OK)
            retval = 1;
        unzClose(unz);
    } /* if */

    return(retval);
} /* ZIP_isArchive */


static DirHandle *ZIP_openArchive(const char *name, int forWriting)
{
    unzFile unz = NULL;
    DirHandle *retval = NULL;
    unz_global_info global;

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, NULL);

    errno = 0;
    BAIL_IF_MACRO(access(name, R_OK) != 0, strerror(errno), NULL);

    retval = malloc(sizeof (DirHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    unz = unzOpen(name);
    if ((unz == NULL) || (unzGetGlobalInfo(unz, &global) != UNZ_OK))
    {
        if (unz)
            unzClose(unz);
        free(retval);
        BAIL_IF_MACRO(1, ERR_UNSUPPORTED_ARCHIVE, NULL);
    } /* if */

    retval->opaque = malloc(sizeof (ZIPinfo));
    if (retval->opaque == NULL)
    {
        free(retval);
        unzClose(unz);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    ((ZIPinfo *) (retval->opaque))->archiveName = malloc(strlen(name) + 1);
    if (((ZIPinfo *) (retval->opaque))->archiveName == NULL)
    {
        free(retval->opaque);
        free(retval);
        unzClose(unz);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    ((ZIPinfo *) (retval->opaque))->handle = unz;
    ((ZIPinfo *) (retval->opaque))->totalEntries = global.number_entry;
    strcpy(((ZIPinfo *) (retval->opaque))->archiveName, name);
    retval->funcs = &__PHYSFS_DirFunctions_ZIP;

    return(retval);
} /* ZIP_openArchive */


/* "uLong" is defined by zlib and/or unzip.h ... */
typedef union
{
    unsigned char uchar4[4];
    uLong ul;
} uchar4_uLong;


static int version_does_symlinks(uLong version)
{
    int retval = 0;
    unsigned char hosttype;
    uchar4_uLong converter;

    converter.ul = version;
    hosttype = converter.uchar4[1]; /* !!! BYTE ORDERING ALERT! */


    /*
     * These are the platforms that can build an archive with symlinks,
     *  according to the Info-ZIP project.
     */
    switch (hosttype)
    {
        case 3:   /* Unix  */
        case 16:  /* BeOS  */
        case 5:   /* Atari */
            retval = 1;
            break;
    } /* switch */

    return(retval);
} /* version_does_symlinks */


static int entry_is_symlink(unz_file_info *info)
{
    return (
              (version_does_symlinks(info->version)) &&
              (info->uncompressed_size > 0) &&
              (info->external_fa & 0x0120000)  /* symlink flag. */
           );
} /* entry_is_symlink */


static char *ZIP_realpath(unzFile fh, unz_file_info *info)
{
    char *retval = NULL;
    int size;

    if (entry_is_symlink(info))
    {
        BAIL_IF_MACRO(unzOpenCurrentFile(fh) != UNZ_OK, ERR_IO_ERROR, NULL);
        size = info->uncompressed_size;
        retval = (char *) malloc(size + 1);
        BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
        if (unzReadCurrentFile(fh, retval, size) != size)
        {
            free(retval);
            __PHYSFS_setError(ERR_IO_ERROR);
            retval = NULL;
        } /* if */
        retval[size] = '\0';
        unzCloseCurrentFile(fh);
    } /* if */

    return(retval);
} /* ZIP_realpath */


/* !!! This is seriously ugly. */
static LinkedStringList *ZIP_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks)
{
    ZIPinfo *zi = (ZIPinfo *) (h->opaque);
    unzFile fh = zi->handle;
    int i;
    int dlen;
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;
    char buf[MAXZIPENTRYSIZE];
    char *d;
    unz_file_info info;

        /* jump to first file entry... */
    BAIL_IF_MACRO(unzGoToFirstFile(fh) != UNZ_OK, ERR_IO_ERROR, NULL);
    dlen = strlen(dirname);
    d = malloc(dlen + 1);
    BAIL_IF_MACRO(d == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(d, dirname);
    if ((dlen > 0) && (d[dlen - 1] == '/'))   /* no trailing slash. */
    {
        dlen--;
        d[dlen] = '\0';
    } /* if */

    for (i = 0; i < zi->totalEntries; i++, unzGoToNextFile(fh))
    {
        char *ptr;
        char *add_file;
        int this_dlen;

        unzGetCurrentFileInfo(fh, &info, buf, sizeof (buf), NULL, 0, NULL, 0);

        if ((omitSymLinks) && (entry_is_symlink(&info)))
            continue;

        buf[sizeof (buf) - 1] = '\0';  /* safety. */
        this_dlen = strlen(buf);
        if ((this_dlen > 0) && (buf[this_dlen - 1] == '/'))   /* no trailing slash. */
        {
            this_dlen--;
            buf[this_dlen] = '\0';
        } /* if */

        if (this_dlen <= dlen)  /* not in this dir. */
            continue;

        if (*d == '\0')
            add_file = buf;
        else
        {
            if (buf[dlen] != '/') /* can't be in same directory? */
                continue;

            buf[dlen] = '\0';
            if (__PHYSFS_platformStricmp(d, buf) != 0)   /* not same directory? */
                continue;

            add_file = buf + dlen + 1;
        } /* else */

        /* handle subdirectories... */
        ptr = strchr(add_file, '/');
        if (ptr != NULL)
        {
            LinkedStringList *j;
            *ptr = '\0';
            for (j = retval; j != NULL; j = j->next)
            {
                if (__PHYSFS_platformStricmp(j->str, ptr) == 0)
                    break;
            } /* for */

            if (j != NULL)
                continue;
        } /* if */

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l == NULL)
            break;

        l->str = (char *) malloc(strlen(add_file) + 1);
        if (l->str == NULL)
        {
            free(l);
            break;
        } /* if */

        strcpy(l->str, add_file);

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* for */

    free(d);
    return(retval);
} /* ZIP_enumerateFiles */


/* !!! This is seriously ugly. */
static int ZIP_exists_symcheck(DirHandle *h, const char *name, int follow)
{
    char buf[MAXZIPENTRYSIZE];
    ZIPinfo *zi = (ZIPinfo *) (h->opaque);
    unzFile fh = zi->handle;
    int dlen;
    char *d;
    int i;
    unz_file_info info;

    BAIL_IF_MACRO(unzGoToFirstFile(fh) != UNZ_OK, ERR_IO_ERROR, 0);
    dlen = strlen(name);
    d = malloc(dlen + 1);
    BAIL_IF_MACRO(d == NULL, ERR_OUT_OF_MEMORY, 0);
    strcpy(d, name);
    if ((dlen > 0) && (d[dlen - 1] == '/'))   /* no trailing slash. */
    {
        dlen--;
        d[dlen] = '\0';
    } /* if */

    for (i = 0; i < zi->totalEntries; i++, unzGoToNextFile(fh))
    {
        int this_dlen;
        unzGetCurrentFileInfo(fh, &info, buf, sizeof (buf), NULL, 0, NULL, 0);
        buf[sizeof (buf) - 1] = '\0';  /* safety. */
        this_dlen = strlen(buf);
        if ((this_dlen > 0) && (buf[this_dlen - 1] == '/'))   /* no trailing slash. */
        {
            this_dlen--;
            buf[this_dlen] = '\0';
        } /* if */

        if ( ((buf[dlen] == '/') || (buf[dlen] == '\0')) &&
             (strncmp(d, buf, dlen) == 0) )
        {
            int retval = 1;
            free(d);
            if (follow)  /* follow symlinks? */
            {
                char *real = ZIP_realpath(fh, &info);
                if (real != NULL)
                {
                    retval = ZIP_exists_symcheck(h, real, follow - 1);
                    free(real);
                } /* if */
            } /* if */
            return(retval);
        } /* if */
    } /* for */

    free(d);
    return(0);
} /* ZIP_exists_symcheck */


static int ZIP_exists_nofollow(DirHandle *h, const char *name)
{
    return(ZIP_exists_symcheck(h, name, 0));
} /* ZIP_exists_nofollow */


static int ZIP_exists(DirHandle *h, const char *name)
{
    /* follow at most 20 links to prevent recursion... */
    int retval = ZIP_exists_symcheck(h, name, 20);
    unz_file_info info;
    unzFile fh = ((ZIPinfo *) (h->opaque))->handle;

    if (retval)
    {
        /* current zip entry will be the file in question. */
        unzGetCurrentFileInfo(fh, &info, NULL, 0, NULL, 0, NULL, 0);

        /* if it's a symlink, then we ran into a possible symlink loop. */
        BAIL_IF_MACRO(entry_is_symlink(&info), ERR_TOO_MANY_SYMLINKS, 0);
    } /* if */

    return(retval);
} /* ZIP_exists */


static int ZIP_isDirectory(DirHandle *h, const char *name)
{
    char buf[MAXZIPENTRYSIZE];
    unzFile fh = ((ZIPinfo *) (h->opaque))->handle;
    int retval = ZIP_exists(h, name);
    int dlen = strlen(name);

    if (retval)
    {
        /* current zip entry will be the file in question. */
        unzGetCurrentFileInfo(fh, NULL, buf, sizeof (buf), NULL, 0, NULL, 0);
        retval = (buf[dlen] == '/');  /* !!! yikes. */
    } /* if */

    return(retval);
} /* ZIP_isDirectory */


static int ZIP_isSymLink(DirHandle *h, const char *name)
{
    unzFile fh = ((ZIPinfo *) (h->opaque))->handle;
    int retval = ZIP_exists_nofollow(h, name);
    unz_file_info info;

    if (retval)
    {
        /* current zip entry will be the file in question. */
        unzGetCurrentFileInfo(fh, &info, NULL, 0, NULL, 0, NULL, 0);
        retval = entry_is_symlink(&info);
    } /* if */

    return(retval);
} /* ZIP_isSymLink */


static FileHandle *ZIP_openRead(DirHandle *h, const char *filename)
{
    FileHandle *retval = NULL;
    ZIPfileinfo *finfo = NULL;
    char *name = ((ZIPinfo *) (h->opaque))->archiveName;
    unzFile f;

    BAIL_IF_MACRO(!ZIP_exists(h, filename), ERR_NO_SUCH_FILE, NULL);

    f = unzOpen(name);
    BAIL_IF_MACRO(f == NULL, ERR_IO_ERROR, NULL);

    if ( (unzLocateFile(f, filename, 2) != UNZ_OK) ||
         (unzOpenCurrentFile(f) != UNZ_OK) ||
         ( (finfo = (ZIPfileinfo *) malloc(sizeof (ZIPfileinfo))) == NULL ) )
    {
        unzClose(f);
        BAIL_IF_MACRO(1, ERR_IO_ERROR, NULL);
    } /* if */

    if ( (!(retval = (FileHandle *) malloc(sizeof (FileHandle)))) ||
         (!(retval->opaque = (ZIPfileinfo *) malloc(sizeof (ZIPfileinfo)))) )
    {
        if (retval)
            free(retval);
        unzClose(f);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    finfo->handle = f;
    retval->opaque = (void *) finfo;
    retval->funcs = &__PHYSFS_FileFunctions_ZIP;
    retval->dirHandle = h;
    return(retval);
} /* ZIP_openRead */


static void ZIP_dirClose(DirHandle *h)
{
    unzClose(((ZIPinfo *) (h->opaque))->handle);
    free(h->opaque);
    free(h);
} /* ZIP_dirClose */


static const FileFunctions __PHYSFS_FileFunctions_ZIP =
{
    ZIP_read,       /* read() method       */
    NULL,           /* write() method      */
    ZIP_eof,        /* eof() method        */
    ZIP_tell,       /* tell() method       */
    ZIP_seek,       /* seek() method       */
    ZIP_fileLength, /* fileLength() method */
    ZIP_fileClose   /* fileClose() method  */
};


const DirFunctions __PHYSFS_DirFunctions_ZIP =
{
    ZIP_isArchive,          /* isArchive() method      */
    ZIP_openArchive,        /* openArchive() method    */
    ZIP_enumerateFiles,     /* enumerateFiles() method */
    ZIP_exists,             /* exists() method         */
    ZIP_isDirectory,        /* isDirectory() method    */
    ZIP_isSymLink,          /* isSymLink() method      */
    ZIP_openRead,           /* openRead() method       */
    NULL,                   /* openWrite() method      */
    NULL,                   /* openAppend() method     */
    NULL,                   /* remove() method         */
    NULL,                   /* mkdir() method          */
    ZIP_dirClose            /* dirClose() method       */
};

const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_ZIP =
{
    "ZIP",
    "PkZip/WinZip/Info-Zip compatible",
    "Ryan C. Gordon (icculus@linuxgames.com)",
    "http://www.icculus.org/~icculus/",
};

/* end of zip.c ... */

