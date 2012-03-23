/*
 * ISO9660 support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Christoph Nelles.
 */

/* !!! FIXME: this file needs Ryanification. */

/*
 * Handles CD-ROM disk images (and raw CD-ROM devices).
 *
 * Not supported:
 * - RockRidge
 * - Non 2048 Sectors
 * - UDF
 *
 * Deviations from the standard
 * - Ignores mandatory sort order
 * - Allows various invalid file names
 *
 * Problems
 * - Ambiguities in the standard
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_ISO9660

#include <time.h>

/* cache files smaller than this completely in memory */
#define ISO9660_FULLCACHEMAXSIZE 2048

/* !!! FIXME: this is going to cause trouble. */
#pragma pack(push)  /* push current alignment to stack */
#pragma pack(1)     /* set alignment to 1 byte boundary */

/* This is the format as defined by the standard
typedef struct
{
    PHYSFS_uint32 lsb;
    PHYSFS_uint32 msb;
} ISOBB32bit; // 32byte Both Byte type, means the value first in LSB then in MSB

typedef struct
{
    PHYSFS_uint16 lsb;
    PHYSFS_uint16 msb;
} ISOBB16bit; // 16byte Both Byte type, means the value first in LSB then in MSB
*/

/* define better ones to simplify coding (less if's) */
#if PHYSFS_BYTEORDER == PHYSFS_LIL_ENDIAN
#define ISOBB32bit(name) PHYSFS_uint32 name; PHYSFS_uint32 __dummy_##name;
#define ISOBB16bit(name) PHYSFS_uint16 name; PHYSFS_uint16 __dummy_##name;
#else
#define ISOBB32bit(name) PHYSFS_uint32 __dummy_##name; PHYSFS_uint32 name;
#define ISOBB16bit(name) PHYSFS_uint16 __dummy_##name; PHYSFS_uint16 name;
#endif

typedef struct
{
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char minute[2];
    char second[2];
    char centisec[2];
    PHYSFS_sint8 offset; /* in 15min from GMT */
} ISO9660VolumeTimestamp;

typedef struct
{
    PHYSFS_uint8 year;
    PHYSFS_uint8 month;
    PHYSFS_uint8 day;
    PHYSFS_uint8 hour;
    PHYSFS_uint8 minute;
    PHYSFS_uint8 second;
    PHYSFS_sint8 offset;
} ISO9660FileTimestamp;

typedef struct
{
  unsigned existence:1;
  unsigned directory:1;
  unsigned associated_file:1;
  unsigned record:1;
  unsigned protection:1;
  unsigned reserved:2;
  unsigned multiextent:1;
} ISO9660FileFlags;

typedef struct
{
    PHYSFS_uint8 length;
    PHYSFS_uint8 attribute_length;
    ISOBB32bit(extent_location)
    ISOBB32bit(data_length)
    ISO9660FileTimestamp timestamp;
    ISO9660FileFlags file_flags;
    PHYSFS_uint8 file_unit_size;
    PHYSFS_uint8 gap_size;
    ISOBB16bit(vol_seq_no)
    PHYSFS_uint8 len_fi;
    char unused;
} ISO9660RootDirectoryRecord;

/* this structure is combined for all Volume descriptor types */
typedef struct
{
    PHYSFS_uint8 type;
    char identifier[5];
    PHYSFS_uint8 version;
    PHYSFS_uint8 flags;
    char system_identifier[32];
    char volume_identifier[32];
    char unused2[8];
    ISOBB32bit(space_size)
    PHYSFS_uint8 escape_sequences[32];
    ISOBB16bit(vol_set_size)
    ISOBB16bit(vol_seq_no)
    ISOBB16bit(block_size)
    ISOBB32bit(path_table_size)
/*    PHYSFS_uint32 path_table_start_lsb; // why didn't they use both byte type?
    PHYSFS_uint32 opt_path_table_start_lsb;
    PHYSFS_uint32 path_table_start_msb;
    PHYSFS_uint32 opt_path_table_start_msb;*/
#if PHYSFS_BYTEORDER == PHYSFS_LIL_ENDIAN
    PHYSFS_uint32 path_table_start;
    PHYSFS_uint32 opt_path_table_start;
    PHYSFS_uint32 unused6;
    PHYSFS_uint32 unused7;
#else
    PHYSFS_uint32 unused6;
    PHYSFS_uint32 unused7;
    PHYSFS_uint32 path_table_start;
    PHYSFS_uint32 opt_path_table_start;
#endif
    ISO9660RootDirectoryRecord rootdirectory;
    char set_identifier[128];
    char publisher_identifier[128];
    char preparer_identifer[128];
    char application_identifier[128];
    char copyright_file_identifier[37];
    char abstract_file_identifier[37];
    char bibliographic_file_identifier[37];
    ISO9660VolumeTimestamp creation_timestamp;
    ISO9660VolumeTimestamp modification_timestamp;
    ISO9660VolumeTimestamp expiration_timestamp;
    ISO9660VolumeTimestamp effective_timestamp;
    PHYSFS_uint8 file_structure_version;
    char unused4;
    char application_use[512];
    char unused5[653];
} ISO9660VolumeDescriptor;

