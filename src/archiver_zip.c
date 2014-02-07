/*
 * ZIP support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon, with some peeking at "unzip.c"
 *   by Gilles Vollant.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_ZIP

#include <errno.h>
#include <time.h>

#include "physfs_miniz.h"

/*
 * A buffer of ZIP_READBUFSIZE is allocated for each compressed file opened,
 *  and is freed when you close the file; compressed data is read into
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
 * Entries are "unresolved" until they are first opened. At that time,
 *  local file headers parsed/validated, data offsets will be updated to look
 *  at the actual file data instead of the header, and symlinks will be
 *  followed and optimized. This means that we don't seek and read around the
 *  archive until forced to do so, and after the first time, we had to do
 *  less reading and parsing, which is very CD-ROM friendly.
 */
typedef enum
{
    ZIP_UNRESOLVED_FILE,
    ZIP_UNRESOLVED_SYMLINK,
    ZIP_RESOLVING,
    ZIP_RESOLVED,
    ZIP_BROKEN_FILE,
    ZIP_BROKEN_SYMLINK
} ZipResolveType;


/*
 * One ZIPentry is kept for each file in an open ZIP archive.
 */
typedef struct _ZIPentry
{
    char *name;                         /* Name of file in archive        */
    struct _ZIPentry *symlink;          /* NULL or file we symlink to     */
    ZipResolveType resolved;            /* Have we resolved file/symlink? */
    PHYSFS_uint64 offset;               /* offset of data in archive      */
    PHYSFS_uint16 version;              /* version made by                */
    PHYSFS_uint16 version_needed;       /* version needed to extract      */
    PHYSFS_uint16 compression_method;   /* compression method             */
    PHYSFS_uint32 crc;                  /* crc-32                         */
    PHYSFS_uint64 compressed_size;      /* compressed size                */
    PHYSFS_uint64 uncompressed_size;    /* uncompressed size              */
    PHYSFS_sint64 last_mod_time;        /* last file mod time             */
} ZIPentry;

/*
 * One ZIPinfo is kept for each open ZIP archive.
 */
typedef struct
{
    PHYSFS_Io *io;
    int zip64;                /* non-zero if this is a Zip64 archive. */
    PHYSFS_uint64 entryCount; /* Number of files in ZIP.              */
    ZIPentry *entries;        /* info on all files in ZIP.            */
} ZIPinfo;

/*
 * One ZIPfileinfo is kept for each open file in a ZIP archive.
 */
typedef struct
{
    ZIPentry *entry;                      /* Info on file.              */
    PHYSFS_Io *io;                        /* physical file handle.      */
    PHYSFS_uint32 compressed_position;    /* offset in compressed data. */
    PHYSFS_uint32 uncompressed_position;  /* tell() position.           */
    PHYSFS_uint8 *buffer;                 /* decompression buffer.      */
    z_stream stream;                      /* zlib stream state.         */
} ZIPfileinfo;


/* Magic numbers... */
#define ZIP_LOCAL_FILE_SIG                          0x04034b50
#define ZIP_CENTRAL_DIR_SIG                         0x02014b50
#define ZIP_END_OF_CENTRAL_DIR_SIG                  0x06054b50
#define ZIP64_END_OF_CENTRAL_DIR_SIG                0x06064b50
#define ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIG  0x07064b50
#define ZIP64_EXTENDED_INFO_EXTRA_FIELD_SIG         0x0001

/* compression methods... */
#define COMPMETH_NONE 0
/* ...and others... */


#define UNIX_FILETYPE_MASK    0170000
#define UNIX_FILETYPE_SYMLINK 0120000


/*
 * Bridge physfs allocation functions to zlib's format...
 */
static voidpf zlibPhysfsAlloc(voidpf opaque, uInt items, uInt size)
{
    return ((PHYSFS_Allocator *) opaque)->Malloc(items * size);
} /* zlibPhysfsAlloc */

/*
 * Bridge physfs allocation functions to zlib's format...
 */
static void zlibPhysfsFree(voidpf opaque, voidpf address)
{
    ((PHYSFS_Allocator *) opaque)->Free(address);
} /* zlibPhysfsFree */


/*
 * Construct a new z_stream to a sane state.
 */
static void initializeZStream(z_stream *pstr)
{
    memset(pstr, '\0', sizeof (z_stream));
    pstr->zalloc = zlibPhysfsAlloc;
    pstr->zfree = zlibPhysfsFree;
    pstr->opaque = &allocator;
} /* initializeZStream */


static PHYSFS_ErrorCode zlib_error_code(int rc)
{
    switch (rc)
    {
        case Z_OK: return PHYSFS_ERR_OK;  /* not an error. */
        case Z_STREAM_END: return PHYSFS_ERR_OK; /* not an error. */
        case Z_ERRNO: return PHYSFS_ERR_IO;
        case Z_MEM_ERROR: return PHYSFS_ERR_OUT_OF_MEMORY;
        default: return PHYSFS_ERR_CORRUPT;
    } /* switch */
} /* zlib_error_string */


/*
 * Wrap all zlib calls in this, so the physfs error state is set appropriately.
 */
static int zlib_err(const int rc)
{
    PHYSFS_setErrorCode(zlib_error_code(rc));
    return rc;
} /* zlib_err */


/*
 * Read an unsigned 64-bit int and swap to native byte order.
 */
static int readui64(PHYSFS_Io *io, PHYSFS_uint64 *val)
{
    PHYSFS_uint64 v;
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &v, sizeof (v)), ERRPASS, 0);
    *val = PHYSFS_swapULE64(v);
    return 1;
} /* readui64 */

/*
 * Read an unsigned 32-bit int and swap to native byte order.
 */
static int readui32(PHYSFS_Io *io, PHYSFS_uint32 *val)
{
    PHYSFS_uint32 v;
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &v, sizeof (v)), ERRPASS, 0);
    *val = PHYSFS_swapULE32(v);
    return 1;
} /* readui32 */


/*
 * Read an unsigned 16-bit int and swap to native byte order.
 */
static int readui16(PHYSFS_Io *io, PHYSFS_uint16 *val)
{
    PHYSFS_uint16 v;
    BAIL_IF_MACRO(!__PHYSFS_readAll(io, &v, sizeof (v)), ERRPASS, 0);
    *val = PHYSFS_swapULE16(v);
    return 1;
} /* readui16 */


