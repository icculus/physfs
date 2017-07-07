/*
 * VDF support routines for PhysicsFS.
 *
 * This driver handles Gothic I/II VDF archives.
 * This format (but not this driver) was designed by Piranha Bytes for
 *  use wih the ZenGin engine.
 *
 * This file was written by Francesco Bertolaccini, based on the UNPK archiver
 *  by Ryan C. Gordon and the works of degenerated1123 and Nico Bendlin.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"
#include <time.h>
#include <ctype.h>

#if PHYSFS_SUPPORTS_VDF

#define VDF_HEADER_COMMENT_LENGTH 256
#define VDF_HEADER_SIGNATURE_LENGTH 16
#define VDF_ENTRY_NAME_LENGTH 64
#define VDF_ENTRY_DIR 0x80000000
#define VDF_ENTRY_LAST 0x40000000

#define VDF_CRC16 0x8005
#define VDF_HASHTABLE_SIZE 65536

const char* VDF_SIGNATURE_G1 = "PSVDSC_V2.00\r\n\r\n";
const char* VDF_SIGNATURE_G2 = "PSVDSC_V2.00\n\r\n\r";

typedef union
{
    PHYSFS_uint32 raw;
    struct
    {
        PHYSFS_uint32 seconds : 5;
        PHYSFS_uint32 minutes : 6;
        PHYSFS_uint32 hour    : 5;
        PHYSFS_uint32 day     : 5;
        PHYSFS_uint32 month   : 4;
        PHYSFS_uint32 year    : 7;
    } members;
} VdfTimestamp;

typedef struct
{
    char comment[VDF_HEADER_COMMENT_LENGTH];
    char signature[VDF_HEADER_SIGNATURE_LENGTH];
    PHYSFS_uint32 numEntries;
    PHYSFS_uint32 numFiles;
    VdfTimestamp timestamp;
    PHYSFS_uint32 dataSize;
    PHYSFS_uint32 rootCatOffset;
    PHYSFS_uint32 version;
} VdfHeader;

typedef struct
{
    char name[VDF_ENTRY_NAME_LENGTH];
    PHYSFS_uint32 jump;
    PHYSFS_uint32 size;
    PHYSFS_uint32 type;
    PHYSFS_uint32 attributes;
} VdfEntryInfo;

typedef struct _VdfLinkedList
{
    VdfEntryInfo *entry;
    struct _VdfLinkedList *next, *prev;
} VdfLinkedList;

typedef struct
{
    VdfEntryInfo *entries;
    int numEntries;
    VdfLinkedList *table[VDF_HASHTABLE_SIZE];
    PHYSFS_sint64 timestamp;
    PHYSFS_Io *io;
} VdfRecord;

typedef struct
{
    PHYSFS_Io *io;
    VdfEntryInfo *entry;
    PHYSFS_uint32 curPos;
} VdfFileinfo;

static const PHYSFS_uint16 vdfCrcTable[256] =
{
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF,
    0x8C48, 0x9DC1, 0xAF5A, 0xBED3, 0xCA6C, 0xDBE5, 0xE97E, 0xF8F7,
    0x0919, 0x1890, 0x2A0B, 0x3B82, 0x4F3D, 0x5EB4, 0x6C2F, 0x7DA6,
    0x8551, 0x94D8, 0xA643, 0xB7CA, 0xC375, 0xD2FC, 0xE067, 0xF1EE,
    0x1232, 0x03BB, 0x3120, 0x20A9, 0x5416, 0x459F, 0x7704, 0x668D,
    0x9E7A, 0x8FF3, 0xBD68, 0xACE1, 0xD85E, 0xC9D7, 0xFB4C, 0xEAC5,
    0x1B2B, 0x0AA2, 0x3839, 0x29B0, 0x5D0F, 0x4C86, 0x7E1D, 0x6F94,
    0x9763, 0x86EA, 0xB471, 0xA5F8, 0xD147, 0xC0CE, 0xF255, 0xE3DC,
    0x2464, 0x35ED, 0x0776, 0x16FF, 0x6240, 0x73C9, 0x4152, 0x50DB,
    0xA82C, 0xB9A5, 0x8B3E, 0x9AB7, 0xEE08, 0xFF81, 0xCD1A, 0xDC93,
    0x2D7D, 0x3CF4, 0x0E6F, 0x1FE6, 0x6B59, 0x7AD0, 0x484B, 0x59C2,
    0xA135, 0xB0BC, 0x8227, 0x93AE, 0xE711, 0xF698, 0xC403, 0xD58A,
    0x3656, 0x27DF, 0x1544, 0x04CD, 0x7072, 0x61FB, 0x5360, 0x42E9,
    0xBA1E, 0xAB97, 0x990C, 0x8885, 0xFC3A, 0xEDB3, 0xDF28, 0xCEA1,
    0x3F4F, 0x2EC6, 0x1C5D, 0x0DD4, 0x796B, 0x68E2, 0x5A79, 0x4BF0,
    0xB307, 0xA28E, 0x9015, 0x819C, 0xF523, 0xE4AA, 0xD631, 0xC7B8,
    0x48C8, 0x5941, 0x6BDA, 0x7A53, 0x0EEC, 0x1F65, 0x2DFE, 0x3C77,
    0xC480, 0xD509, 0xE792, 0xF61B, 0x82A4, 0x932D, 0xA1B6, 0xB03F,
    0x41D1, 0x5058, 0x62C3, 0x734A, 0x07F5, 0x167C, 0x24E7, 0x356E,
    0xCD99, 0xDC10, 0xEE8B, 0xFF02, 0x8BBD, 0x9A34, 0xA8AF, 0xB926,
    0x5AFA, 0x4B73, 0x79E8, 0x6861, 0x1CDE, 0x0D57, 0x3FCC, 0x2E45,
    0xD6B2, 0xC73B, 0xF5A0, 0xE429, 0x9096, 0x811F, 0xB384, 0xA20D,
    0x53E3, 0x426A, 0x70F1, 0x6178, 0x15C7, 0x044E, 0x36D5, 0x275C,
    0xDFAB, 0xCE22, 0xFCB9, 0xED30, 0x998F, 0x8806, 0xBA9D, 0xAB14,
    0x6CAC, 0x7D25, 0x4FBE, 0x5E37, 0x2A88, 0x3B01, 0x099A, 0x1813,
    0xE0E4, 0xF16D, 0xC3F6, 0xD27F, 0xA6C0, 0xB749, 0x85D2, 0x945B,
    0x65B5, 0x743C, 0x46A7, 0x572E, 0x2391, 0x3218, 0x0083, 0x110A,
    0xE9FD, 0xF874, 0xCAEF, 0xDB66, 0xAFD9, 0xBE50, 0x8CCB, 0x9D42,
    0x7E9E, 0x6F17, 0x5D8C, 0x4C05, 0x38BA, 0x2933, 0x1BA8, 0x0A21,
    0xF2D6, 0xE35F, 0xD1C4, 0xC04D, 0xB4F2, 0xA57B, 0x97E0, 0x8669,
    0x7787, 0x660E, 0x5495, 0x451C, 0x31A3, 0x202A, 0x12B1, 0x0338,
    0xFBCF, 0xEA46, 0xD8DD, 0xC954, 0xBDEB, 0xAC62, 0x9EF9, 0x8F70
};

/*
 * Performs a case-insensitive CRC16 sum.
 */