typedef struct
{
    PHYSFS_uint8 recordlen;
    PHYSFS_uint8 extattributelen;
    ISOBB32bit(extentpos)
    ISOBB32bit(datalen)
    ISO9660FileTimestamp recordtime;
    ISO9660FileFlags flags;
    PHYSFS_uint8 file_unit_size;
    PHYSFS_uint8 interleave_gap;
    ISOBB16bit(volseqno)
    PHYSFS_uint8 filenamelen;
    char filename[222]; /* This is not exact, but makes reading easier */
} ISO9660FileDescriptor;

typedef struct
{
    ISOBB16bit(owner)
    ISOBB16bit(group)
    PHYSFS_uint16 flags; /* not implemented*/
    ISO9660VolumeTimestamp create_time; /* yes, not file timestamp */
    ISO9660VolumeTimestamp mod_time;
    ISO9660VolumeTimestamp expire_time;
    ISO9660VolumeTimestamp effective_time;
    PHYSFS_uint8 record_format;
    PHYSFS_uint8 record_attributes;
    ISOBB16bit(record_len)
    char system_identifier[32];
    char system_use[64];
    PHYSFS_uint8 version;
    ISOBB16bit(escape_len)
    char reserved[64];
    /** further fields not implemented */
} ISO9660ExtAttributeRec;

#pragma pack(pop)   /* restore original alignment from stack */

typedef struct
{
    PHYSFS_Io *io;
    PHYSFS_uint32 rootdirstart;
    PHYSFS_uint32 rootdirsize;
    PHYSFS_uint64 currpos;
    int isjoliet;
    char *path;
    void *mutex;
} ISO9660Handle;


typedef struct __ISO9660FileHandle
{
    PHYSFS_sint64 filesize;
    PHYSFS_uint64 currpos;
    PHYSFS_uint64 startblock;
    ISO9660Handle *isohandle;
    PHYSFS_uint32 (*read) (struct __ISO9660FileHandle *filehandle, void *buffer,
            PHYSFS_uint64 len);
    int (*seek)(struct __ISO9660FileHandle *filehandle,  PHYSFS_sint64 offset);
    void (*close)(struct __ISO9660FileHandle *filehandle);
    /* !!! FIXME: anonymouse union is going to cause problems. */
    union
    {
        /* !!! FIXME: just use a memory PHYSFS_Io here, unify all this code. */
        char *cacheddata; /* data of file when cached */
        PHYSFS_Io *io; /* handle to separate opened file */
    };
} ISO9660FileHandle;

/*******************************************************************************
 * Time conversion functions
 ******************************************************************************/

static PHYSFS_sint64 iso_mktime(ISO9660FileTimestamp *timestamp)
{
    struct tm tm;
    tm.tm_year = timestamp->year;
    tm.tm_mon = timestamp->month - 1;
    tm.tm_mday = timestamp->day;
    tm.tm_hour = timestamp->hour;
    tm.tm_min = timestamp->minute;
    tm.tm_sec = timestamp->second;
    /* Ignore GMT offset for now... */
    return mktime(&tm);
} /* iso_mktime */

static int iso_atoi2(char *text)
{
    return ((text[0] - 40) * 10) + (text[1] - 40);
} /* iso_atoi2 */