static PHYSFS_sint64 ZIP_read(PHYSFS_Io *_io, void *buf, PHYSFS_uint64 len)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) _io->opaque;
    PHYSFS_Io *io = finfo->io;
    ZIPentry *entry = finfo->entry;
    PHYSFS_sint64 retval = 0;
    PHYSFS_sint64 maxread = (PHYSFS_sint64) len;
    PHYSFS_sint64 avail = entry->uncompressed_size -
                          finfo->uncompressed_position;

    if (avail < maxread)
        maxread = avail;

    BAIL_IF_MACRO(maxread == 0, ERRPASS, 0);    /* quick rejection. */

    if (entry->compression_method == COMPMETH_NONE)
        retval = io->read(io, buf, maxread);
    else
    {
        finfo->stream.next_out = buf;
        finfo->stream.avail_out = (uInt) maxread;

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

                    br = io->read(io, finfo->buffer, (PHYSFS_uint64) br);
                    if (br <= 0)
                        break;

                    finfo->compressed_position += (PHYSFS_uint32) br;
                    finfo->stream.next_in = finfo->buffer;
                    finfo->stream.avail_in = (PHYSFS_uint32) br;
                } /* if */
            } /* if */

            rc = zlib_err(inflate(&finfo->stream, Z_SYNC_FLUSH));
            retval += (finfo->stream.total_out - before);

            if (rc != Z_OK)
                break;
        } /* while */
    } /* else */

    if (retval > 0)
        finfo->uncompressed_position += (PHYSFS_uint32) retval;

    return retval;
} /* ZIP_read */


static PHYSFS_sint64 ZIP_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, -1);
} /* ZIP_write */


static PHYSFS_sint64 ZIP_tell(PHYSFS_Io *io)
{
    return ((ZIPfileinfo *) io->opaque)->uncompressed_position;
} /* ZIP_tell */


static int ZIP_seek(PHYSFS_Io *_io, PHYSFS_uint64 offset)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) _io->opaque;
    ZIPentry *entry = finfo->entry;
    PHYSFS_Io *io = finfo->io;

    BAIL_IF_MACRO(offset > entry->uncompressed_size, PHYSFS_ERR_PAST_EOF, 0);

    if (entry->compression_method == COMPMETH_NONE)
    {
        const PHYSFS_sint64 newpos = offset + entry->offset;
        BAIL_IF_MACRO(!io->seek(io, newpos), ERRPASS, 0);
        finfo->uncompressed_position = (PHYSFS_uint32) offset;
    } /* if */

    else
    {
        /*
         * If seeking backwards, we need to redecode the file
         *  from the start and throw away the compressed bits until we hit
         *  the offset we need. If seeking forward, we still need to
         *  decode, but we don't rewind first.
         */
        if (offset < finfo->uncompressed_position)
        {
            /* we do a copy so state is sane if inflateInit2() fails. */
            z_stream str;
            initializeZStream(&str);
            if (zlib_err(inflateInit2(&str, -MAX_WBITS)) != Z_OK)
                return 0;

            if (!io->seek(io, entry->offset))
                return 0;

            inflateEnd(&finfo->stream);
            memcpy(&finfo->stream, &str, sizeof (z_stream));
            finfo->uncompressed_position = finfo->compressed_position = 0;
        } /* if */

        while (finfo->uncompressed_position != offset)
        {
            PHYSFS_uint8 buf[512];
            PHYSFS_uint32 maxread;

            maxread = (PHYSFS_uint32) (offset - finfo->uncompressed_position);
            if (maxread > sizeof (buf))
                maxread = sizeof (buf);

            if (ZIP_read(_io, buf, maxread) != maxread)
                return 0;
        } /* while */
    } /* else */

    return 1;
} /* ZIP_seek */


static PHYSFS_sint64 ZIP_length(PHYSFS_Io *io)
{
    const ZIPfileinfo *finfo = (ZIPfileinfo *) io->opaque;
    return (PHYSFS_sint64) finfo->entry->uncompressed_size;
} /* ZIP_length */


static PHYSFS_Io *zip_get_io(PHYSFS_Io *io, ZIPinfo *inf, ZIPentry *entry);

static PHYSFS_Io *ZIP_duplicate(PHYSFS_Io *io)
{
    ZIPfileinfo *origfinfo = (ZIPfileinfo *) io->opaque;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    ZIPfileinfo *finfo = (ZIPfileinfo *) allocator.Malloc(sizeof (ZIPfileinfo));
    GOTO_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, failed);
    GOTO_IF_MACRO(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, failed);
    memset(finfo, '\0', sizeof (*finfo));

    finfo->entry = origfinfo->entry;
    finfo->io = zip_get_io(origfinfo->io, NULL, finfo->entry);
    GOTO_IF_MACRO(!finfo->io, ERRPASS, failed);

    if (finfo->entry->compression_method != COMPMETH_NONE)
    {
        finfo->buffer = (PHYSFS_uint8 *) allocator.Malloc(ZIP_READBUFSIZE);
        GOTO_IF_MACRO(!finfo->buffer, PHYSFS_ERR_OUT_OF_MEMORY, failed);
        if (zlib_err(inflateInit2(&finfo->stream, -MAX_WBITS)) != Z_OK)
            goto failed;
    } /* if */

    memcpy(retval, io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);

        if (finfo->buffer != NULL)
        {
            allocator.Free(finfo->buffer);
            inflateEnd(&finfo->stream);
        } /* if */

        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* ZIP_duplicate */

static int ZIP_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void ZIP_destroy(PHYSFS_Io *io)
{
    ZIPfileinfo *finfo = (ZIPfileinfo *) io->opaque;
    finfo->io->destroy(finfo->io);

    if (finfo->entry->compression_method != COMPMETH_NONE)
        inflateEnd(&finfo->stream);

    if (finfo->buffer != NULL)
        allocator.Free(finfo->buffer);

    allocator.Free(finfo);
    allocator.Free(io);
} /* ZIP_destroy */


static const PHYSFS_Io ZIP_Io =
{
    CURRENT_PHYSFS_IO_API_VERSION, NULL,
    ZIP_read,
    ZIP_write,
    ZIP_seek,
    ZIP_tell,
    ZIP_length,
    ZIP_duplicate,
    ZIP_flush,
    ZIP_destroy
};