static PHYSFS_uint16 vdfGenCrc16(const char *c_ptr)
{
    PHYSFS_uint16 crc = 0xFFFF;

    while (*c_ptr)
        crc = (crc << 8) ^ vdfCrcTable[((crc >> 8) ^ toupper(*c_ptr++))];

    return crc;
} /* vdfGenCrc16 */

static PHYSFS_sint64 VDF_read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 len)
{
    VdfFileinfo *finfo = (VdfFileinfo *)io->opaque;
    const VdfEntryInfo *entry = finfo->entry;
    const PHYSFS_uint64 bytesLeft = (PHYSFS_uint64)(entry->size - finfo->curPos);
    PHYSFS_sint64 rc;

    if (bytesLeft < len)
        len = bytesLeft;

    rc = finfo->io->read(finfo->io, buffer, len);
    if (rc > 0)
        finfo->curPos += (PHYSFS_uint32)rc;

    return rc;
} /* VDF_read */


static PHYSFS_sint64 VDF_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL(PHYSFS_ERR_READ_ONLY, -1);
} /* VDF_write */


static PHYSFS_sint64 VDF_tell(PHYSFS_Io *io)
{
    return ((VdfFileinfo *)io->opaque)->curPos;
} /* VDF_tell */


static int VDF_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    VdfFileinfo *finfo = (VdfFileinfo *)io->opaque;
    const VdfEntryInfo *entry = finfo->entry;
    int rc;

    BAIL_IF(offset >= entry->size, PHYSFS_ERR_PAST_EOF, 0);
    rc = finfo->io->seek(finfo->io, entry->jump + offset);
    if (rc)
        finfo->curPos = (PHYSFS_uint32)offset;

    return rc;
} /* VDF_seek */