static int iso_atoi4(char *text)
{
    return ((text[0] - 40) * 1000) + ((text[1] - 40) * 100) +
           ((text[2] - 40) * 10) + (text[3] - 40);
} /* iso_atoi4 */

static PHYSFS_sint64 iso_volume_mktime(ISO9660VolumeTimestamp *timestamp)
{
    struct tm tm;
    tm.tm_year = iso_atoi4(timestamp->year);
    tm.tm_mon = iso_atoi2(timestamp->month) - 1;
    tm.tm_mday = iso_atoi2(timestamp->day);
    tm.tm_hour = iso_atoi2(timestamp->hour);
    tm.tm_min = iso_atoi2(timestamp->minute);
    tm.tm_sec = iso_atoi2(timestamp->second);
    /* this allows values outside the range of a unix timestamp... sanitize them */
    PHYSFS_sint64 value = mktime(&tm);
    return value == -1 ? 0 : value;
} /* iso_volume_mktime */

/*******************************************************************************
 * Filename extraction
 ******************************************************************************/

static int iso_extractfilenameISO(ISO9660FileDescriptor *descriptor,
        char *filename, int *version)
{
    *filename = '\0';
    if (descriptor->flags.directory)
    {
        strncpy(filename, descriptor->filename, descriptor->filenamelen);
        filename[descriptor->filenamelen] = '\0';
        *version = 0;
    } /* if */
    else
    {
        /* find last SEPARATOR2 */
        int pos = 0;
        int lastfound = -1;
        for(;pos < descriptor->filenamelen; pos++)
            if (descriptor->filename[pos] == ';')
                lastfound = pos;
        BAIL_IF_MACRO(lastfound < 1, PHYSFS_ERR_NO_SUCH_PATH /* !!! FIXME: PHYSFS_ERR_BAD_FILENAME */, -1);
        BAIL_IF_MACRO(lastfound == (descriptor->filenamelen -1), PHYSFS_ERR_NO_SUCH_PATH /* !!! PHYSFS_ERR_BAD_FILENAME */, -1);
        strncpy(filename, descriptor->filename, lastfound);
        if (filename[lastfound - 1] == '.')
            filename[lastfound - 1] = '\0'; /* consume trailing ., as done in all implementations */
        else
            filename[lastfound] = '\0';
        *version = atoi(descriptor->filename + lastfound);
    } /* else */

    return 0;
} /* iso_extractfilenameISO */


static int iso_extractfilenameUCS2(ISO9660FileDescriptor *descriptor,
        char *filename, int *version)
{
    PHYSFS_uint16 tmp[128];
    PHYSFS_uint16 *src;
    int len;

    *filename = '\0';
    *version = 1; /* Joliet does not have versions.. at least not on my images */

    src = (PHYSFS_uint16*) descriptor->filename;
    len = descriptor->filenamelen / 2;
    tmp[len] = 0;

    while(len--)
        tmp[len] = PHYSFS_swapUBE16(src[len]);

    PHYSFS_utf8FromUcs2(tmp, filename, 255);

    return 0;
} /* iso_extractfilenameUCS2 */


static int iso_extractfilename(ISO9660Handle *handle,
        ISO9660FileDescriptor *descriptor, char *filename,int *version)
{
    if (handle->isjoliet)
        return iso_extractfilenameUCS2(descriptor, filename, version);
    else
        return iso_extractfilenameISO(descriptor, filename, version);
} /* iso_extractfilename */

/*******************************************************************************
 * Basic image read functions
 ******************************************************************************/

static int iso_readimage(ISO9660Handle *handle, PHYSFS_uint64 where,
                         void *buffer, PHYSFS_uint64 len)
{
    BAIL_IF_MACRO(!__PHYSFS_platformGrabMutex(handle->mutex), ERRPASS, -1);
    int rc = -1;
    if (where != handle->currpos)
        GOTO_IF_MACRO(!handle->io->seek(handle->io,where), ERRPASS, unlockme);
    rc = handle->io->read(handle->io, buffer, len);
    if (rc == -1)
    {
        handle->currpos = (PHYSFS_uint64) -1;
        goto unlockme;
    } /* if */
    handle->currpos += rc;

    unlockme:
    __PHYSFS_platformReleaseMutex(handle->mutex);
    return rc;
} /* iso_readimage */