static PHYSFS_sint64 zip_find_end_of_central_dir(PHYSFS_Io *io, PHYSFS_sint64 *len)
{
    PHYSFS_uint8 buf[256];
    PHYSFS_uint8 extra[4] = { 0, 0, 0, 0 };
    PHYSFS_sint32 i = 0;
    PHYSFS_sint64 filelen;
    PHYSFS_sint64 filepos;
    PHYSFS_sint32 maxread;
    PHYSFS_sint32 totalread = 0;
    int found = 0;

    filelen = io->length(io);
    BAIL_IF_MACRO(filelen == -1, ERRPASS, 0);

    /*
     * Jump to the end of the file and start reading backwards.
     *  The last thing in the file is the zipfile comment, which is variable
     *  length, and the field that specifies its size is before it in the
     *  file (argh!)...this means that we need to scan backwards until we
     *  hit the end-of-central-dir signature. We can then sanity check that
     *  the comment was as big as it should be to make sure we're in the
     *  right place. The comment length field is 16 bits, so we can stop
     *  searching for that signature after a little more than 64k at most,
     *  and call it a corrupted zipfile.
     */

    if (sizeof (buf) < filelen)
    {
        filepos = filelen - sizeof (buf);
        maxread = sizeof (buf);
    } /* if */
    else
    {
        filepos = 0;
        maxread = (PHYSFS_uint32) filelen;
    } /* else */

    while ((totalread < filelen) && (totalread < 65557))
    {
        BAIL_IF_MACRO(!io->seek(io, filepos), ERRPASS, -1);

        /* make sure we catch a signature between buffers. */
        if (totalread != 0)
        {
            if (!__PHYSFS_readAll(io, buf, maxread - 4))
                return -1;
            memcpy(&buf[maxread - 4], &extra, sizeof (extra));
            totalread += maxread - 4;
        } /* if */
        else
        {
            if (!__PHYSFS_readAll(io, buf, maxread))
                return -1;
            totalread += maxread;
        } /* else */

        memcpy(&extra, buf, sizeof (extra));

        for (i = maxread - 4; i > 0; i--)
        {
            if ((buf[i + 0] == 0x50) &&
                (buf[i + 1] == 0x4B) &&
                (buf[i + 2] == 0x05) &&
                (buf[i + 3] == 0x06) )
            {
                found = 1;  /* that's the signature! */
                break;  
            } /* if */
        } /* for */

        if (found)
            break;

        filepos -= (maxread - 4);
        if (filepos < 0)
            filepos = 0;
    } /* while */

    BAIL_IF_MACRO(!found, PHYSFS_ERR_UNSUPPORTED, -1);

    if (len != NULL)
        *len = filelen;

    return (filepos + i);
} /* zip_find_end_of_central_dir */


static int isZip(PHYSFS_Io *io)
{
    PHYSFS_uint32 sig = 0;
    int retval = 0;

    /*
     * The first thing in a zip file might be the signature of the
     *  first local file record, so it makes for a quick determination.
     */
    if (readui32(io, &sig))
    {
        retval = (sig == ZIP_LOCAL_FILE_SIG);
        if (!retval)
        {
            /*
             * No sig...might be a ZIP with data at the start
             *  (a self-extracting executable, etc), so we'll have to do
             *  it the hard way...
             */
            retval = (zip_find_end_of_central_dir(io, NULL) != -1);
        } /* if */
    } /* if */

    return retval;
} /* isZip */


static void zip_free_entries(ZIPentry *entries, PHYSFS_uint64 max)
{
    PHYSFS_uint64 i;
    for (i = 0; i < max; i++)
    {
        ZIPentry *entry = &entries[i];
        if (entry->name != NULL)
            allocator.Free(entry->name);
    } /* for */

    allocator.Free(entries);
} /* zip_free_entries */


/*
 * This will find the ZIPentry associated with a path in platform-independent
 *  notation. Directories don't have ZIPentries associated with them, but 
 *  (*isDir) will be set to non-zero if a dir was hit.
 */
static ZIPentry *zip_find_entry(const ZIPinfo *info, const char *path,
                                int *isDir)
{
    ZIPentry *a = info->entries;
    PHYSFS_sint32 pathlen = (PHYSFS_sint32) strlen(path);
    PHYSFS_sint64 lo = 0;
    PHYSFS_sint64 hi = (PHYSFS_sint64) (info->entryCount - 1);
    PHYSFS_sint64 middle;
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

    BAIL_MACRO(PHYSFS_ERR_NOT_FOUND, NULL);
} /* zip_find_entry */


/* Convert paths from old, buggy DOS zippers... */
static void zip_convert_dos_path(ZIPentry *entry, char *path)
{
    PHYSFS_uint8 hosttype = (PHYSFS_uint8) ((entry->version >> 8) & 0xFF);
    if (hosttype == 0)  /* FS_FAT_ */
    {
        while (*path)
        {
            if (*path == '\\')
                *path = '/';
            path++;
        } /* while */
    } /* if */
} /* zip_convert_dos_path */


static void zip_expand_symlink_path(char *path)
{
    char *ptr = path;
    char *prevptr = path;

    while (1)
    {
        ptr = strchr(ptr, '/');
        if (ptr == NULL)
            break;

        if (*(ptr + 1) == '.')
        {
            if (*(ptr + 2) == '/')
            {
                /* current dir in middle of string: ditch it. */
                memmove(ptr, ptr + 2, strlen(ptr + 2) + 1);
            } /* else if */

            else if (*(ptr + 2) == '\0')
            {
                /* current dir at end of string: ditch it. */
                *ptr = '\0';
            } /* else if */

            else if (*(ptr + 2) == '.')
            {
                if (*(ptr + 3) == '/')
                {
                    /* parent dir in middle: move back one, if possible. */
                    memmove(prevptr, ptr + 4, strlen(ptr + 4) + 1);
                    ptr = prevptr;
                    while (prevptr != path)
                    {
                        prevptr--;
                        if (*prevptr == '/')
                        {
                            prevptr++;
                            break;
                        } /* if */
                    } /* while */
                } /* if */

                if (*(ptr + 3) == '\0')
                {
                    /* parent dir at end: move back one, if possible. */
                    *prevptr = '\0';
                } /* if */
            } /* if */
        } /* if */
        else
        {
            prevptr = ptr;
            ptr++;
        } /* else */
    } /* while */
} /* zip_expand_symlink_path */

/* (forward reference: zip_follow_symlink and zip_resolve call each other.) */
static int zip_resolve(PHYSFS_Io *io, ZIPinfo *info, ZIPentry *entry);

/*
 * Look for the entry named by (path). If it exists, resolve it, and return
 *  a pointer to that entry. If it's another symlink, keep resolving until you
 *  hit a real file and then return a pointer to the final non-symlink entry.
 *  If there's a problem, return NULL.
 */
static ZIPentry *zip_follow_symlink(PHYSFS_Io *io, ZIPinfo *info, char *path)
{
    ZIPentry *entry;

    zip_expand_symlink_path(path);
    entry = zip_find_entry(info, path, NULL);
    if (entry != NULL)
    {
        if (!zip_resolve(io, info, entry))  /* recursive! */
            entry = NULL;
        else
        {
            if (entry->symlink != NULL)
                entry = entry->symlink;
        } /* else */
    } /* if */

    return entry;
} /* zip_follow_symlink */


