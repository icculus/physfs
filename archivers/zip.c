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
 *  Maybe add a seekToStartOfCurrentFile() in unzip.c if complete seek
 *   semantics are impossible.
 *
 *  Could be more i/o efficient if we combined unzip.c and this file.
 *   (and thus lose all the unzGoToNextFile() dummy loops.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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
    char *name;
    unz_file_info info;
    char *symlink;
} ZIPentry;

typedef struct
{
    char *archiveName;
    unz_global_info global;
    ZIPentry *entries;
} ZIPinfo;

typedef struct
{
    unzFile handle;
} ZIPfileinfo;


/* Number of symlinks to follow before we assume it's a recursive link... */
#define SYMLINK_RECURSE_COUNT 20


static PHYSFS_sint64 ZIP_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static int ZIP_eof(FileHandle *handle);
static PHYSFS_sint64 ZIP_tell(FileHandle *handle);
static int ZIP_seek(FileHandle *handle, PHYSFS_uint64 offset);
static PHYSFS_sint64 ZIP_fileLength(FileHandle *handle);
static int ZIP_fileClose(FileHandle *handle);
static int ZIP_isArchive(const char *filename, int forWriting);
static char *ZIP_realpath(unzFile fh, unz_file_info *info, ZIPentry *entry);
static DirHandle *ZIP_openArchive(const char *name, int forWriting);
static LinkedStringList *ZIP_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks);
static int ZIP_exists(DirHandle *h, const char *name);
static int ZIP_isDirectory(DirHandle *h, const char *name);
static int ZIP_isSymLink(DirHandle *h, const char *name);
static FileHandle *ZIP_openRead(DirHandle *h, const char *filename);
static void ZIP_dirClose(DirHandle *h);


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
    "Ryan C. Gordon <icculus@clutteredmind.org>",
    "http://www.icculus.org/physfs/",
};



static PHYSFS_sint64 ZIP_read(FileHandle *handle, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    unzFile fh = ((ZIPfileinfo *) (handle->opaque))->handle;
    int bytes = (int) (objSize * objCount); /* !!! FIXME: overflow? */
    PHYSFS_sint32 rc = unzReadCurrentFile(fh, buffer, bytes);

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


static PHYSFS_sint64 ZIP_tell(FileHandle *handle)
{
    return(unztell(((ZIPfileinfo *) (handle->opaque))->handle));
} /* ZIP_tell */


static int ZIP_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    /* !!! FIXME : this blows. */
    unzFile fh = ((ZIPfileinfo *) (handle->opaque))->handle;
    char *buf = NULL;
    PHYSFS_uint32 bufsize = 4096 * 2;

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
        PHYSFS_uint32 chunk = (offset > bufsize) ? bufsize : offset;
        PHYSFS_sint32 rc = unzReadCurrentFile(fh, buf, chunk);
        BAIL_IF_MACRO(rc == 0, ERR_IO_ERROR, 0);  /* shouldn't happen. */
        BAIL_IF_MACRO(rc == UNZ_ERRNO, ERR_IO_ERROR, 0);
        BAIL_IF_MACRO(rc < 0, ERR_COMPRESSION, 0);
        offset -= rc;
    } /* while */

    free(buf);
    return(offset == 0);
} /* ZIP_seek */


static PHYSFS_sint64 ZIP_fileLength(FileHandle *handle)
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


static void freeEntries(ZIPinfo *info, int count, const char *errmsg)
{
    int i;

    for (i = 0; i < count; i++)
    {
        free(info->entries[i].name);
        if (info->entries[i].symlink != NULL)
            free(info->entries[i].symlink);
    } /* for */

    free(info->entries);

    if (errmsg != NULL)
        __PHYSFS_setError(errmsg);
} /* freeEntries */


/*
 * !!! FIXME: Really implement this.
 * !!! FIXME:  symlinks in zipfiles can be relative paths, including
 * !!! FIXME:  "." and ".." entries. These need to be parsed out.
 * !!! FIXME:  For now, though, we're just copying the relative path. Oh well.
 */
static char *expand_symlink_path(const char *path, ZIPentry *entry)
{
    char *retval = (char *) malloc(strlen(path) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, path);
    return(retval);
} /* expand_symlink_path */