static PHYSFS_sint64 iso_readfiledescriptor(ISO9660Handle *handle,
                                            PHYSFS_uint64 where,
                                            ISO9660FileDescriptor *descriptor)
{
    PHYSFS_sint64 rc = iso_readimage(handle, where, descriptor,
                                     sizeof (descriptor->recordlen));
    BAIL_IF_MACRO(rc == -1, ERRPASS, -1);
    BAIL_IF_MACRO(rc != 1, PHYSFS_ERR_CORRUPT, -1);

    if (descriptor->recordlen == 0)
        return 0; /* fill bytes at the end of a sector */

    rc = iso_readimage(handle, where + 1, &descriptor->extattributelen,
            descriptor->recordlen - sizeof(descriptor->recordlen));
    BAIL_IF_MACRO(rc == -1, ERRPASS, -1);
    BAIL_IF_MACRO(rc != 1, PHYSFS_ERR_CORRUPT, -1);

    return 0;
} /* iso_readfiledescriptor */

static void iso_extractsubpath(char *path, char **subpath)
{
    *subpath = strchr(path,'/');
    if (*subpath != 0)
    {
        **subpath = 0;
        *subpath +=1;
    } /* if */
} /* iso_extractsubpath */

/*
 * Don't use path tables, they are not necessarily faster, but more complicated
 * to implement as they store only directories and not files, so searching for
 * a file needs to branch to the directory extent sooner or later.
 */
static int iso_find_dir_entry(ISO9660Handle *handle,const char *path,
                              ISO9660FileDescriptor *descriptor, int *exists)
{
    char *subpath = 0;
    PHYSFS_uint64 readpos, end_of_dir;
    char filename[255];
    char pathcopy[256];
    char *mypath;
    int version = 0;

    strcpy(pathcopy, path);
    mypath = pathcopy;
    *exists = 0;

    readpos = handle->rootdirstart;
    end_of_dir = handle->rootdirstart + handle->rootdirsize;
    iso_extractsubpath(mypath, &subpath);
    while (1)
    {
        BAIL_IF_MACRO(iso_readfiledescriptor(handle, readpos, descriptor), ERRPASS, -1);

        /* recordlen = 0 -> no more entries or fill entry */
        if (!descriptor->recordlen)
        {
            /* if we are in the last sector of the directory & it's 0 -> end */
            if ((end_of_dir - 2048) <= (readpos -1))
                break; /* finished */

            /* else skip to the next sector & continue; */
            readpos = (((readpos - 1) / 2048) + 1) * 2048;
            continue;
        } /* if */

        readpos += descriptor->recordlen;
        if (descriptor->filenamelen == 1 && (descriptor->filename[0] == 0
                || descriptor->filename[0] == 1))
            continue; /* special ones, ignore */

        BAIL_IF_MACRO(
            iso_extractfilename(handle, descriptor, filename, &version),
            ERRPASS, -1);

        if (strcmp(filename, mypath) == 0)
        {
            if ( (subpath == 0) || (subpath[0] == 0) )
            {
                *exists = 1;
                return 0;  /* no subpaths left and we found the entry */
            } /* if */

            if (descriptor->flags.directory)
            {
                /* shorten the path to the subpath */
                mypath = subpath;
                iso_extractsubpath(mypath, &subpath);
                /* gosub to the new directory extent */
                readpos = descriptor->extentpos * 2048;
                end_of_dir = readpos + descriptor->datalen;
            } /* if */
            else
            {
                /* we're at a file but have a remaining subpath -> no match */
                return 0;
            } /* else */
        } /* if */
    } /* while */

    return 0;
} /* iso_find_dir_entry */


static int iso_read_ext_attributes(ISO9660Handle *handle, int block,
                                   ISO9660ExtAttributeRec *attributes)
{
    return iso_readimage(handle, block * 2048, attributes,
                         sizeof(ISO9660ExtAttributeRec));
} /* iso_read_ext_attributes */


static int ISO9660_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static PHYSFS_Io *ISO9660_duplicate(PHYSFS_Io *_io)
{
    BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);  /* !!! FIXME: write me. */
} /* ISO9660_duplicate */


static void ISO9660_destroy(PHYSFS_Io *io)
{
    ISO9660FileHandle *fhandle = (ISO9660FileHandle*) io->opaque;
    fhandle->close(fhandle);
    allocator.Free(io);
} /* ISO9660_destroy */