static int zip_resolve_symlink(PHYSFS_Io *io, ZIPinfo *info, ZIPentry *entry)
{
    const PHYSFS_uint64 size = entry->uncompressed_size;
    char *path = NULL;
    int rc = 0;

    /*
     * We've already parsed the local file header of the symlink at this
     *  point. Now we need to read the actual link from the file data and
     *  follow it.
     */

    BAIL_IF_MACRO(!io->seek(io, entry->offset), ERRPASS, 0);

    path = (char *) __PHYSFS_smallAlloc(size + 1);
    BAIL_IF_MACRO(!path, PHYSFS_ERR_OUT_OF_MEMORY, 0);
    
    if (entry->compression_method == COMPMETH_NONE)
        rc = __PHYSFS_readAll(io, path, size);

    else  /* symlink target path is compressed... */
    {
        z_stream stream;
        const PHYSFS_uint64 complen = entry->compressed_size;
        PHYSFS_uint8 *compressed = (PHYSFS_uint8*) __PHYSFS_smallAlloc(complen);
        if (compressed != NULL)
        {
            if (__PHYSFS_readAll(io, compressed, complen))
            {
                initializeZStream(&stream);
                stream.next_in = compressed;
                stream.avail_in = complen;
                stream.next_out = (unsigned char *) path;
                stream.avail_out = size;
                if (zlib_err(inflateInit2(&stream, -MAX_WBITS)) == Z_OK)
                {
                    rc = zlib_err(inflate(&stream, Z_FINISH));
                    inflateEnd(&stream);

                    /* both are acceptable outcomes... */
                    rc = ((rc == Z_OK) || (rc == Z_STREAM_END));
                } /* if */
            } /* if */
            __PHYSFS_smallFree(compressed);
        } /* if */
    } /* else */

    if (rc)
    {
        path[entry->uncompressed_size] = '\0';    /* null-terminate it. */
        zip_convert_dos_path(entry, path);
        entry->symlink = zip_follow_symlink(io, info, path);
    } /* else */

    __PHYSFS_smallFree(path);

    return (entry->symlink != NULL);
} /* zip_resolve_symlink */


/*
 * Parse the local file header of an entry, and update entry->offset.
 */
static int zip_parse_local(PHYSFS_Io *io, ZIPentry *entry)
{
    PHYSFS_uint32 ui32;
    PHYSFS_uint16 ui16;
    PHYSFS_uint16 fnamelen;
    PHYSFS_uint16 extralen;

    /*
     * crc and (un)compressed_size are always zero if this is a "JAR"
     *  archive created with Sun's Java tools, apparently. We only
     *  consider this archive corrupted if those entries don't match and
     *  aren't zero. That seems to work well.
     * We also ignore a mismatch if the value is 0xFFFFFFFF here, since it's
     *  possible that's a Zip64 thing.
     */

    BAIL_IF_MACRO(!io->seek(io, entry->offset), ERRPASS, 0);
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != ZIP_LOCAL_FILE_SIG, PHYSFS_ERR_CORRUPT, 0);
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);
    BAIL_IF_MACRO(ui16 != entry->version_needed, PHYSFS_ERR_CORRUPT, 0);
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);  /* general bits. */
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);
    BAIL_IF_MACRO(ui16 != entry->compression_method, PHYSFS_ERR_CORRUPT, 0);
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);  /* date/time */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 && (ui32 != entry->crc), PHYSFS_ERR_CORRUPT, 0);

    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 && (ui32 != 0xFFFFFFFF) &&
                  (ui32 != entry->compressed_size), PHYSFS_ERR_CORRUPT, 0);

    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 && (ui32 != 0xFFFFFFFF) &&
                 (ui32 != entry->uncompressed_size), PHYSFS_ERR_CORRUPT, 0);

    BAIL_IF_MACRO(!readui16(io, &fnamelen), ERRPASS, 0);
    BAIL_IF_MACRO(!readui16(io, &extralen), ERRPASS, 0);

    entry->offset += fnamelen + extralen + 30;
    return 1;
} /* zip_parse_local */


static int zip_resolve(PHYSFS_Io *io, ZIPinfo *info, ZIPentry *entry)
{
    int retval = 1;
    ZipResolveType resolve_type = entry->resolved;

    /* Don't bother if we've failed to resolve this entry before. */
    BAIL_IF_MACRO(resolve_type == ZIP_BROKEN_FILE, PHYSFS_ERR_CORRUPT, 0);
    BAIL_IF_MACRO(resolve_type == ZIP_BROKEN_SYMLINK, PHYSFS_ERR_CORRUPT, 0);

    /* uhoh...infinite symlink loop! */
    BAIL_IF_MACRO(resolve_type == ZIP_RESOLVING, PHYSFS_ERR_SYMLINK_LOOP, 0);

    /*
     * We fix up the offset to point to the actual data on the
     *  first open, since we don't want to seek across the whole file on
     *  archive open (can be SLOW on large, CD-stored files), but we
     *  need to check the local file header...not just for corruption,
     *  but since it stores offset info the central directory does not.
     */
    if (resolve_type != ZIP_RESOLVED)
    {
        entry->resolved = ZIP_RESOLVING;

        retval = zip_parse_local(io, entry);
        if (retval)
        {
            /*
             * If it's a symlink, find the original file. This will cause
             *  resolution of other entries (other symlinks and, eventually,
             *  the real file) if all goes well.
             */
            if (resolve_type == ZIP_UNRESOLVED_SYMLINK)
                retval = zip_resolve_symlink(io, info, entry);
        } /* if */

        if (resolve_type == ZIP_UNRESOLVED_SYMLINK)
            entry->resolved = ((retval) ? ZIP_RESOLVED : ZIP_BROKEN_SYMLINK);
        else if (resolve_type == ZIP_UNRESOLVED_FILE)
            entry->resolved = ((retval) ? ZIP_RESOLVED : ZIP_BROKEN_FILE);
    } /* if */

    return retval;
} /* zip_resolve */


static int zip_version_does_symlinks(PHYSFS_uint32 version)
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

    return retval;
} /* zip_version_does_symlinks */


static int zip_entry_is_symlink(const ZIPentry *entry)
{
    return ((entry->resolved == ZIP_UNRESOLVED_SYMLINK) ||
            (entry->resolved == ZIP_BROKEN_SYMLINK) ||
            (entry->symlink));
} /* zip_entry_is_symlink */


static int zip_has_symlink_attr(ZIPentry *entry, PHYSFS_uint32 extern_attr)
{
    PHYSFS_uint16 xattr = ((extern_attr >> 16) & 0xFFFF);
    return ( (zip_version_does_symlinks(entry->version)) &&
             (entry->uncompressed_size > 0) &&
             ((xattr & UNIX_FILETYPE_MASK) == UNIX_FILETYPE_SYMLINK) );
} /* zip_has_symlink_attr */


