/*
 * ZIP support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon, with some peeking at "unzip.c"
 *   by Gilles Vollant.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined PHYSFS_SUPPORTS_ZIP)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include "physfs.h"
#include "zlib.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


/*
 * A buffer of ZIP_READBUFSIZE is malloc() for each compressed file opened,
 *  and is free()'d when you close the file; compressed data is read into
 *  this buffer, and then is decompressed into the buffer passed to
 *  PHYSFS_read().
 *
 * Uncompressed entries in a zipfile do not allocate this buffer; they just
 *  read data directly into the buffer passed to PHYSFS_read().
 *
 * Depending on your speed and memory requirements, you should tweak this
 *  value.
 */
#define ZIP_READBUFSIZE   (16 * 1024)

/*
 * One ZIPentry is kept for each file in an open ZIP archive.
 */
typedef struct
{
    char *name;                         /* Name of file in archive   */
    char *symlink;                      /* NULL or file we link to   */
    int fixed_up;                       /* Have we updated offset?   */
    PHYSFS_uint32 offset;               /* offset of data in file    */
    PHYSFS_uint16 version;              /* version made by           */
    PHYSFS_uint16 version_needed;       /* version needed to extract */
    PHYSFS_uint16 flag;                 /* general purpose bit flag  */
    PHYSFS_uint16 compression_method;   /* compression method        */
    PHYSFS_uint32 crc;                  /* crc-32                    */
    PHYSFS_uint32 compressed_size;      /* compressed size           */
    PHYSFS_uint32 uncompressed_size;    /* uncompressed size         */
    PHYSFS_sint64 last_mod_time;        /* last file mod time        */
} ZIPentry;

/*
 * One ZIPinfo is kept for each open ZIP archive.
 */
typedef struct
{
    char *archiveName;        /* path to ZIP in platform-dependent notation. */
    PHYSFS_uint16 entryCount; /* Number of files in ZIP.                     */
    ZIPentry *entries;        /* info on all files in ZIP.                   */
} ZIPinfo;

/*
 * One ZIPfileinfo is kept for each open file in a ZIP archive.
 */
typedef struct
{
    ZIPentry *entry;                      /* Info on file.              */
    void *handle;                         /* physical file handle.      */
    PHYSFS_uint32 compressed_position;    /* offset in compressed data. */
    PHYSFS_uint32 uncompressed_position;  /* tell() position.           */
    PHYSFS_uint8 *buffer;                 /* decompression buffer.      */
    z_stream stream;                      /* zlib stream state.         */
} ZIPfileinfo;


/* Magic numbers... */
#define ZIP_LOCAL_FILE_SIG          0x04034b50
#define ZIP_CENTRAL_DIR_SIG         0x02014b50
#define ZIP_END_OF_CENTRAL_DIR_SIG  0x06054b50

/* compression methods... */
#define COMPMETH_NONE 0
/* ...and others... */


#define UNIX_FILETYPE_MASK    0170000
#define UNIX_FILETYPE_SYMLINK 0120000


#define MAXZIPENTRYSIZE 256  /* !!! FIXME: get rid of this. */


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
static char *get_zip_realpath(void *in, ZIPentry *entry);
static DirHandle *ZIP_openArchive(const char *name, int forWriting);
static LinkedStringList *ZIP_enumerateFiles(DirHandle *h,
                                            const char *dirname,
                                            int omitSymLinks);
static int ZIP_exists(DirHandle *h, const char *name);
static int ZIP_isDirectory(DirHandle *h, const char *name);
static int ZIP_isSymLink(DirHandle *h, const char *name);
static PHYSFS_sint64 ZIP_getLastModTime(DirHandle *h, const char *name);
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
    ZIP_getLastModTime,     /* getLastModTime() method */
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



/*
 * Wrap all zlib calls in this, so the physfs error state is set appropriately.
 */
static int zlib_err(int rc)
{
    const char *err = NULL;

    switch (rc)
    {
        case Z_OK:
        case Z_STREAM_END:
            break;   /* not errors. */

        case Z_ERRNO:
            err = strerror(errno);
            break;

        case Z_NEED_DICT:
            err = "zlib: need dictionary";
            break;
        case Z_DATA_ERROR:
            err = "zlib: need dictionary";
            break;
        case Z_MEM_ERROR:
            err = "zlib: memory error";
            break;
        case Z_BUF_ERROR:
            err = "zlib: buffer error";
            break;
        case Z_VERSION_ERROR:
            err = "zlib: version error";
            break;
        default:
            err = "unknown zlib error";
            break;
    } /* switch */

    if (err != NULL)
        __PHYSFS_setError(err);

    return(rc);
} /* zlib_err */