static PHYSFS_sint64 ISO9660_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 len)
{
    ISO9660FileHandle *fhandle = (ISO9660FileHandle*) io->opaque;
    return fhandle->read(fhandle, buf, len);
} /* ISO9660_read */


static PHYSFS_sint64 ISO9660_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 l)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, -1);
} /* ISO9660_write */


static PHYSFS_sint64 ISO9660_tell(PHYSFS_Io *io)
{
    return ((ISO9660FileHandle*) io->opaque)->currpos;
} /* ISO9660_tell */


static int ISO9660_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    ISO9660FileHandle *fhandle = (ISO9660FileHandle*) io->opaque;
    return fhandle->seek(fhandle, offset);
} /* ISO9660_seek */


static PHYSFS_sint64 ISO9660_length(PHYSFS_Io *io)
{
    return ((ISO9660FileHandle*) io->opaque)->filesize;
} /* ISO9660_length */


static const PHYSFS_Io ISO9660_Io =
{
    ISO9660_read,
    ISO9660_write,
    ISO9660_seek,
    ISO9660_tell,
    ISO9660_length,
    ISO9660_duplicate,
    ISO9660_flush,
    ISO9660_destroy,
    NULL
};


/*******************************************************************************
 * Archive management functions
 ******************************************************************************/

static void *ISO9660_openArchive(PHYSFS_Io *io, const char *filename, int forWriting)
{
    char magicnumber[6];
    ISO9660Handle *handle;
    int founddescriptor = 0;
    int foundjoliet = 0;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    /* Skip system area to magic number in Volume descriptor */
    BAIL_IF_MACRO(!io->seek(io, 32769), ERRPASS, NULL);
    BAIL_IF_MACRO(!io->read(io, magicnumber, 5) != 5, ERRPASS, NULL);
    if (memcmp(magicnumber, "CD001", 6) != 0)
        BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);

    handle = allocator.Malloc(sizeof(ISO9660Handle));
    GOTO_IF_MACRO(!handle, PHYSFS_ERR_OUT_OF_MEMORY, errorcleanup);
    handle->path = 0;
    handle->mutex= 0;
    handle->io = NULL;

    handle->path = allocator.Malloc(strlen(filename) + 1);
    GOTO_IF_MACRO(!handle->path, PHYSFS_ERR_OUT_OF_MEMORY, errorcleanup);
    strcpy(handle->path, filename);

    handle->mutex = __PHYSFS_platformCreateMutex();
    GOTO_IF_MACRO(!handle->mutex, ERRPASS, errorcleanup);

    handle->io = io;

    /* seek Primary Volume Descriptor */
    GOTO_IF_MACRO(!io->seek(io, 32768), PHYSFS_ERR_IO, errorcleanup);

    while (1)
    {
        ISO9660VolumeDescriptor descriptor;
        GOTO_IF_MACRO(io->read(io, &descriptor, sizeof(ISO9660VolumeDescriptor)) != sizeof(ISO9660VolumeDescriptor), PHYSFS_ERR_IO, errorcleanup);
        GOTO_IF_MACRO(strncmp(descriptor.identifier, "CD001", 5) != 0, PHYSFS_ERR_UNSUPPORTED, errorcleanup);

        if (descriptor.type == 255)
        {
            /* type 255 terminates the volume descriptor list */
            if (founddescriptor)
                return handle; /* ok, we've found one volume descriptor */
            else
                GOTO_MACRO(PHYSFS_ERR_CORRUPT, errorcleanup);
        } /* if */
        if (descriptor.type == 1 && !founddescriptor)
        {
            handle->currpos = io->tell(io);
            handle->rootdirstart =
                    descriptor.rootdirectory.extent_location * 2048;
            handle->rootdirsize =
                    descriptor.rootdirectory.data_length;
            handle->isjoliet = 0;
            founddescriptor = 1; /* continue search for joliet */
        } /* if */
        if (descriptor.type == 2 && !foundjoliet)
        {
            /* check if is joliet */
            PHYSFS_uint8 *s = descriptor.escape_sequences;
            int joliet = !(descriptor.flags & 1)
                    && (s[0] == 0x25)
                    && (s[1] == 0x2F)
                    && ((s[2] == 0x40) || (s[2] == 0x43) || (s[2] == 0x45));
            if (!joliet)
                continue;

            handle->currpos = io->tell(io);
            handle->rootdirstart =
                    descriptor.rootdirectory.extent_location * 2048;
            handle->rootdirsize =
                    descriptor.rootdirectory.data_length;
            handle->isjoliet = 1;
            founddescriptor = 1;
            foundjoliet = 1;
        } /* if */
    } /* while */

    GOTO_MACRO(PHYSFS_ERR_CORRUPT, errorcleanup);  /* not found. */