static PHYSFS_sint64 zip_dos_time_to_physfs_time(PHYSFS_uint32 dostime)
{
    PHYSFS_uint32 dosdate;
    struct tm unixtime;
    memset(&unixtime, '\0', sizeof (unixtime));

    dosdate = (PHYSFS_uint32) ((dostime >> 16) & 0xFFFF);
    dostime &= 0xFFFF;

    /* dissect date */
    unixtime.tm_year = ((dosdate >> 9) & 0x7F) + 80;
    unixtime.tm_mon  = ((dosdate >> 5) & 0x0F) - 1;
    unixtime.tm_mday = ((dosdate     ) & 0x1F);

    /* dissect time */
    unixtime.tm_hour = ((dostime >> 11) & 0x1F);
    unixtime.tm_min  = ((dostime >>  5) & 0x3F);
    unixtime.tm_sec  = ((dostime <<  1) & 0x3E);

    /* let mktime calculate daylight savings time. */
    unixtime.tm_isdst = -1;

    return ((PHYSFS_sint64) mktime(&unixtime));
} /* zip_dos_time_to_physfs_time */


static int zip_load_entry(PHYSFS_Io *io, const int zip64, ZIPentry *entry,
                          PHYSFS_uint64 ofs_fixup)
{
    PHYSFS_uint16 fnamelen, extralen, commentlen;
    PHYSFS_uint32 external_attr;
    PHYSFS_uint32 starting_disk;
    PHYSFS_uint64 offset;
    PHYSFS_uint16 ui16;
    PHYSFS_uint32 ui32;
    PHYSFS_sint64 si64;

    /* sanity check with central directory signature... */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != ZIP_CENTRAL_DIR_SIG, PHYSFS_ERR_CORRUPT, 0);

    /* Get the pertinent parts of the record... */
    BAIL_IF_MACRO(!readui16(io, &entry->version), ERRPASS, 0);
    BAIL_IF_MACRO(!readui16(io, &entry->version_needed), ERRPASS, 0);
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);  /* general bits */
    BAIL_IF_MACRO(!readui16(io, &entry->compression_method), ERRPASS, 0);
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    entry->last_mod_time = zip_dos_time_to_physfs_time(ui32);
    BAIL_IF_MACRO(!readui32(io, &entry->crc), ERRPASS, 0);
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    entry->compressed_size = (PHYSFS_uint64) ui32;
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    entry->uncompressed_size = (PHYSFS_uint64) ui32;
    BAIL_IF_MACRO(!readui16(io, &fnamelen), ERRPASS, 0);
    BAIL_IF_MACRO(!readui16(io, &extralen), ERRPASS, 0);
    BAIL_IF_MACRO(!readui16(io, &commentlen), ERRPASS, 0);
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);
    starting_disk = (PHYSFS_uint32) ui16;
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);  /* internal file attribs */
    BAIL_IF_MACRO(!readui32(io, &external_attr), ERRPASS, 0);
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    offset = (PHYSFS_uint64) ui32;

    entry->symlink = NULL;  /* will be resolved later, if necessary. */
    entry->resolved = (zip_has_symlink_attr(entry, external_attr)) ?
                            ZIP_UNRESOLVED_SYMLINK : ZIP_UNRESOLVED_FILE;

    entry->name = (char *) allocator.Malloc(fnamelen + 1);
    BAIL_IF_MACRO(entry->name == NULL, PHYSFS_ERR_OUT_OF_MEMORY, 0);
    if (!__PHYSFS_readAll(io, entry->name, fnamelen))
        goto zip_load_entry_puked;

    entry->name[fnamelen] = '\0';  /* null-terminate the filename. */
    zip_convert_dos_path(entry, entry->name);

    si64 = io->tell(io);
    if (si64 == -1)
        goto zip_load_entry_puked;

    /*
     * The actual sizes didn't fit in 32-bits; look for the Zip64
     *  extended information extra field...
     */
    if ( (zip64) &&
         ((offset == 0xFFFFFFFF) ||
          (starting_disk == 0xFFFFFFFF) ||
          (entry->compressed_size == 0xFFFFFFFF) ||
          (entry->uncompressed_size == 0xFFFFFFFF)) )
    {
        int found = 0;
        PHYSFS_uint16 sig, len;
        while (extralen > 4)
        {
            if (!readui16(io, &sig))
                goto zip_load_entry_puked;
            else if (!readui16(io, &len))
                goto zip_load_entry_puked;

            si64 += 4 + len;
            extralen -= 4 + len;
            if (sig != ZIP64_EXTENDED_INFO_EXTRA_FIELD_SIG)
            {
                if (!io->seek(io, si64))
                    goto zip_load_entry_puked;
                continue;
            } /* if */

            found = 1;
            break;
        } /* while */

        GOTO_IF_MACRO(!found, PHYSFS_ERR_CORRUPT, zip_load_entry_puked);

        if (entry->uncompressed_size == 0xFFFFFFFF)
        {
            GOTO_IF_MACRO(len < 8, PHYSFS_ERR_CORRUPT, zip_load_entry_puked);
            if (!readui64(io, &entry->uncompressed_size))
                goto zip_load_entry_puked;
            len -= 8;
        } /* if */

        if (entry->compressed_size == 0xFFFFFFFF)
        {
            GOTO_IF_MACRO(len < 8, PHYSFS_ERR_CORRUPT, zip_load_entry_puked);
            if (!readui64(io, &entry->compressed_size))
                goto zip_load_entry_puked;
            len -= 8;
        } /* if */

        if (offset == 0xFFFFFFFF)
        {
            GOTO_IF_MACRO(len < 8, PHYSFS_ERR_CORRUPT, zip_load_entry_puked);
            if (!readui64(io, &offset))
                goto zip_load_entry_puked;
            len -= 8;
        } /* if */

        if (starting_disk == 0xFFFFFFFF)
        {
            GOTO_IF_MACRO(len < 8, PHYSFS_ERR_CORRUPT, zip_load_entry_puked);
            if (!readui32(io, &starting_disk))
                goto zip_load_entry_puked;
            len -= 4;
        } /* if */

        GOTO_IF_MACRO(len != 0, PHYSFS_ERR_CORRUPT, zip_load_entry_puked);
    } /* if */

    GOTO_IF_MACRO(starting_disk != 0, PHYSFS_ERR_CORRUPT, zip_load_entry_puked);

    entry->offset = offset + ofs_fixup;

    /* seek to the start of the next entry in the central directory... */
    if (!io->seek(io, si64 + extralen + commentlen))
        goto zip_load_entry_puked;

    return 1;  /* success. */

zip_load_entry_puked:
    allocator.Free(entry->name);
    return 0;  /* failure. */
} /* zip_load_entry */