/*
 * Read an unsigned 32-bit int and swap to native byte order.
 */
static int readui32(void *in, PHYSFS_uint32 *val)
{
    PHYSFS_uint32 v;
    BAIL_IF_MACRO(__PHYSFS_platformRead(in, &v, sizeof (v), 1) != 1, NULL, 0);
    *val = PHYSFS_swapULE32(v);
    return(1);
} /* readui32 */


/*
 * Read an unsigned 16-bit int and swap to native byte order.
 */
static int readui16(void *in, PHYSFS_uint16 *val)
{
    PHYSFS_uint16 v;
    BAIL_IF_MACRO(__PHYSFS_platformRead(in, &v, sizeof (v), 1) != 1, NULL, 0);
    *val = PHYSFS_swapULE16(v);
    return(1);
} /* readui16 */


static PHYSFS_sint64 ZIP_read(FileHandle *handle, void *buf,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) (handle->opaque);
    ZIPentry *entry = finfo->entry;
    PHYSFS_sint64 retval = 0;
    PHYSFS_sint64 maxread = ((PHYSFS_sint64) objSize) * objCount;
    PHYSFS_sint64 avail = entry->uncompressed_size -
                          finfo->uncompressed_position;

    BAIL_IF_MACRO(maxread == 0, NULL, 0);    /* quick rejection. */

    if (avail < maxread)
    {
        maxread = avail - (avail % objSize);
        objCount = maxread / objSize;
        BAIL_IF_MACRO(objCount == 0, ERR_PAST_EOF, 0);  /* quick rejection. */
        __PHYSFS_setError(ERR_PAST_EOF);   /* this is always true here. */
    } /* if */

    if (entry->compression_method == COMPMETH_NONE)
    {
        retval = __PHYSFS_platformRead(finfo->handle, buf, objSize, objCount);
    } /* if */

    else
    {
        finfo->stream.next_out = buf;
        finfo->stream.avail_out = objSize * objCount;

        while (retval < maxread)
        {
            PHYSFS_uint32 before = finfo->stream.total_out;
            int rc;

            if (finfo->stream.avail_in == 0)
            {
                PHYSFS_sint64 br;

                br = entry->compressed_size - finfo->compressed_position;
                if (br > 0)
                {
                    if (br > ZIP_READBUFSIZE)
                        br = ZIP_READBUFSIZE;

                    br = __PHYSFS_platformRead(finfo->handle,
                                               finfo->buffer,
                                               1, br);
                    if (br <= 0)
                        break;

                    finfo->compressed_position += br;
                    finfo->stream.next_in = finfo->buffer;
                    finfo->stream.avail_in = br;
                } /* if */
            } /* if */

            rc = zlib_err(inflate(&finfo->stream, Z_SYNC_FLUSH));
            retval += (finfo->stream.total_out - before);

            if (rc != Z_OK)
                break;
        } /* while */

        retval /= objSize;
    } /* else */

    if (retval > 0)
        finfo->uncompressed_position += (retval * objSize);

    return(retval);
} /* ZIP_read */


static int ZIP_eof(FileHandle *handle)
{
    ZIPfileinfo *finfo = ((ZIPfileinfo *) (handle->opaque));
    return(finfo->uncompressed_position >= finfo->entry->uncompressed_size);
} /* ZIP_eof */


static PHYSFS_sint64 ZIP_tell(FileHandle *handle)
{
    return(((ZIPfileinfo *) (handle->opaque))->uncompressed_position);
} /* ZIP_tell */