static PHYSFS_sint64 VDF_length(PHYSFS_Io *io)
{
    const VdfFileinfo *finfo = (VdfFileinfo *)io->opaque;
    return ((PHYSFS_sint64)finfo->entry->size);
} /* VDF_length */


static PHYSFS_Io *VDF_duplicate(PHYSFS_Io *_io)
{
    VdfFileinfo *origfinfo = (VdfFileinfo *)_io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *)allocator.Malloc(sizeof(PHYSFS_Io));
    VdfFileinfo *finfo = (VdfFileinfo *)allocator.Malloc(sizeof(VdfFileinfo));
    GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, VDF_duplicate_failed);
    GOTO_IF(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, VDF_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    if (!io) goto VDF_duplicate_failed;
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;
    memcpy(retval, _io, sizeof(PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

VDF_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* VDF_duplicate */

static int VDF_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }

static void VDF_destroy(PHYSFS_Io *io)
{
    VdfFileinfo *finfo = (VdfFileinfo *)io->opaque;
    finfo->io->destroy(finfo->io);
    allocator.Free(finfo);
    allocator.Free(io);
} /* VDF_destroy */

static const PHYSFS_Io VDF_Io =
{
    CURRENT_PHYSFS_IO_API_VERSION, NULL,
    VDF_read,
    VDF_write,
    VDF_seek,
    VDF_tell,
    VDF_length,
    VDF_duplicate,
    VDF_flush,
    VDF_destroy
};

/*
 * VDF stores timestamps as 32bit DOS dates:
 *  the seconds are counted in 2-seconds intervals
 *  and the years are counted since 1 Jan. 1980
 */
static PHYSFS_sint64 vdfDosTimeToEpoch(VdfTimestamp time)
{
    struct tm t = { 0 };
    t.tm_year = time.members.year + 80;
    t.tm_mon = time.members.month - 1; /* DOS time counts months 1-12, we need 0-11 */
    t.tm_mday = time.members.day;
    t.tm_hour = time.members.hour;
    t.tm_min = time.members.minutes;
    t.tm_sec = time.members.seconds * 2;

    return (PHYSFS_sint64)difftime(mktime(&t), 0);
} /* vdfDosTimeToEpoch */

/*
 * Sets the last bytes to zero until we find a non-empty character
 * FIXME: May corrupt filenames which are actually 64 bytes long!
 */
static void vdfTruncateFilename(char *s, PHYSFS_uint32 *nameLength)
{
    int i;

    s[VDF_ENTRY_NAME_LENGTH - 1] = '\0';
    for (i = VDF_ENTRY_NAME_LENGTH - 2; i > 0; i--) {
        if (isspace(s[i]))
        {
            s[i] = '\0';
        } /* if */
        else
        {
            break;
        } /* else */
    } /* for */

    i = 0;
    while (s[i])
    {
        i++;
    } /* while */
    *nameLength = i;
} /* vdfTruncateFilename */

/*
 * Compares two strings in a case-insensitive way
 * Returns 0 if they are equal, 1 otherwise
 */
static int vdfStrcmp(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (toupper(*a) != toupper(*b)) return 1;
        a++;
        b++;
    } /* while */
    if (*a == 0 && *b == 0) return 0;
    return 1;
} /* vdfStrcmp */

/* Adds an entry to the hashtable */
static void vdfAddEntry(VdfRecord *record, VdfEntryInfo *entry)
{
    PHYSFS_uint16 hash = vdfGenCrc16(entry->name);
    VdfLinkedList *list = record->table[hash];
    if (list == NULL)
    {
        list = (VdfLinkedList*)allocator.Malloc(sizeof(VdfLinkedList));
        list->next = NULL;
        list->prev = NULL;
        list->entry = entry;
        record->table[hash] = list;
    }
    else
    {
        VdfLinkedList *newElement = (VdfLinkedList*)allocator.Malloc(sizeof(VdfLinkedList));
        newElement->next = list;
        newElement->prev = NULL;
        list->prev = newElement;
        newElement->entry = entry;
        record->table[hash] = newElement;
    }
} /* vdfAddEntry */

static VdfRecord *vdfLoadRecord(PHYSFS_Io *io, VdfHeader header)
{
    VdfRecord *record = (VdfRecord*)allocator.Malloc(sizeof(VdfRecord));
    VdfEntryInfo *entries;
    VdfEntryInfo *entry;

    BAIL_IF(!record, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    entries = (VdfEntryInfo *)allocator.Malloc(sizeof(VdfEntryInfo) * header.numEntries);
    GOTO_IF(!entries, PHYSFS_ERR_OUT_OF_MEMORY, failed);

    if (!__PHYSFS_readAll(io, entries, sizeof(VdfEntryInfo) * header.numEntries)) goto failed;

    record->timestamp = vdfDosTimeToEpoch(header.timestamp);
    record->numEntries = header.numEntries;
    record->entries = entries;
    memset(record->table, 0, sizeof(record->table));

    for (entry = entries; header.numEntries > 0; header.numEntries--, entry++)
    {
        PHYSFS_uint32 len;
        entry->jump = PHYSFS_swapULE32(entry->jump);
        entry->size = PHYSFS_swapULE32(entry->size);
        entry->type = PHYSFS_swapULE32(entry->type);
        entry->attributes = PHYSFS_swapULE32(entry->attributes);
        vdfTruncateFilename(entry->name, &len);
        vdfAddEntry(record, entry);
    } /* for */

    return record;

failed:
    if (!record) allocator.Free(record);
    if (!entries) allocator.Free(entries);
    return NULL;
} /* vdfLoadList */

void VDF_enumerateFiles(void *opaque, const char *dname,
    PHYSFS_EnumFilesCallback cb,
    const char *origdir, void *callbackdata)
{
    const VdfRecord *record = (const VdfRecord *)opaque;
    if (strcmp(dname, "") == 0)
    {
        int i;
        for (i = 0; i < VDF_HASHTABLE_SIZE; i++)
        {
            VdfLinkedList *list = record->table[i];
            while (list)
            {
                cb(callbackdata, origdir, list->entry->name);
                list = list->next;
            } /* while */
        } /* for */
    } /* if */
} /* VDF_enumerateFiles */

/* Retrieves a file from the hashtable */
static int vdfFindFile(const VdfRecord *record, const char *filename, VdfEntryInfo *outEntry)
{
    VdfLinkedList *list = record->table[vdfGenCrc16(filename)];
    while (list)
    {
        if (vdfStrcmp(list->entry->name, filename) == 0 && !(list->entry->type & VDF_ENTRY_DIR))
        {
            *outEntry = *(list->entry);
            return 1;
        } /* if */
        list = list->next;
    } /* while */
    return 0;
} /* vdfFindFile */

int VDF_stat(void *opaque, const char *filename, PHYSFS_Stat *stat)
{
    const VdfRecord *record = (const VdfRecord *)opaque;
    VdfEntryInfo entry;
    if (vdfFindFile(record, filename, &entry))
    {
        stat->readonly = 1;
        stat->accesstime = -1;
        stat->createtime = record->timestamp;
        stat->modtime = record->timestamp;
        stat->filesize = entry.size;
        stat->filetype = PHYSFS_FILETYPE_REGULAR;

        return 1;
    } /* if */

    return 0;
} /* VDF_stat */

void *VDF_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    VdfHeader header;
    VdfRecord *record = NULL;
    assert(io != NULL); /* shouldn't ever happen. */

    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
    BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &header, sizeof(VdfHeader)), NULL);

    header.numEntries = PHYSFS_swapULE32(header.numEntries);
    header.numFiles = PHYSFS_swapULE32(header.numFiles);
    header.timestamp.raw = PHYSFS_swapULE32(header.timestamp.raw);
    header.dataSize = PHYSFS_swapULE32(header.dataSize);
    header.rootCatOffset = PHYSFS_swapULE32(header.rootCatOffset);
    header.version = PHYSFS_swapULE32(header.version);

    BAIL_IF(header.version != 0x50, PHYSFS_ERR_UNSUPPORTED, NULL);

    if ((memcmp(header.signature, VDF_SIGNATURE_G1, VDF_HEADER_SIGNATURE_LENGTH) != 0) &&
        (memcmp(header.signature, VDF_SIGNATURE_G2, VDF_HEADER_SIGNATURE_LENGTH) != 0))
    {
        BAIL(PHYSFS_ERR_UNSUPPORTED, NULL);
    } /* if */

    record = vdfLoadRecord(io, header);
    BAIL_IF_ERRPASS(!record, NULL);
    record->io = io;
    return record;
} /* VDF_openArchive */