static int zip_entry_cmp(void *_a, size_t one, size_t two)
{
    if (one != two)
    {
        const ZIPentry *a = (const ZIPentry *) _a;
        return strcmp(a[one].name, a[two].name);
    } /* if */

    return 0;
} /* zip_entry_cmp */


static void zip_entry_swap(void *_a, size_t one, size_t two)
{
    if (one != two)
    {
        ZIPentry tmp;
        ZIPentry *first = &(((ZIPentry *) _a)[one]);
        ZIPentry *second = &(((ZIPentry *) _a)[two]);
        memcpy(&tmp, first, sizeof (ZIPentry));
        memcpy(first, second, sizeof (ZIPentry));
        memcpy(second, &tmp, sizeof (ZIPentry));
    } /* if */
} /* zip_entry_swap */


static int zip_load_entries(PHYSFS_Io *io, ZIPinfo *info,
                            const PHYSFS_uint64 data_ofs,
                            const PHYSFS_uint64 central_ofs)
{
    const PHYSFS_uint64 max = info->entryCount;
    const int zip64 = info->zip64;
    PHYSFS_uint64 i;

    BAIL_IF_MACRO(!io->seek(io, central_ofs), ERRPASS, 0);

    info->entries = (ZIPentry *) allocator.Malloc(sizeof (ZIPentry) * max);
    BAIL_IF_MACRO(!info->entries, PHYSFS_ERR_OUT_OF_MEMORY, 0);

    for (i = 0; i < max; i++)
    {
        if (!zip_load_entry(io, zip64, &info->entries[i], data_ofs))
        {
            zip_free_entries(info->entries, i);
            return 0;
        } /* if */
    } /* for */

    __PHYSFS_sort(info->entries, (size_t) max, zip_entry_cmp, zip_entry_swap);
    return 1;
} /* zip_load_entries */


static PHYSFS_sint64 zip64_find_end_of_central_dir(PHYSFS_Io *io,
                                                   PHYSFS_sint64 _pos,
                                                   PHYSFS_uint64 offset)
{
    /*
     * Naturally, the offset is useless to us; it is the offset from the
     *  start of file, which is meaningless if we've appended this .zip to
     *  a self-extracting .exe. We need to find this on our own. It should
     *  be directly before the locator record, but the record in question,
     *  like the original end-of-central-directory record, ends with a
     *  variable-length field. Unlike the original, which has to store the
     *  size of that variable-length field in a 16-bit int and thus has to be
     *  within 64k, the new one gets 64-bits.
     *
     * Fortunately, the only currently-specified record for that variable
     *  length block is some weird proprietary thing that deals with EBCDIC
     *  and tape backups or something. So we don't seek far.
     */

    PHYSFS_uint32 ui32;
    const PHYSFS_uint64 pos = (PHYSFS_uint64) _pos;

    assert(_pos > 0);

    /* Try offset specified in the Zip64 end of central directory locator. */
    /* This works if the entire PHYSFS_Io is the zip file. */
    BAIL_IF_MACRO(!io->seek(io, offset), ERRPASS, -1);
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, -1);
    if (ui32 == ZIP64_END_OF_CENTRAL_DIR_SIG)
        return offset;

    /* Try 56 bytes before the Zip64 end of central directory locator. */
    /* This works if the record isn't variable length and is version 1. */
    if (pos > 56)
    {
        BAIL_IF_MACRO(!io->seek(io, pos-56), ERRPASS, -1);
        BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, -1);
        if (ui32 == ZIP64_END_OF_CENTRAL_DIR_SIG)
            return pos-56;
    } /* if */

    /* Try 84 bytes before the Zip64 end of central directory locator. */
    /* This works if the record isn't variable length and is version 2. */
    if (pos > 84)
    {
        BAIL_IF_MACRO(!io->seek(io, pos-84), ERRPASS, -1);
        BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, -1);
        if (ui32 == ZIP64_END_OF_CENTRAL_DIR_SIG)
            return pos-84;
    } /* if */

    /* Ok, brute force: we know it's between (offset) and (pos) somewhere. */
    /*  Just try moving back at most 256k. Oh well. */
    if ((offset < pos) && (pos > 4))
    {
        /* we assume you can eat this stack if you handle Zip64 files. */
        PHYSFS_uint8 buf[256 * 1024];
        PHYSFS_uint64 len = pos - offset;
        PHYSFS_sint32 i;

        if (len > sizeof (buf))
            len = sizeof (buf);

        BAIL_IF_MACRO(!io->seek(io, pos - len), ERRPASS, -1);
        BAIL_IF_MACRO(!__PHYSFS_readAll(io, buf, len), ERRPASS, -1);
        for (i = (PHYSFS_sint32) (len - 4); i >= 0; i--)
        {
            if (buf[i] != 0x50)
                continue;
            if ( (buf[i+1] == 0x4b) &&
                 (buf[i+2] == 0x06) &&
                 (buf[i+3] == 0x06) )
                return pos - (len - i);
        } /* for */
    } /* if */

    BAIL_MACRO(PHYSFS_ERR_CORRUPT, -1);  /* didn't find it. */
} /* zip64_find_end_of_central_dir */


static int zip64_parse_end_of_central_dir(PHYSFS_Io *io, ZIPinfo *info,
                                          PHYSFS_uint64 *data_start,
                                          PHYSFS_uint64 *dir_ofs,
                                          PHYSFS_sint64 pos)
{
    PHYSFS_uint64 ui64;
    PHYSFS_uint32 ui32;
    PHYSFS_uint16 ui16;

    /* We should be positioned right past the locator signature. */

    if ((pos < 0) || (!io->seek(io, pos)))
        return 0;

    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    if (ui32 != ZIP64_END_OF_CENTRAL_DIRECTORY_LOCATOR_SIG)
        return -1;  /* it's not a Zip64 archive. Not an error, though! */

    info->zip64 = 1;

    /* number of the disk with the start of the central directory. */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != 0, PHYSFS_ERR_CORRUPT, 0);

    /* offset of Zip64 end of central directory record. */
    BAIL_IF_MACRO(!readui64(io, &ui64), ERRPASS, 0);

    /* total number of disks */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != 1, PHYSFS_ERR_CORRUPT, 0);

    pos = zip64_find_end_of_central_dir(io, pos, ui64);
    if (pos < 0)
        return 0;  /* oh well. */

    /*
     * For self-extracting archives, etc, there's crapola in the file
     *  before the zipfile records; we calculate how much data there is
     *  prepended by determining how far the zip64-end-of-central-directory
     *  offset is from where it is supposed to be...the difference in bytes
     *  is how much arbitrary data is at the start of the physical file.
     */
    assert(((PHYSFS_uint64) pos) >= ui64);
    *data_start = ((PHYSFS_uint64) pos) - ui64;

    BAIL_IF_MACRO(!io->seek(io, pos), ERRPASS, 0);

    /* check signature again, just in case. */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != ZIP64_END_OF_CENTRAL_DIR_SIG, PHYSFS_ERR_CORRUPT, 0);

    /* size of Zip64 end of central directory record. */
    BAIL_IF_MACRO(!readui64(io, &ui64), ERRPASS, 0);

    /* version made by. */
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);

    /* version needed to extract. */
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);

    /* number of this disk. */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != 0, PHYSFS_ERR_CORRUPT, 0);

    /* number of disk with start of central directory record. */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != 0, PHYSFS_ERR_CORRUPT, 0);

    /* total number of entries in the central dir on this disk */
    BAIL_IF_MACRO(!readui64(io, &ui64), ERRPASS, 0);

    /* total number of entries in the central dir */
    BAIL_IF_MACRO(!readui64(io, &info->entryCount), ERRPASS, 0);
    BAIL_IF_MACRO(ui64 != info->entryCount, PHYSFS_ERR_CORRUPT, 0);

    /* size of the central directory */
    BAIL_IF_MACRO(!readui64(io, &ui64), ERRPASS, 0);

    /* offset of central directory */
    BAIL_IF_MACRO(!readui64(io, dir_ofs), ERRPASS, 0);

    /* Since we know the difference, fix up the central dir offset... */
    *dir_ofs += *data_start;

    /*
     * There are more fields here, for encryption and feature-specific things,
     *  but we don't care about any of them at the moment.
     */

    return 1;  /* made it. */
} /* zip64_parse_end_of_central_dir */