static int ZIP_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) (handle->opaque);
    ZIPentry *entry = finfo->entry;
    void *in = finfo->handle;

    BAIL_IF_MACRO(offset > entry->uncompressed_size, ERR_PAST_EOF, 0);

    if (entry->compression_method == COMPMETH_NONE)
    {
        PHYSFS_sint64 newpos = offset + entry->offset;
        BAIL_IF_MACRO(!__PHYSFS_platformSeek(in, newpos), NULL, 0);
        finfo->uncompressed_position = newpos;
    } /* if */

    else
    {
        /*
         * If seeking backwards, we need to redecode the file
         *  from the start and throw away the compressed bits until we hit
         *  the offset we need. If seeking forward, we still need to
         *  redecode, but we don't rewind first.
         */
        if (offset < finfo->uncompressed_position)
        {
            /* we do a copy so state is sane if inflateInit2() fails. */
            z_stream str;
            memset(&str, '\0', sizeof (z_stream));
            if (zlib_err(inflateInit2(&str, -MAX_WBITS)) != Z_OK)
                return(0);

            if (!__PHYSFS_platformSeek(in, entry->offset))
                return(0);

            inflateEnd(&finfo->stream);
            memcpy(&finfo->stream, &str, sizeof (z_stream));
            finfo->uncompressed_position = finfo->compressed_position = 0;
        } /* if */

        while (finfo->uncompressed_position != offset)
        {
            PHYSFS_uint8 buf[512];
            PHYSFS_uint32 maxread = offset - finfo->uncompressed_position;
            if (maxread > sizeof (buf))
                maxread = sizeof (buf);

            if (ZIP_read(handle, buf, maxread, 1) != 1)
                return(0);
        } /* while */
    } /* else */

    return(1);
} /* ZIP_seek */


static PHYSFS_sint64 ZIP_fileLength(FileHandle *handle)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) (handle->opaque);
    return(finfo->entry->uncompressed_size);
} /* ZIP_fileLength */


static int ZIP_fileClose(FileHandle *handle)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) (handle->opaque);
    __PHYSFS_platformClose(finfo->handle);

    if (finfo->entry->compression_method != COMPMETH_NONE)
        inflateEnd(&finfo->stream);

    if (finfo->buffer != NULL)
        free(finfo->buffer);

    free(finfo);
    return(1);
} /* ZIP_fileClose */


static PHYSFS_sint64 find_end_of_central_dir(void *in, PHYSFS_sint64 *len)
{
    /* !!! FIXME: potential race condition! */
    /* !!! FIXME: mutex this or switch to smaller stack-based buffer. */
    static PHYSFS_uint8 buf[0xFFFF + 20];

    PHYSFS_sint32 i;
    PHYSFS_sint64 filelen;
    PHYSFS_sint64 filepos;
    PHYSFS_sint32 maxread;

    filelen = __PHYSFS_platformFileLength(in);
    BAIL_IF_MACRO(filelen == -1, NULL, 0);

    /*
     * Jump to the end of the file and start reading backwards.
     *  The last thing in the file is the zipfile comment, which is variable
     *  length, and the field that specifies its size is before it in the
     *  file (argh!)...this means that we need to scan backwards until we
     *  hit the end-of-central-dir signature. We can then sanity check that
     *  the comment was as big as it should be to make sure we're in the
     *  right place. The comment length field is 16 bits, so we can stop
     *  searching for that signature after 64k at most, and call it a
     *  corrupted zipfile.
     */

    /*
     * !!! FIXME: This was easier than reading backwards in chunks, but it's
     * !!! FIXME:  rather memory hungry.
     */
    if (sizeof (buf) < filelen)
    {
        filepos = filelen - sizeof (buf);
        maxread = sizeof (buf);
    } /* if */
    else
    {
        filepos = 0;
        maxread = filelen;
    } /* else */

    BAIL_IF_MACRO(!__PHYSFS_platformSeek(in, filepos), NULL, -1);
    BAIL_IF_MACRO(__PHYSFS_platformRead(in, buf, maxread, 1) != 1, NULL, -1);

    for (i = maxread - 4; i > 0; i--)
    {
        if ((buf[i + 0] == 0x50) &&
            (buf[i + 1] == 0x4B) &&
            (buf[i + 2] == 0x05) &&
            (buf[i + 3] == 0x06) )
        {
            break;  /* that's the signature! */
        } /* if */
    } /* for */

    BAIL_IF_MACRO(i < 0, ERR_NOT_AN_ARCHIVE, -1);

    if (len != NULL)
        *len = filelen;

    return(filelen - (maxread - i));
} /* find_end_of_central_dir */