void VDF_closeArchive(void *opaque)
{
    VdfRecord *record = ((VdfRecord *)opaque);
    int i;
    for (i = 0; i < VDF_HASHTABLE_SIZE; i++)
    {
        VdfLinkedList *list = record->table[i];
        while (list)
        {
            VdfLinkedList *next = list->next;
            allocator.Free(list);
            list = next;
        }
    }
    allocator.Free(record->entries);
    allocator.Free(record);
} /* VDF_closeArchive */

PHYSFS_Io *VDF_openRead(void *opaque, const char *name)
{
    PHYSFS_Io *retval = NULL;
    VdfRecord *record = (VdfRecord *)opaque;
    VdfFileinfo *finfo = NULL;
    VdfEntryInfo entry;
    GOTO_IF(!vdfFindFile(record, name, &entry), PHYSFS_ERR_NOT_FOUND, VDF_openRead_failed);

    retval = (PHYSFS_Io *)allocator.Malloc(sizeof(PHYSFS_Io));
    GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, VDF_openRead_failed);

    finfo = (VdfFileinfo *)allocator.Malloc(sizeof(VdfFileinfo));
    GOTO_IF(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, VDF_openRead_failed);

    finfo->io = record->io->duplicate(record->io);
    GOTO_IF_ERRPASS(!finfo->io, VDF_openRead_failed);

    if (!finfo->io->seek(finfo->io, entry.jump))
        goto VDF_openRead_failed;

    finfo->curPos = 0;

    finfo->entry = (VdfEntryInfo *)allocator.Malloc(sizeof(VdfEntryInfo));
    GOTO_IF(!finfo->entry, PHYSFS_ERR_OUT_OF_MEMORY, VDF_openRead_failed);
    memcpy(finfo->entry, &entry, sizeof(VdfEntryInfo));

    memcpy(retval, &VDF_Io, sizeof(*retval));
    retval->opaque = finfo;
    return retval;

VDF_openRead_failed:
    if (finfo != NULL)
    {
        if (finfo->io != NULL)
            finfo->io->destroy(finfo->io);
        allocator.Free(finfo);
    } /* if */

    if (retval != NULL)
        allocator.Free(retval);

    return NULL;
} /* VDF_openRead */

const PHYSFS_Archiver __PHYSFS_Archiver_VDF =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "VDF",
        "Gothic I/II engine format",
        "Francesco Bertolaccini <bertolaccinifrancesco@gmail.com>",
        "https://github.com/frabert",
        0,  /* supportsSymlinks */
    },
    VDF_openArchive,
    VDF_enumerateFiles,
    VDF_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
    VDF_stat,
    VDF_closeArchive
};

#endif /* defined PHYSFS_SUPPORTS_VDF */

/* end of archiver_vdf.c ... */