static int zip_parse_end_of_central_dir(PHYSFS_Io *io, ZIPinfo *info,
                                        PHYSFS_uint64 *data_start,
                                        PHYSFS_uint64 *dir_ofs)
{
    PHYSFS_uint16 entryCount16;
    PHYSFS_uint32 offset32;
    PHYSFS_uint32 ui32;
    PHYSFS_uint16 ui16;
    PHYSFS_sint64 len;
    PHYSFS_sint64 pos;
    int rc;

    /* find the end-of-central-dir record, and seek to it. */
    pos = zip_find_end_of_central_dir(io, &len);
    BAIL_IF_MACRO(pos == -1, ERRPASS, 0);
    BAIL_IF_MACRO(!io->seek(io, pos), ERRPASS, 0);

    /* check signature again, just in case. */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);
    BAIL_IF_MACRO(ui32 != ZIP_END_OF_CENTRAL_DIR_SIG, PHYSFS_ERR_CORRUPT, 0);

    /* Seek back to see if "Zip64 end of central directory locator" exists. */
    /* this record is 20 bytes before end-of-central-dir */
    rc = zip64_parse_end_of_central_dir(io, info, data_start, dir_ofs, pos-20);
    BAIL_IF_MACRO(rc == 0, ERRPASS, 0);
    if (rc == 1)
        return 1;  /* we're done here. */

    assert(rc == -1);  /* no error, just not a Zip64 archive. */

    /* Not Zip64? Seek back to where we were and keep processing. */
    BAIL_IF_MACRO(!io->seek(io, pos + 4), ERRPASS, 0);

    /* number of this disk */
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);
    BAIL_IF_MACRO(ui16 != 0, PHYSFS_ERR_CORRUPT, 0);

    /* number of the disk with the start of the central directory */
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);
    BAIL_IF_MACRO(ui16 != 0, PHYSFS_ERR_CORRUPT, 0);

    /* total number of entries in the central dir on this disk */
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);

    /* total number of entries in the central dir */
    BAIL_IF_MACRO(!readui16(io, &entryCount16), ERRPASS, 0);
    BAIL_IF_MACRO(ui16 != entryCount16, PHYSFS_ERR_CORRUPT, 0);

    info->entryCount = entryCount16;

    /* size of the central directory */
    BAIL_IF_MACRO(!readui32(io, &ui32), ERRPASS, 0);

    /* offset of central directory */
    BAIL_IF_MACRO(!readui32(io, &offset32), ERRPASS, 0);
    *dir_ofs = (PHYSFS_uint64) offset32;
    BAIL_IF_MACRO(pos < (*dir_ofs + ui32), PHYSFS_ERR_CORRUPT, 0);

    /*
     * For self-extracting archives, etc, there's crapola in the file
     *  before the zipfile records; we calculate how much data there is
     *  prepended by determining how far the central directory offset is
     *  from where it is supposed to be (start of end-of-central-dir minus
     *  sizeof central dir)...the difference in bytes is how much arbitrary
     *  data is at the start of the physical file.
     */
    *data_start = (PHYSFS_uint64) (pos - (*dir_ofs + ui32));

    /* Now that we know the difference, fix up the central dir offset... */
    *dir_ofs += *data_start;

    /* zipfile comment length */
    BAIL_IF_MACRO(!readui16(io, &ui16), ERRPASS, 0);

    /*
     * Make sure that the comment length matches to the end of file...
     *  If it doesn't, we're either in the wrong part of the file, or the
     *  file is corrupted, but we give up either way.
     */
    BAIL_IF_MACRO((pos + 22 + ui16) != len, PHYSFS_ERR_CORRUPT, 0);

    return 1;  /* made it. */
} /* zip_parse_end_of_central_dir */


static void *ZIP_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    ZIPinfo *info = NULL;
    PHYSFS_uint64 data_start;
    PHYSFS_uint64 cent_dir_ofs;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
    BAIL_IF_MACRO(!isZip(io), ERRPASS, NULL);

    info = (ZIPinfo *) allocator.Malloc(sizeof (ZIPinfo));
    BAIL_IF_MACRO(!info, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    memset(info, '\0', sizeof (ZIPinfo));
    info->io = io;

    if (!zip_parse_end_of_central_dir(io, info, &data_start, &cent_dir_ofs))
        goto ZIP_openarchive_failed;

    if (!zip_load_entries(io, info, data_start, cent_dir_ofs))
        goto ZIP_openarchive_failed;

    return info;

ZIP_openarchive_failed:
    if (info != NULL)
        allocator.Free(info);

    return NULL;
} /* ZIP_openArchive */