static char *ZIP_realpath(unzFile fh, unz_file_info *info, ZIPentry *entry)
{
    char path[MAXZIPENTRYSIZE];
    int size = info->uncompressed_size;
    int rc;

    BAIL_IF_MACRO(size >= sizeof (path), ERR_IO_ERROR, NULL);
    BAIL_IF_MACRO(unzOpenCurrentFile(fh) != UNZ_OK, ERR_IO_ERROR, NULL);
    rc = unzReadCurrentFile(fh, path, size);
    unzCloseCurrentFile(fh);
    BAIL_IF_MACRO(rc != size, ERR_IO_ERROR, NULL);
    path[size] = '\0'; /* null terminate it. */

    return(expand_symlink_path(path, entry));  /* retval is malloc()'d. */
} /* ZIP_realpath */


static int version_does_symlinks(uLong version)
{
    int retval = 0;
    unsigned char hosttype = ((version >> 8) & 0xFF);

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


static int loadZipEntries(ZIPinfo *info, unzFile unz)
{
    int i, max;

    BAIL_IF_MACRO(unzGetGlobalInfo(unz, &(info->global)) != UNZ_OK,
                    ERR_IO_ERROR, 0);
    BAIL_IF_MACRO(unzGoToFirstFile(unz) != UNZ_OK, ERR_IO_ERROR, 0);

    max = info->global.number_entry;
    info->entries = (ZIPentry *) malloc(sizeof (ZIPentry) * max);
    BAIL_IF_MACRO(info->entries == NULL, ERR_OUT_OF_MEMORY, 0);

    for (i = 0; i < max; i++)
    {
        unz_file_info *d = &((info->entries[i]).info);
        if (unzGetCurrentFileInfo(unz, d, NULL, 0, NULL, 0, NULL, 0) != UNZ_OK)
        {
            freeEntries(info, i, ERR_IO_ERROR);
            return(0);
        } /* if */

        (info->entries[i]).name = (char *) malloc(d->size_filename + 1);
        if ((info->entries[i]).name == NULL)
        {
            freeEntries(info, i, ERR_OUT_OF_MEMORY);
            return(0);
        } /* if */

        info->entries[i].symlink = NULL;

        if (unzGetCurrentFileInfo(unz, NULL, (info->entries[i]).name,
                                  d->size_filename + 1, NULL, 0,
                                  NULL, 0) != UNZ_OK)
        {
            freeEntries(info, i + 1, ERR_IO_ERROR);
            return(0);
        } /* if */

        if (entry_is_symlink(d))
        {
            info->entries[i].symlink = ZIP_realpath(unz, d, &info->entries[i]);
            if (info->entries[i].symlink == NULL)
            {
                freeEntries(info, i + 1, NULL);
                return(0);
            } /* if */
        } /* if */

        if ((unzGoToNextFile(unz) != UNZ_OK) && (i + 1 < max))
        {
            freeEntries(info, i + 1, ERR_IO_ERROR);
            return(0);
        } /* if */
    } /* for */

    return(1);
} /* loadZipEntries */


static DirHandle *ZIP_openArchive(const char *name, int forWriting)
{
    unzFile unz = NULL;
    DirHandle *retval = NULL;

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, NULL);

    retval = malloc(sizeof (DirHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    unz = unzOpen(name);
    if (unz == NULL)
    {
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
    if ( (((ZIPinfo *) (retval->opaque))->archiveName == NULL) ||
         (!loadZipEntries( (ZIPinfo *) (retval->opaque), unz)) )
    {
        if (((ZIPinfo *) (retval->opaque))->archiveName != NULL)
            free(((ZIPinfo *) (retval->opaque))->archiveName);
        free(retval->opaque);
        free(retval);
        unzClose(unz);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    unzClose(unz);
    strcpy(((ZIPinfo *) (retval->opaque))->archiveName, name);
    retval->funcs = &__PHYSFS_DirFunctions_ZIP;

    return(retval);
} /* ZIP_openArchive */


/* !!! This is seriously ugly. */
static LinkedStringList *ZIP_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks)
{
    ZIPinfo *zi = (ZIPinfo *) (h->opaque);
    unsigned int i;
    int dlen;
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;
    char *d;
    ZIPentry *entry;
    char buf[MAXZIPENTRYSIZE];

    dlen = strlen(dirname);
    d = malloc(dlen + 1);
    BAIL_IF_MACRO(d == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(d, dirname);
    if ((dlen > 0) && (d[dlen - 1] == '/'))   /* no trailing slash. */
    {
        dlen--;
        d[dlen] = '\0';
    } /* if */

    for (i = 0, entry = zi->entries; i < zi->global.number_entry; i++, entry++)
    {
        char *ptr;
        char *add_file;
        int this_dlen;

        if ((omitSymLinks) && (entry->symlink != NULL))
            continue;

        this_dlen = strlen(entry->name);
        if (this_dlen + 1 > MAXZIPENTRYSIZE)
            continue;  /* ugh. */

        strcpy(buf, entry->name);

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
    int dlen;
    char *d;
    unsigned int i;
    ZIPentry *entry;

    dlen = strlen(name);
    d = malloc(dlen + 1);
    BAIL_IF_MACRO(d == NULL, ERR_OUT_OF_MEMORY, -1);
    strcpy(d, name);
    if ((dlen > 0) && (d[dlen - 1] == '/'))   /* no trailing slash. */
    {
        dlen--;
        d[dlen] = '\0';
    } /* if */

    for (i = 0, entry = zi->entries; i < zi->global.number_entry; i++, entry++)
    {
        int this_dlen = strlen(entry->name);
        if (this_dlen + 1 > MAXZIPENTRYSIZE)
            continue;  /* ugh. */

        strcpy(buf, entry->name);
        if ((this_dlen > 0) && (buf[this_dlen - 1] == '/'))   /* no trailing slash. */
        {
            this_dlen--;
            buf[this_dlen] = '\0';
        } /* if */

        if ( ((buf[dlen] == '/') || (buf[dlen] == '\0')) &&
             (strncmp(d, buf, dlen) == 0) )
        {
            int retval = i;
            free(d);
            if (follow)  /* follow symlinks? */
            {
                if (entry->symlink != NULL)
                    retval = ZIP_exists_symcheck(h, entry->symlink, follow-1);
            } /* if */
            return(retval);
        } /* if */
    } /* for */

    free(d);
    return(-1);
} /* ZIP_exists_symcheck */


static int ZIP_exists(DirHandle *h, const char *name)
{
    int retval = ZIP_exists_symcheck(h, name, SYMLINK_RECURSE_COUNT);
    int is_sym;

    if (retval == -1)
        return(0);

    /* if it's a symlink, then we ran into a possible symlink loop. */
    is_sym = ( ((ZIPinfo *)(h->opaque))->entries[retval].symlink != NULL );
    BAIL_IF_MACRO(is_sym, ERR_TOO_MANY_SYMLINKS, 0);

    return(1);
} /* ZIP_exists */


static int ZIP_isDirectory(DirHandle *h, const char *name)
{
    int dlen;
    int is_sym;
    int retval = ZIP_exists_symcheck(h, name, SYMLINK_RECURSE_COUNT);

    if (retval == -1)
        return(0);

    /* if it's a symlink, then we ran into a possible symlink loop. */
    is_sym = ( ((ZIPinfo *)(h->opaque))->entries[retval].symlink != NULL );
    BAIL_IF_MACRO(is_sym, ERR_TOO_MANY_SYMLINKS, 0);

    dlen = strlen(name);
    /* !!! yikes. Better way to check? */
    retval = (((ZIPinfo *)(h->opaque))->entries[retval].name[dlen] == '/');
    return(retval);
} /* ZIP_isDirectory */


static int ZIP_isSymLink(DirHandle *h, const char *name)
{
    int retval = ZIP_exists_symcheck(h, name, 0);
    if (retval == -1)
        return(0);

    retval = ( ((ZIPinfo *)(h->opaque))->entries[retval].symlink != NULL );
    return(retval);
} /* ZIP_isSymLink */


static FileHandle *ZIP_openRead(DirHandle *h, const char *filename)
{
    FileHandle *retval = NULL;
    ZIPinfo *zi = ((ZIPinfo *) (h->opaque));
    ZIPfileinfo *finfo = NULL;
    int pos = ZIP_exists_symcheck(h, filename, SYMLINK_RECURSE_COUNT);
    unzFile f;

    BAIL_IF_MACRO(pos == -1, ERR_NO_SUCH_FILE, NULL);

    f = unzOpen(zi->archiveName);
    BAIL_IF_MACRO(f == NULL, ERR_IO_ERROR, NULL);

    if (unzGoToFirstFile(f) != UNZ_OK)
    {
        unzClose(f);
        BAIL_IF_MACRO(1, ERR_IO_ERROR, NULL);
    } /* if */

    for (; pos > 0; pos--)
    {
        if (unzGoToNextFile(f) != UNZ_OK)
        {
            unzClose(f);
            BAIL_IF_MACRO(1, ERR_IO_ERROR, NULL);
        } /* if */
    } /* for */

    if ( (unzOpenCurrentFile(f) != UNZ_OK) ||
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
    ZIPinfo *zi = (ZIPinfo *) (h->opaque);
    freeEntries(zi, zi->global.number_entry, NULL);
    free(zi->archiveName);
    free(zi);
    free(h);
} /* ZIP_dirClose */

/* end of zip.c ... */