errorcleanup:
    if (handle)
    {
        if (handle->path)
            allocator.Free(handle->path);
        if (handle->mutex)
            __PHYSFS_platformDestroyMutex(handle->mutex);
        allocator.Free(handle);
    } /* if */
    return NULL;
} /* ISO9660_openArchive */


static void ISO9660_dirClose(dvoid *opaque)
{
    ISO9660Handle *handle = (ISO9660Handle*) opaque;
    handle->io->destroy(handle->io);
    __PHYSFS_platformDestroyMutex(handle->mutex);
    allocator.Free(handle->path);
    allocator.Free(handle);
} /* ISO9660_dirClose */


/*******************************************************************************
 * Read functions
 ******************************************************************************/


static PHYSFS_uint32 iso_file_read_mem(ISO9660FileHandle *filehandle,
                                       void *buffer, PHYSFS_uint64 len)
{
    /* check remaining bytes & max obj which can be fetched */
    const PHYSFS_sint64 bytesleft = filehandle->filesize - filehandle->currpos;
    if (bytesleft < len)
        len = bytesleft;

    if (len == 0)
        return 0;

    memcpy(buffer, filehandle->cacheddata + filehandle->currpos, (size_t) len);

    filehandle->currpos += len;
    return (PHYSFS_uint32) len;
} /* iso_file_read_mem */