static int ZIP_isArchive(const char *filename, int forWriting)
{
    PHYSFS_uint32 sig;
    int retval;
    void *in;

    in = __PHYSFS_platformOpenRead(filename);
    BAIL_IF_MACRO(in == NULL, NULL, 0);

    /*
     * The first thing in a zip file might be the signature of the
     *  first local file record, so it makes for a quick determination.
     */
    BAIL_IF_MACRO(!readui32(in, &sig), NULL, 0);
    retval = (sig == ZIP_LOCAL_FILE_SIG);
    if (!retval)
    {
        /*
         * No sig...might be a ZIP with data at the start
         *  (a self-extracting executable, etc), so we'll have to do
         *  it the hard way...
         */
        retval = (find_end_of_central_dir(in, NULL) == -1);
    } /* if */

    __PHYSFS_platformClose(in);
    return(retval);
} /* ZIP_isArchive */


static int zip_set_open_position(void *in, ZIPentry *entry)
{
    /*
     * We fix up the offset to point to the actual data on the
     *  first open, since we don't want to seek across the whole file on
     *  archive open (can be SLOW on large, CD-stored files), but we
     *  need to check the local file header...not just for corruption,
     *  but since it stores offset info the central directory does not.
     */
    if (!entry->fixed_up)
    {
        PHYSFS_uint32 ui32;
        PHYSFS_uint16 ui16;
        PHYSFS_uint16 fnamelen;
        PHYSFS_uint16 extralen;

        BAIL_IF_MACRO(!__PHYSFS_platformSeek(in, entry->offset), NULL, 0);
        BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
        BAIL_IF_MACRO(ui32 != ZIP_LOCAL_FILE_SIG, ERR_CORRUPTED, 0);
        BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
        BAIL_IF_MACRO(ui16 != entry->version_needed, ERR_CORRUPTED, 0);
        BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);  /* general bits. */
        BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
        BAIL_IF_MACRO(ui16 != entry->compression_method, ERR_CORRUPTED, 0);
        BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);  /* date/time */
        BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
        BAIL_IF_MACRO(ui32 != entry->crc, ERR_CORRUPTED, 0);
        BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
        BAIL_IF_MACRO(ui32 != entry->compressed_size, ERR_CORRUPTED, 0);
        BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
        BAIL_IF_MACRO(ui32 != entry->uncompressed_size, ERR_CORRUPTED, 0);
        BAIL_IF_MACRO(!readui16(in, &fnamelen), NULL, 0);
        BAIL_IF_MACRO(!readui16(in, &extralen), NULL, 0);

        entry->offset += fnamelen + extralen + 30;
        entry->fixed_up = 1;
    } /* if */

    return(__PHYSFS_platformSeek(in, entry->offset));
} /* zip_set_open_position */



static void freeEntries(ZIPinfo *info, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        free(info->entries[i].name);
        if (info->entries[i].symlink != NULL)
            free(info->entries[i].symlink);
    } /* for */

    free(info->entries);
} /* freeEntries */


/*
 * !!! FIXME: Really implement this.
 * !!! FIXME:  symlinks in zipfiles can be relative paths, including
 * !!! FIXME:  "." and ".." entries. These need to be parsed out.
 * !!! FIXME:  For now, though, we're just copying the relative path. Oh well.
 */
static char *expand_symlink_path(char *path, ZIPentry *entry)
{
/*
    char *retval = (char *) malloc(strlen(path) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, path);
    return(retval);
*/
    return(path);  /* !!! FIXME */
} /* expand_symlink_path */


static char *get_zip_realpath(void *in, ZIPentry *entry)
{
    char *path;
    PHYSFS_uint32 size = entry->uncompressed_size;
    int rc = 0;

    BAIL_IF_MACRO(!zip_set_open_position(in, entry), NULL, NULL);
    path = (char *) malloc(size + 1);
    BAIL_IF_MACRO(path == NULL, ERR_OUT_OF_MEMORY, NULL);
    
    if (entry->compression_method == COMPMETH_NONE)
        rc = (__PHYSFS_platformRead(in, path, size, 1) == 1);

    else  /* symlink target path is compressed... */
    {
        z_stream stream;
        PHYSFS_uint32 compsize = entry->compressed_size;
        PHYSFS_uint8 *compressed = (PHYSFS_uint8 *) malloc(compsize);
        if (compressed != NULL)
        {
            if (__PHYSFS_platformRead(in, compressed, compsize, 1) == 1)
            {
                memset(&stream, '\0', sizeof (z_stream));
                stream.next_in = compressed;
                stream.avail_in = compsize;
                stream.next_out = path;
                stream.avail_out = size;
                if (zlib_err(inflateInit2(&stream, -MAX_WBITS)) == Z_OK)
                {
                    rc = zlib_err(inflate(&stream, Z_FINISH));
                    inflateEnd(&stream);

                    /* both are acceptable outcomes... */
                    rc = ((rc == Z_OK) || (rc == Z_STREAM_END));
                } /* if */
            } /* if */
            free(compressed);
        } /* if */
    } /* else */

    if (!rc)
    {
        free(path);
        return(NULL);
    } /* if */
    path[entry->uncompressed_size] = '\0'; /* null-terminate it. */

    return(expand_symlink_path(path, entry)); /* retval is realloc()'d path. */
} /* get_zip_realpath */