static PHYSFS_sint64 zip_find_start_of_dir(ZIPinfo *info, const char *path,
                                            int stop_on_first_find)
{
    PHYSFS_sint64 lo = 0;
    PHYSFS_sint64 hi = (PHYSFS_sint64) (info->entryCount - 1);
    PHYSFS_sint64 middle;
    PHYSFS_uint32 dlen = (PHYSFS_uint32) strlen(path);
    PHYSFS_sint64 retval = -1;
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
        rc = strncmp(path, name, dlen);
        if (rc == 0)
        {
            char ch = name[dlen];
            if ('/' < ch) /* make sure this isn't just a substr match. */
                rc = -1;
            else if ('/' > ch)
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
} /* zip_find_start_of_dir */


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


static void ZIP_enumerateFiles(void *opaque, const char *dname,
                               PHYSFS_EnumFilesCallback cb,
                               const char *origdir, void *callbackdata)
{
    ZIPinfo *info = ((ZIPinfo *) opaque);
    PHYSFS_sint32 dlen, dlen_inc;
    PHYSFS_sint64 i, max;

    i = zip_find_start_of_dir(info, dname, 0);
    if (i == -1)  /* no such directory. */
        return;

    dlen = (PHYSFS_sint32) strlen(dname);
    if ((dlen > 0) && (dname[dlen - 1] == '/')) /* ignore trailing slash. */
        dlen--;

    dlen_inc = ((dlen > 0) ? 1 : 0) + dlen;
    max = (PHYSFS_sint64) info->entryCount;
    while (i < max)
    {
        char *e = info->entries[i].name;
        if ((dlen) && ((strncmp(e, dname, dlen) != 0) || (e[dlen] != '/')))
            break;  /* past end of this dir; we're done. */
        else
        {
            char *add = e + dlen_inc;
            char *ptr = strchr(add, '/');
            PHYSFS_sint32 ln = (PHYSFS_sint32) ((ptr) ? ptr-add : strlen(add));
            doEnumCallback(cb, callbackdata, origdir, add, ln);
            ln += dlen_inc;  /* point past entry to children... */

            /* increment counter and skip children of subdirs... */
            while ((++i < max) && (ptr != NULL))
            {
                char *e_new = info->entries[i].name;
                if ((strncmp(e, e_new, ln) != 0) || (e_new[ln] != '/'))
                    break;
            } /* while */
        } /* else */
    } /* while */
} /* ZIP_enumerateFiles */


static PHYSFS_Io *zip_get_io(PHYSFS_Io *io, ZIPinfo *inf, ZIPentry *entry)
{
    int success;
    PHYSFS_Io *retval = io->duplicate(io);
    BAIL_IF_MACRO(!retval, ERRPASS, NULL);

    /* !!! FIXME: if you open a dir here, it should bail ERR_NOT_A_FILE */

    /* (inf) can be NULL if we already resolved. */
    success = (inf == NULL) || zip_resolve(retval, inf, entry);
    if (success)
    {
        PHYSFS_sint64 offset;
        offset = ((entry->symlink) ? entry->symlink->offset : entry->offset);
        success = retval->seek(retval, offset);
    } /* if */

    if (!success)
    {
        retval->destroy(retval);
        retval = NULL;
    } /* if */

    return retval;
} /* zip_get_io */


static PHYSFS_Io *ZIP_openRead(void *opaque, const char *filename)
{
    PHYSFS_Io *retval = NULL;
    ZIPinfo *info = (ZIPinfo *) opaque;
    ZIPentry *entry = zip_find_entry(info, filename, NULL);
    ZIPfileinfo *finfo = NULL;

    BAIL_IF_MACRO(!entry, ERRPASS, NULL);

    retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    GOTO_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, ZIP_openRead_failed);

    finfo = (ZIPfileinfo *) allocator.Malloc(sizeof (ZIPfileinfo));
    GOTO_IF_MACRO(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, ZIP_openRead_failed);
    memset(finfo, '\0', sizeof (ZIPfileinfo));

    finfo->io = zip_get_io(info->io, info, entry);
    GOTO_IF_MACRO(!finfo->io, ERRPASS, ZIP_openRead_failed);
    finfo->entry = ((entry->symlink != NULL) ? entry->symlink : entry);
    initializeZStream(&finfo->stream);

    if (finfo->entry->compression_method != COMPMETH_NONE)
    {
        finfo->buffer = (PHYSFS_uint8 *) allocator.Malloc(ZIP_READBUFSIZE);
        if (!finfo->buffer)
            GOTO_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, ZIP_openRead_failed);
        else if (zlib_err(inflateInit2(&finfo->stream, -MAX_WBITS)) != Z_OK)
            goto ZIP_openRead_failed;
    } /* if */

    memcpy(retval, &ZIP_Io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;

    return retval;

ZIP_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);

        if (finfo->buffer != NULL)
        {
            allocator.Free(finfo->buffer);
            inflateEnd(&finfo->stream);
        } /* if */

        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* ZIP_openRead */


static PHYSFS_Io *ZIP_openWrite(void *opaque, const char *filename)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* ZIP_openWrite */


static PHYSFS_Io *ZIP_openAppend(void *opaque, const char *filename)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* ZIP_openAppend */


static void ZIP_closeArchive(void *opaque)
{
    ZIPinfo *zi = (ZIPinfo *) (opaque);
    zi->io->destroy(zi->io);
    zip_free_entries(zi->entries, zi->entryCount);
    allocator.Free(zi);
} /* ZIP_closeArchive */


static int ZIP_remove(void *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* ZIP_remove */


static int ZIP_mkdir(void *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* ZIP_mkdir */


static int ZIP_stat(void *opaque, const char *filename, PHYSFS_Stat *stat)
{
    int isDir = 0;
    const ZIPinfo *info = (const ZIPinfo *) opaque;
    const ZIPentry *entry = zip_find_entry(info, filename, &isDir);

    /* !!! FIXME: does this need to resolve entries here? */

    if ((!isDir) && (entry == NULL))
        return 0;

    else if (isDir)
    {
        stat->filesize = 0;
        stat->filetype = PHYSFS_FILETYPE_DIRECTORY;
    } /* if */

    else if (zip_entry_is_symlink(entry))
    {
        stat->filesize = 0;
        stat->filetype = PHYSFS_FILETYPE_SYMLINK;
    } /* else if */

    else
    {
        stat->filesize = (PHYSFS_sint64) entry->uncompressed_size;
        stat->filetype = PHYSFS_FILETYPE_REGULAR;
    } /* else */

    stat->modtime = ((entry) ? entry->last_mod_time : 0);
    stat->createtime = stat->modtime;
    stat->accesstime = 0;
    stat->readonly = 1; /* .zip files are always read only */

    return 1;
} /* ZIP_stat */


const PHYSFS_Archiver __PHYSFS_Archiver_ZIP =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "ZIP",
        "PkZip/WinZip/Info-Zip compatible",
        "Ryan C. Gordon <icculus@icculus.org>",
        "http://icculus.org/physfs/",
        1,  /* supportsSymlinks */
    },
    ZIP_openArchive,
    ZIP_enumerateFiles,
    ZIP_openRead,
    ZIP_openWrite,
    ZIP_openAppend,
    ZIP_remove,
    ZIP_mkdir,
    ZIP_stat,
    ZIP_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_ZIP */

/* end of archiver_zip.c ... */