static int iso_file_seek_mem(ISO9660FileHandle *fhandle, PHYSFS_sint64 offset)
{
    BAIL_IF_MACRO(offset < 0, PHYSFS_ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(offset >= fhandle->filesize, PHYSFS_ERR_PAST_EOF, 0);

    fhandle->currpos = offset;
    return 0;
} /* iso_file_seek_mem */


static void iso_file_close_mem(ISO9660FileHandle *fhandle)
{
    allocator.Free(fhandle->cacheddata);
    allocator.Free(fhandle);
} /* iso_file_close_mem */


static PHYSFS_uint32 iso_file_read_foreign(ISO9660FileHandle *filehandle,
                                           void *buffer, PHYSFS_uint64 len)
{
    /* check remaining bytes & max obj which can be fetched */
    const PHYSFS_sint64 bytesleft = filehandle->filesize - filehandle->currpos;
    if (bytesleft < len)
        len = bytesleft;

    const PHYSFS_sint64 rc = filehandle->io->read(filehandle->io, buffer, len);
    BAIL_IF_MACRO(rc == -1, ERRPASS, -1);

    filehandle->currpos += rc; /* i trust my internal book keeping */
    BAIL_IF_MACRO(rc < len, PHYSFS_ERR_CORRUPT, -1);
    return rc;
} /* iso_file_read_foreign */


static int iso_file_seek_foreign(ISO9660FileHandle *fhandle,
                                 PHYSFS_sint64 offset)
{
    BAIL_IF_MACRO(offset < 0, PHYSFS_ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(offset >= fhandle->filesize, PHYSFS_ERR_PAST_EOF, 0);

    PHYSFS_sint64 pos = fhandle->startblock * 2048 + offset;
    BAIL_IF_MACRO(!fhandle->io->seek(fhandle->io, pos), ERRPASS, -1);

    fhandle->currpos = offset;
    return 0;
} /* iso_file_seek_foreign */


static void iso_file_close_foreign(ISO9660FileHandle *fhandle)
{
    fhandle->io->destroy(fhandle->io);
    allocator.Free(fhandle);
} /* iso_file_close_foreign */


static int iso_file_open_mem(ISO9660Handle *handle, ISO9660FileHandle *fhandle)
{
    fhandle->cacheddata = allocator.Malloc(fhandle->filesize);
    BAIL_IF_MACRO(!fhandle->cacheddata, PHYSFS_ERR_OUT_OF_MEMORY, -1);
    int rc = iso_readimage(handle, fhandle->startblock * 2048,
                           fhandle->cacheddata, fhandle->filesize);
    GOTO_IF_MACRO(rc < 0, ERRPASS, freemem);
    GOTO_IF_MACRO(rc == 0, PHYSFS_ERR_CORRUPT, freemem);

    fhandle->read = iso_file_read_mem;
    fhandle->seek = iso_file_seek_mem;
    fhandle->close = iso_file_close_mem;
    return 0;

freemem:
    allocator.Free(fhandle->cacheddata);
    return -1;
} /* iso_file_open_mem */


static int iso_file_open_foreign(ISO9660Handle *handle,
                                 ISO9660FileHandle *fhandle)
{
    int rc;
    fhandle->io = __PHYSFS_createNativeIo(handle->path, 'r');
    BAIL_IF_MACRO(!fhandle->io, ERRPASS, -1);
    rc = fhandle->io->seek(fhandle->io, fhandle->startblock * 2048);
    GOTO_IF_MACRO(!rc, ERRPASS, closefile);

    fhandle->read = iso_file_read_foreign;
    fhandle->seek = iso_file_seek_foreign;
    fhandle->close = iso_file_close_foreign;
    return 0;

closefile:
    fhandle->io->destroy(fhandle->io);
    return -1;
} /* iso_file_open_foreign */


static PHYSFS_Io *ISO9660_openRead(dvoid *opaque, const char *filename, int *exists)
{
    PHYSFS_Io *retval = NULL;
    ISO9660Handle *handle = (ISO9660Handle*) opaque;
    ISO9660FileHandle *fhandle;
    ISO9660FileDescriptor descriptor;
    int rc;

    fhandle = allocator.Malloc(sizeof(ISO9660FileHandle));
    BAIL_IF_MACRO(fhandle == 0, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    fhandle->cacheddata = 0;

    retval = allocator.Malloc(sizeof(PHYSFS_Io));
    GOTO_IF_MACRO(retval == 0, PHYSFS_ERR_OUT_OF_MEMORY, errorhandling);

    /* find file descriptor */
    rc = iso_find_dir_entry(handle, filename, &descriptor, exists);
    GOTO_IF_MACRO(rc, ERRPASS, errorhandling);
    GOTO_IF_MACRO(!*exists, PHYSFS_ERR_NO_SUCH_PATH, errorhandling);

    fhandle->startblock = descriptor.extentpos + descriptor.extattributelen;
    fhandle->filesize = descriptor.datalen;
    fhandle->currpos = 0;
    fhandle->isohandle = handle;
    fhandle->cacheddata = NULL;
    fhandle->io = NULL;

    if (descriptor.datalen <= ISO9660_FULLCACHEMAXSIZE)
        rc = iso_file_open_mem(handle, fhandle);
    else
        rc = iso_file_open_foreign(handle, fhandle);
    GOTO_IF_MACRO(rc, ERRPASS, errorhandling);

    memcpy(retval, &ISO9660_Io, sizeof (PHYSFS_Io));
    retval->opaque = fhandle;
    return retval;

errorhandling:
    if (retval) allocator.Free(retval);
    if (fhandle) allocator.Free(fhandle);
    return NULL;
} /* ISO9660_openRead */



/*******************************************************************************
 * Information gathering functions
 ******************************************************************************/

static void ISO9660_enumerateFiles(dvoid *opaque, const char *dname,
                                   int omitSymLinks,
                                   PHYSFS_EnumFilesCallback cb,
                                   const char *origdir, void *callbackdata)
{
    ISO9660Handle *handle = (ISO9660Handle*) opaque;
    ISO9660FileDescriptor descriptor;
    PHYSFS_uint64 readpos;
    PHYSFS_uint64 end_of_dir;
    char filename[130]; /* ISO allows 31, Joliet 128 -> 128 + 2 eol bytes */
    int version = 0;

    if (*dname == '\0')
    {
        readpos = handle->rootdirstart;
        end_of_dir = readpos + handle->rootdirsize;
    } /* if */
    else
    {
        printf("pfad %s\n",dname);
        int exists = 0;
        BAIL_IF_MACRO(iso_find_dir_entry(handle,dname, &descriptor, &exists), ERRPASS,);
        BAIL_IF_MACRO(!exists, ERRPASS, );
        BAIL_IF_MACRO(!descriptor.flags.directory, ERRPASS,);

        readpos = descriptor.extentpos * 2048;
        end_of_dir = readpos + descriptor.datalen;
    } /* else */

    while (1)
    {
        BAIL_IF_MACRO(iso_readfiledescriptor(handle, readpos, &descriptor), ERRPASS, );

        /* recordlen = 0 -> no more entries or fill entry */
        if (!descriptor.recordlen)
        {
            /* if we are in the last sector of the directory & it's 0 -> end */
            if ((end_of_dir - 2048) <= (readpos -1))
                break;  /* finished */

            /* else skip to the next sector & continue; */
            readpos = (((readpos - 1) / 2048) + 1) * 2048;
            continue;
        } /* if */

        readpos +=  descriptor.recordlen;
        if (descriptor.filenamelen == 1 && (descriptor.filename[0] == 0
                || descriptor.filename[0] == 1))
            continue; /* special ones, ignore */

        strncpy(filename,descriptor.filename,descriptor.filenamelen);
        iso_extractfilename(handle, &descriptor, filename, &version);
        cb(callbackdata, origdir,filename);
    } /* while */
} /* ISO9660_enumerateFiles */


static int ISO9660_stat(dvoid *opaque, const char *name, int *exists,
                        PHYSFS_Stat *stat)
{
    ISO9660Handle *handle = (ISO9660Handle*) opaque;
    ISO9660FileDescriptor descriptor;
    ISO9660ExtAttributeRec extattr;
    BAIL_IF_MACRO(iso_find_dir_entry(handle, name, &descriptor, exists), ERRPASS, -1);
    if (!*exists)
        return 0;

    stat->readonly = 1;

    /* try to get extended info */
    if (descriptor.extattributelen)
    {
        BAIL_IF_MACRO(iso_read_ext_attributes(handle,
                descriptor.extentpos, &extattr), ERRPASS, -1);
        stat->createtime = iso_volume_mktime(&extattr.create_time);
        stat->modtime = iso_volume_mktime(&extattr.mod_time);
        stat->accesstime = iso_volume_mktime(&extattr.mod_time);
    } /* if */
    else
    {
        stat->createtime = iso_mktime(&descriptor.recordtime);
        stat->modtime = iso_mktime(&descriptor.recordtime);
        stat->accesstime = iso_mktime(&descriptor.recordtime);
    } /* else */

    if (descriptor.flags.directory)
    {
        stat->filesize = 0;
        stat->filetype = PHYSFS_FILETYPE_DIRECTORY;
    } /* if */
    else
    {
        stat->filesize = descriptor.datalen;
        stat->filetype = PHYSFS_FILETYPE_REGULAR;
    } /* else */

    return 1;
} /* ISO9660_stat */


/*******************************************************************************
 * Not supported functions
 ******************************************************************************/

static PHYSFS_Io *ISO9660_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* ISO9660_openWrite */


static PHYSFS_Io *ISO9660_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* ISO9660_openAppend */


static int ISO9660_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* ISO9660_remove */


static int ISO9660_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* ISO9660_mkdir */



const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_ISO9660 =
{
    "ISO",
    "ISO9660 image file",
    "Christoph Nelles <evilazrael@evilazrael.de>",
    "http://www.evilazrael.de",
};

const PHYSFS_Archiver __PHYSFS_Archiver_ISO9660 =
{
    &__PHYSFS_ArchiveInfo_ISO9660,
    ISO9660_openArchive,        /* openArchive() method    */
    ISO9660_enumerateFiles,     /* enumerateFiles() method */
    ISO9660_openRead,           /* openRead() method       */
    ISO9660_openWrite,          /* openWrite() method      */
    ISO9660_openAppend,         /* openAppend() method     */
    ISO9660_remove,             /* remove() method         */
    ISO9660_mkdir,              /* mkdir() method          */
    ISO9660_dirClose,           /* dirClose() method       */
    ISO9660_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_ISO9660 */

/* end of archiver_iso9660.c ... */