static int version_does_symlinks(PHYSFS_uint32 version)
{
    int retval = 0;
    PHYSFS_uint8 hosttype = (PHYSFS_uint8) ((version >> 8) & 0xFF);

    switch (hosttype)
    {
            /*
             * These are the platforms that can NOT build an archive with
             *  symlinks, according to the Info-ZIP project.
             */
        case 0:  /* FS_FAT_  */
        case 1:  /* AMIGA_   */
        case 2:  /* VMS_     */
        case 4:  /* VM_CSM_  */
        case 6:  /* FS_HPFS_ */
        case 11: /* FS_NTFS_ */
        case 14: /* FS_VFAT_ */
        case 13: /* ACORN_   */
        case 15: /* MVS_     */
        case 18: /* THEOS_   */
            break;  /* do nothing. */

        default:  /* assume the rest to be unix-like. */
            retval = 1;
            break;
    } /* switch */

    return(retval);
} /* version_does_symlinks */


static int entry_is_symlink(ZIPentry *entry, PHYSFS_uint32 extern_attr)
{
    PHYSFS_uint16 xattr = ((extern_attr >> 16) & 0xFFFF);

    return (
              (version_does_symlinks(entry->version)) &&
              (entry->uncompressed_size > 0) &&
              ((xattr & UNIX_FILETYPE_MASK) == UNIX_FILETYPE_SYMLINK)
           );
} /* entry_is_symlink */


PHYSFS_sint64 dos_time_to_physfs_time(PHYSFS_uint32 dostime)
{
    PHYSFS_uint32 dosdate = (PHYSFS_uint32) (dostime >> 16);
    struct tm unixtime;
    memset(&unixtime, '\0', sizeof (unixtime));

    unixtime.tm_mday = (PHYSFS_uint32) (dosdate & 0x1F);
    unixtime.tm_mon = (PHYSFS_uint32) ((((dosdate) & 0x1E0) / 0x20) - 1);
    unixtime.tm_year = (PHYSFS_uint32) (((dosdate & 0x0FE00) / 0x0200) + 80);

    unixtime.tm_hour = (PHYSFS_uint32) ((dostime & 0xF800) / 0x800);
    unixtime.tm_min = (PHYSFS_uint32) ((dostime & 0x7E0) / 0x20);
    unixtime.tm_sec = (PHYSFS_uint32) (2 * (dostime & 0x1F));

    return((PHYSFS_sint64) mktime(&unixtime));
} /* dos_time_to_physfs_time */


static int load_zip_entry(void *in, ZIPentry *entry, PHYSFS_uint32 ofs_fixup)
{
    PHYSFS_uint16 fnamelen, extralen, commentlen;
    PHYSFS_uint32 external_attr;
    PHYSFS_uint16 ui16;
    PHYSFS_uint32 ui32;
    PHYSFS_sint64 si64;

    /* sanity check with central directory signature... */
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != ZIP_CENTRAL_DIR_SIG, ERR_CORRUPTED, 0);

    /* Get the pertinent parts of the record... */
    BAIL_IF_MACRO(!readui16(in, &entry->version), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &entry->version_needed), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &entry->flag), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &entry->compression_method), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    entry->last_mod_time = dos_time_to_physfs_time(ui32);
    BAIL_IF_MACRO(!readui32(in, &entry->crc), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &entry->compressed_size), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &entry->uncompressed_size), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &fnamelen), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &extralen), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &commentlen), NULL, 0);
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);  /* disk number start */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);  /* internal file attribs */
    BAIL_IF_MACRO(!readui32(in, &external_attr), NULL, 0);
    BAIL_IF_MACRO(!readui32(in, &entry->offset), NULL, 0);
    entry->offset += ofs_fixup;
    entry->fixed_up = 0; /* we need to do a second fixup at openRead(). */

    entry->name = (char *) malloc(fnamelen + 1);
    BAIL_IF_MACRO(entry->name == NULL, ERR_OUT_OF_MEMORY, 0);
    if (__PHYSFS_platformRead(in, entry->name, fnamelen, 1) != 1)
        goto load_zip_entry_puked;

    entry->name[fnamelen] = '\0';  /* null-terminate the filename. */

    si64 = __PHYSFS_platformTell(in);
    if (si64 == -1)
        goto load_zip_entry_puked;

        /* If this is a symlink, resolve it. */
    if (!entry_is_symlink(entry, external_attr))
        entry->symlink = NULL;
    else
    {
        entry->symlink = get_zip_realpath(in, entry);
        if (entry->symlink == NULL)
            goto load_zip_entry_puked;
    } /* else */

        /* seek to the start of the next entry in the central directory... */
    if (!__PHYSFS_platformSeek(in, si64 + extralen + commentlen))
        goto load_zip_entry_puked;

    return(1);  /* success. */

load_zip_entry_puked:
    free(entry->name);
    return(0);  /* failure. */
} /* load_zip_entry */


static int load_zip_entries(void *in, DirHandle *dirh,
                            PHYSFS_uint32 data_ofs, PHYSFS_uint32 central_ofs)
{
    ZIPinfo *info = (ZIPinfo *) dirh->opaque;
    PHYSFS_uint32 max = info->entryCount;
    PHYSFS_uint32 i;

    BAIL_IF_MACRO(!__PHYSFS_platformSeek(in, central_ofs), NULL, 0);

    info->entries = (ZIPentry *) malloc(sizeof (ZIPentry) * max);
    BAIL_IF_MACRO(info->entries == NULL, ERR_OUT_OF_MEMORY, 0);

    for (i = 0; i < max; i++)
    {
        if (!load_zip_entry(in, &info->entries[i], data_ofs))
        {
            freeEntries(info, i);
            return(0);
        } /* if */
    } /* for */

    return(1);
} /* load_zip_entries */


static int parse_end_of_central_dir(void *in, DirHandle *dirh,
                                    PHYSFS_uint32 *data_start,
                                    PHYSFS_uint32 *central_dir_ofs)
{
    ZIPinfo *zipinfo = (ZIPinfo *) dirh->opaque;
    PHYSFS_uint32 ui32;
    PHYSFS_uint16 ui16;
    PHYSFS_sint64 len;
    PHYSFS_sint64 pos;

    /* find the end-of-central-dir record, and seek to it. */
    pos = find_end_of_central_dir(in, &len);
    BAIL_IF_MACRO(pos == -1, NULL, 0);
	BAIL_IF_MACRO(!__PHYSFS_platformSeek(in, pos), NULL, 0);

    /* check signature again, just in case. */
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);
    BAIL_IF_MACRO(ui32 != ZIP_END_OF_CENTRAL_DIR_SIG, ERR_NOT_AN_ARCHIVE, 0);

	/* number of this disk */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
    BAIL_IF_MACRO(ui16 != 0, ERR_UNSUPPORTED_ARCHIVE, 0);

	/* number of the disk with the start of the central directory */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);
    BAIL_IF_MACRO(ui16 != 0, ERR_UNSUPPORTED_ARCHIVE, 0);

	/* total number of entries in the central dir on this disk */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);

	/* total number of entries in the central dir */
    BAIL_IF_MACRO(!readui16(in, &zipinfo->entryCount), NULL, 0);
    BAIL_IF_MACRO(ui16 != zipinfo->entryCount, ERR_UNSUPPORTED_ARCHIVE, 0);

	/* size of the central directory */
    BAIL_IF_MACRO(!readui32(in, &ui32), NULL, 0);

	/* offset of central directory */
    BAIL_IF_MACRO(!readui32(in, central_dir_ofs), NULL, 0);
    BAIL_IF_MACRO(pos < *central_dir_ofs + ui32, ERR_UNSUPPORTED_ARCHIVE, 0);

    /*
     * For self-extracting archives, etc, there's crapola in the file
     *  before the zipfile records; we calculate how much data there is
     *  prepended by determining how far the central directory offset is
     *  from where it is supposed to be (start of end-of-central-dir minus
     *  sizeof central dir)...the difference in bytes is how much arbitrary
     *  data is at the start of the physical file.
     */
	*data_start = pos - (*central_dir_ofs + ui32);

    /* Now that we know the difference, fix up the central dir offset... */
    *central_dir_ofs += *data_start;

	/* zipfile comment length */
    BAIL_IF_MACRO(!readui16(in, &ui16), NULL, 0);

    /*
     * Make sure that the comment length matches to the end of file...
     *  If it doesn't, we're either in the wrong part of the file, or the
     *  file is corrupted, but we give up either way.
     */
    BAIL_IF_MACRO((pos + 22 + ui16) != len, ERR_UNSUPPORTED_ARCHIVE, 0);

    return(1);  /* made it. */
} /* parse_end_of_central_dir */


static DirHandle *allocate_zip_dirhandle(const char *name)
{
    char *ptr;
    DirHandle *retval = malloc(sizeof (DirHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    memset(retval, '\0', sizeof (DirHandle));

    retval->opaque = malloc(sizeof (ZIPinfo));
    if (retval->opaque == NULL)
    {
        free(retval);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    memset(retval->opaque, '\0', sizeof (ZIPinfo));

    ptr = (char *) malloc(strlen(name) + 1);
    if (ptr == NULL)
    {
        free(retval->opaque);
        free(retval);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    ((ZIPinfo *) (retval->opaque))->archiveName = ptr;
    strcpy(((ZIPinfo *) (retval->opaque))->archiveName, name);

    retval->funcs = &__PHYSFS_DirFunctions_ZIP;

    return(retval);
} /* allocate_zip_dirhandle */


static DirHandle *ZIP_openArchive(const char *name, int forWriting)
{
    DirHandle *retval = NULL;
    void *in = NULL;
    PHYSFS_uint32 data_start;
    PHYSFS_uint32 central_dir_ofs;
    int success = 0;

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, NULL);

    if ((in = __PHYSFS_platformOpenRead(name)) == NULL)
        goto zip_openarchive_end;
    
    if ((retval = allocate_zip_dirhandle(name)) == NULL)
        goto zip_openarchive_end;

    if (!parse_end_of_central_dir(in, retval, &data_start, &central_dir_ofs))
        goto zip_openarchive_end;

    if (!load_zip_entries(in, retval, data_start, central_dir_ofs))
        goto zip_openarchive_end;

    success = 1;  /* ...and we're good to go.  :)  */

zip_openarchive_end:
    if (!success)  /* clean up for failures. */
    {
        if (retval != NULL)
        {
            if (retval->opaque != NULL)
            {
                if (((ZIPinfo *) (retval->opaque))->archiveName != NULL)
                    free(((ZIPinfo *) (retval->opaque))->archiveName);

                free(retval->opaque);
            } /* if */

            free(retval);
            retval = NULL;
        } /* if */
    } /* if */

    if (in != NULL)
        __PHYSFS_platformClose(in);  /* Close this even with success. */

    return(retval);
} /* ZIP_openArchive */


/* !!! FIXME: This is seriously ugly. */
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
    if ((dlen > 0) && (d[dlen - 1] == '/'))   /* remove trailing slash. */
    {
        dlen--;
        d[dlen] = '\0';
    } /* if */

    for (i = 0, entry = zi->entries; i < zi->entryCount; i++, entry++)
    {
        char *ptr;
        char *add_file;
        int this_dlen;

        if ((omitSymLinks) && (entry->symlink != NULL))
            continue;

        this_dlen = strlen(entry->name);
        if (this_dlen + 1 > MAXZIPENTRYSIZE)  /* !!! FIXME */
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


/* !!! FIXME: This is seriously ugly. */
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

    for (i = 0, entry = zi->entries; i < zi->entryCount; i++, entry++)
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
    int pos = ZIP_exists_symcheck(h, name, SYMLINK_RECURSE_COUNT);
    int is_sym;

    BAIL_IF_MACRO(pos == -1, ERR_NO_SUCH_FILE, 0);

    /* if it's a symlink, then we ran into a possible symlink loop. */
    is_sym = ( ((ZIPinfo *)(h->opaque))->entries[pos].symlink != NULL );
    BAIL_IF_MACRO(is_sym, ERR_SYMLINK_LOOP, 0);

    return(1);
} /* ZIP_exists */


static PHYSFS_sint64 ZIP_getLastModTime(DirHandle *h, const char *name)
{
    ZIPinfo *zi = (ZIPinfo *) (h->opaque);
    int pos = ZIP_exists_symcheck(h, name, SYMLINK_RECURSE_COUNT);
    ZIPentry *entry;

    BAIL_IF_MACRO(pos == -1, ERR_NO_SUCH_FILE, 0);

    entry = &zi->entries[pos];

    /* if it's a symlink, then we ran into a possible symlink loop. */
    BAIL_IF_MACRO(entry->symlink != NULL, ERR_SYMLINK_LOOP, 0);

    return(entry->last_mod_time);
} /* ZIP_getLastModTime */


static int ZIP_isDirectory(DirHandle *h, const char *name)
{
    int dlen;
    int is_sym;
    int pos = ZIP_exists_symcheck(h, name, SYMLINK_RECURSE_COUNT);

    BAIL_IF_MACRO(pos == -1, ERR_NO_SUCH_FILE, 0);

    /* if it's a symlink, then we ran into a possible symlink loop. */
    is_sym = ( ((ZIPinfo *)(h->opaque))->entries[pos].symlink != NULL );
    BAIL_IF_MACRO(is_sym, ERR_SYMLINK_LOOP, 0);

    dlen = strlen(name);

    /* !!! FIXME: yikes. Better way to check? */
    return((((ZIPinfo *)(h->opaque))->entries[pos].name[dlen] == '/'));
} /* ZIP_isDirectory */


static int ZIP_isSymLink(DirHandle *h, const char *name)
{
    int pos = ZIP_exists_symcheck(h, name, 0);
    BAIL_IF_MACRO(pos == -1, ERR_NO_SUCH_FILE, 0);
    return( ((ZIPinfo *)(h->opaque))->entries[pos].symlink != NULL );
} /* ZIP_isSymLink */


static FileHandle *ZIP_openRead(DirHandle *h, const char *filename)
{
    FileHandle *retval = NULL;
    ZIPinfo *zi = ((ZIPinfo *) (h->opaque));
    ZIPfileinfo *finfo = NULL;
    int pos = ZIP_exists_symcheck(h, filename, SYMLINK_RECURSE_COUNT);
    void *in;

    BAIL_IF_MACRO(pos == -1, ERR_NO_SUCH_FILE, NULL);

    /* if it's a symlink, then we ran into a possible symlink loop. */
    BAIL_IF_MACRO(zi->entries[pos].symlink != NULL, ERR_SYMLINK_LOOP, 0);

    in = __PHYSFS_platformOpenRead(zi->archiveName);
    BAIL_IF_MACRO(in == NULL, ERR_IO_ERROR, NULL);
    if (!zip_set_open_position(in, &zi->entries[pos]))
    {
        __PHYSFS_platformClose(in);
        return(0);
    } /* if */

    if ( ((retval = (FileHandle *) malloc(sizeof (FileHandle))) == NULL) ||
         ((finfo = (ZIPfileinfo *) malloc(sizeof (ZIPfileinfo))) == NULL) )
    {
        if (retval)
            free(retval);
        __PHYSFS_platformClose(in);
        BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    retval->opaque = (void *) finfo;
    retval->funcs = &__PHYSFS_FileFunctions_ZIP;
    retval->dirHandle = h;

    memset(finfo, '\0', sizeof (ZIPfileinfo));
    finfo->handle = in;
    finfo->entry = &zi->entries[pos];
    if (finfo->entry->compression_method != COMPMETH_NONE)
    {
        if (zlib_err(inflateInit2(&finfo->stream, -MAX_WBITS)) != Z_OK)
        {
            ZIP_fileClose(retval);
            return(NULL);
        } /* if */

        finfo->buffer = (PHYSFS_uint8 *) malloc(ZIP_READBUFSIZE);
        if (finfo->buffer == NULL)
        {
            ZIP_fileClose(retval);
            BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
        } /* if */
    } /* if */

    return(retval);
} /* ZIP_openRead */


static void ZIP_dirClose(DirHandle *h)
{
    ZIPinfo *zi = (ZIPinfo *) (h->opaque);
    freeEntries(zi, zi->entryCount);
    free(zi->archiveName);
    free(zi);
    free(h);
} /* ZIP_dirClose */

#endif  /* defined PHYSFS_SUPPORTS_ZIP */

/* end of zip.c ... */

