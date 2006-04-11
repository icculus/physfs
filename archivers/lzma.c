/*
 * LZMA support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Dennis Schridde, with some peeking at "7zMain.c"
 *   by Igor Pavlov.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined PHYSFS_SUPPORTS_LZMA)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "physfs.h"

#include "7zIn.h"
#include "LzmaStateDecode.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#define LZMA_READBUFSIZE (16 * 1024)

#define LZMA_7Z_FILE_SIG 0x7a37

typedef struct
{
    ISzInStream InStream;
    void *File;
} CFileInStream;

typedef struct
{
    CArchiveDatabaseEx db;
    CFileInStream stream;
    struct _LZMAentry *firstEntry;
    struct _LZMAentry *lastEntry;
} LZMAarchive;

typedef struct _LZMAentry
{
    LZMAarchive *archive;
    struct _LZMAentry *next;
    struct _LZMAentry *previous;
    CFileItem *file;
    CLzmaDecoderState state;
    PHYSFS_uint32 index;
    PHYSFS_uint32 folderIndex;
    PHYSFS_uint32 bufferedBytes;
    PHYSFS_uint32 compressedSize;
    PHYSFS_uint32 position;
    PHYSFS_uint32 compressedPosition;
    PHYSFS_uint8 buffer[LZMA_READBUFSIZE];
    PHYSFS_uint8 *bufferPos;
} LZMAentry;


static void *SzAllocPhysicsFS(size_t size)
{
    return ((size == 0) ? NULL : allocator.Malloc(size));
} /* SzAllocPhysicsFS */


static void SzFreePhysicsFS(void *address)
{
    if (address != NULL)
        allocator.Free(address);
} /* SzFreePhysicsFS */


SZ_RESULT SzFileReadImp(void *object, void *buffer,
                        size_t size, size_t *processedSize)
{
    CFileInStream *s = (CFileInStream *) object;
    size_t processedSizeLoc;

    processedSizeLoc = __PHYSFS_platformRead(s->File, buffer, size, 1) * size;
    if (processedSize != NULL)
        *processedSize = processedSizeLoc;

    return SZ_OK;
} /* SzFileReadImp */


SZ_RESULT SzFileSeekImp(void *object, CFileSize pos)
{
    CFileInStream *s = (CFileInStream *) object;
    if (__PHYSFS_platformSeek(s->File, (PHYSFS_uint64) pos))
        return SZ_OK;
    return SZE_FAIL;
} /* SzFileSeekImp */


LZMAentry *lzma_find_entry(LZMAarchive *archive, const char *name)
{
    /* !!! FIXME: don't malloc here */
    LZMAentry *entry = (LZMAentry *) allocator.Malloc(sizeof (LZMAentry));
    const PHYSFS_uint32 imax = archive->db.Database.NumFiles;
    for (entry->index = 0; entry->index < imax; entry->index++)
    {
        entry->file = archive->db.Database.Files + entry->index;
        if (strcmp(entry->file->Name, name) == 0)
            return entry;
    } /* for */
    allocator.Free(entry);

    BAIL_MACRO(ERR_NO_SUCH_FILE, NULL);
} /* lzma_find_entry */


static PHYSFS_sint32 lzma_find_start_of_dir(LZMAarchive *archive,
                                            const char *path,
                                            int stop_on_first_find)
{
    PHYSFS_sint32 lo = 0;
    PHYSFS_sint32 hi = (PHYSFS_sint32) (archive->db.Database.NumFiles - 1);
    PHYSFS_sint32 middle;
    PHYSFS_uint32 dlen = strlen(path);
    PHYSFS_sint32 retval = -1;
    const char *name;
    int rc;

    if (*path == '\0')  /* root dir? */
        return(0);

    if ((dlen > 0) && (path[dlen - 1] == '/')) /* ignore trailing slash. */
        dlen--;

    while (lo <= hi)
    {
        middle = lo + ((hi - lo) / 2);
        name = archive->db.Database.Files[middle].Name;
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
                    return(middle);

                if (name[dlen + 1] == '\0') /* Skip initial dir entry. */
                    return(middle + 1);

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

    return(retval);
} /* lzma_find_start_of_dir */


/*
 * Wrap all 7z calls in this, so the physfs error state is set appropriately.
 */
static int lzma_err(SZ_RESULT rc)
{
    switch (rc)
    {
        case SZ_OK: /* Same as LZMA_RESULT_OK */
            break;
        case SZE_DATA_ERROR: /* Same as LZMA_RESULT_DATA_ERROR */
            __PHYSFS_setError(ERR_DATA_ERROR);
            break;
        case SZE_OUTOFMEMORY:
            __PHYSFS_setError(ERR_OUT_OF_MEMORY);
            break;
        case SZE_CRC_ERROR:
            __PHYSFS_setError(ERR_CORRUPTED);
            break;
        case SZE_NOTIMPL:
            __PHYSFS_setError(ERR_NOT_IMPLEMENTED);
            break;
        case SZE_FAIL:
            __PHYSFS_setError(ERR_UNKNOWN_ERROR);  /* !!! FIXME: right? */
            break;
        case SZE_ARCHIVE_ERROR:
            __PHYSFS_setError(ERR_CORRUPTED);  /* !!! FIXME: right? */
            break;
        default:
            __PHYSFS_setError(ERR_UNKNOWN_ERROR);
    } /* switch */

    return(rc);
} /* lzma_err */


static PHYSFS_sint64 LZMA_read(fvoid *opaque, void *outBuffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    LZMAentry *entry = (LZMAentry *) opaque;

    size_t wantBytes = objSize * objCount;
    size_t decodedBytes = 0;
    size_t totalDecodedBytes = 0;
    size_t bufferBytes = 0;
    size_t usedBufferedBytes = 0;
    size_t availableBytes = entry->file->Size - entry->position;

    BAIL_IF_MACRO(wantBytes == 0, NULL, 0); /* quick rejection. */

    if (availableBytes < wantBytes)
    {
        wantBytes = availableBytes - (availableBytes % objSize);
        objCount = wantBytes / objSize;
        BAIL_IF_MACRO(objCount == 0, ERR_PAST_EOF, 0); /* quick rejection. */
        __PHYSFS_setError(ERR_PAST_EOF);   /* this is always true here. */
    } /* if */

    while (totalDecodedBytes < wantBytes)
    {
        if (entry->bufferedBytes == 0)
        {
            bufferBytes = entry->compressedSize - entry->compressedPosition;
            if (bufferBytes > 0)
            {
                if (bufferBytes > LZMA_READBUFSIZE)
                    bufferBytes = LZMA_READBUFSIZE;

                entry->bufferedBytes =
                        __PHYSFS_platformRead(entry->archive->stream.File,
                                              entry->buffer, 1, bufferBytes);

                if (entry->bufferedBytes <= 0)
                    break;

                entry->compressedPosition += entry->bufferedBytes;
                entry->bufferPos = entry->buffer;
            } /* if */
        } /* if */

        /* bufferedBytes == 0 if we finished decompressing */
        lzma_err(LzmaDecode(&entry->state,
            entry->bufferPos, entry->bufferedBytes, &usedBufferedBytes,
            outBuffer, wantBytes, &decodedBytes,
            (entry->bufferedBytes == 0)));

        entry->bufferedBytes -= usedBufferedBytes;
        entry->bufferPos += usedBufferedBytes;
        totalDecodedBytes += decodedBytes;
        entry->position += decodedBytes;
    } /* while */

    return(decodedBytes);
} /* LZMA_read */


static PHYSFS_sint64 LZMA_write(fvoid *opaque, const void *buf,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* LZMA_write */


static int LZMA_eof(fvoid *opaque)
{
    LZMAentry *entry = (LZMAentry *) opaque;
    return (entry->compressedPosition >= entry->compressedSize);
} /* LZMA_eof */


static PHYSFS_sint64 LZMA_tell(fvoid *opaque)
{
    LZMAentry *entry = (LZMAentry *) opaque;
    return (entry->position);
} /* LZMA_tell */


static int LZMA_seek(fvoid *opaque, PHYSFS_uint64 offset)
{
    LZMAentry *entry = (LZMAentry *) opaque;
    PHYSFS_uint8 buf[512];
    PHYSFS_uint32 maxread;

    BAIL_IF_MACRO(offset < 0, ERR_SEEK_OUT_OF_RANGE, 0);
    BAIL_IF_MACRO(offset > entry->file->Size, ERR_PAST_EOF, 0);

    if (offset < entry->position)
    {
        __PHYSFS_platformSeek(entry->archive->stream.File,
                              SzArDbGetFolderStreamPos(&entry->archive->db,
                                                       entry->folderIndex, 0));
        entry->position = 0;
        entry->compressedPosition = 0;
        LzmaDecoderInit(&entry->state);
    } /* if */

    while (offset != entry->position)
    {
        maxread = (PHYSFS_uint32) (offset - entry->position);
        if (maxread > sizeof (buf))
            maxread = sizeof (buf);
        if (!LZMA_read(entry, buf, maxread, 1))
            return(0);
    } /* while */

    return(1);
} /* LZMA_seek */


static PHYSFS_sint64 LZMA_fileLength(fvoid *opaque)
{
    return ((LZMAentry *) opaque)->file->Size;
} /* LZMA_fileLength */


static int LZMA_fileClose(fvoid *opaque)
{
    LZMAentry *entry = (LZMAentry *) opaque;

    /* Fix archive */
    if (entry == entry->archive->firstEntry)
        entry->archive->firstEntry = entry->next;
    if (entry == entry->archive->lastEntry)
        entry->archive->lastEntry = entry->previous;

    /* Fix neighbours */
    if (entry->previous != NULL)
        entry->previous->next = entry->next;
    if (entry->next != NULL)
        entry->next->previous = entry->previous;

    /* Free */
    if (entry->state.Probs != NULL)
        allocator.Free(entry->state.Probs);
    if (entry->state.Dictionary != NULL)
        allocator.Free(entry->state.Dictionary);

    allocator.Free(entry);
    return(1);
} /* LZMA_fileClose */


static int LZMA_isArchive(const char *filename, int forWriting)
{
    PHYSFS_uint8 sig[k7zSignatureSize];
    PHYSFS_uint8 res;
    void *in;

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    in = __PHYSFS_platformOpenRead(filename);
    BAIL_IF_MACRO(in == NULL, NULL, 0);

    if (__PHYSFS_platformRead(in, sig, k7zSignatureSize, 1) != 1)
        BAIL_MACRO(NULL, 0);

    res = TestSignatureCandidate(sig);
    __PHYSFS_platformClose(in);

    return res;
} /* LZMA_isArchive */


static void *LZMA_openArchive(const char *name, int forWriting)
{
    LZMAarchive *archive;
    ISzAlloc allocImp;
    ISzAlloc allocTempImp;

    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, NULL);

    archive = (LZMAarchive *) allocator.Malloc(sizeof(LZMAarchive));
    if ((archive->stream.File = __PHYSFS_platformOpenRead(name)) == NULL)
    {
        allocator.Free(archive);
        return NULL;
    } /* if */

    archive->stream.InStream.Read = SzFileReadImp;
    archive->stream.InStream.Seek = SzFileSeekImp;

    archive->firstEntry = NULL;
    archive->lastEntry = NULL;

    allocImp.Alloc = SzAllocPhysicsFS;
    allocImp.Free = SzFreePhysicsFS;

    allocTempImp.Alloc = SzAllocPhysicsFS;
    allocTempImp.Free = SzFreePhysicsFS;

    InitCrcTable();
    SzArDbExInit(&archive->db);
    if (lzma_err(SzArchiveOpen(&archive->stream.InStream, &archive->db,
                               &allocImp, &allocTempImp)) != SZ_OK)
    {
        SzArDbExFree(&archive->db, allocImp.Free);
        __PHYSFS_platformClose(archive->stream.File);
        allocator.Free(archive);
        return NULL;
    } /* if */

    return(archive);
} /* LZMA_openArchive */


/*
 * Moved to seperate function so we can use alloca then immediately throw
 *  away the allocated stack space...
 */
static void doEnumCallback(PHYSFS_EnumFilesCallback cb, void *callbackdata,
                           const char *odir, const char *str, PHYSFS_sint32 ln)
{
    char *newstr = alloca(ln + 1);
    if (newstr == NULL)
        return;

    memcpy(newstr, str, ln);
    newstr[ln] = '\0';
    cb(callbackdata, odir, newstr);
} /* doEnumCallback */


static void LZMA_enumerateFiles(dvoid *opaque, const char *dname,
                                int omitSymLinks, PHYSFS_EnumFilesCallback cb,
                                const char *origdir, void *callbackdata)
{
    LZMAarchive *archive = (LZMAarchive *) opaque;
    PHYSFS_sint32 dlen;
    PHYSFS_sint32 dlen_inc;
    PHYSFS_sint32 max;
    PHYSFS_sint32 i;

    i = lzma_find_start_of_dir(archive, dname, 0);
    if (i == -1)  /* no such directory. */
        return;

    dlen = strlen(dname);
    if ((dlen > 0) && (dname[dlen - 1] == '/')) /* ignore trailing slash. */
        dlen--;

    dlen_inc = ((dlen > 0) ? 1 : 0) + dlen;
    max = (PHYSFS_sint32) archive->db.Database.NumFiles;
    while (i < max)
    {
        char *add;
        char *ptr;
        PHYSFS_sint32 ln;
        char *e = archive->db.Database.Files[i].Name;
        if ((dlen) && ((strncmp(e, dname, dlen)) || (e[dlen] != '/')))
            break;  /* past end of this dir; we're done. */

        add = e + dlen_inc;
        ptr = strchr(add, '/');
        ln = (PHYSFS_sint32) ((ptr) ? ptr-add : strlen(add));
        doEnumCallback(cb, callbackdata, origdir, add, ln);
        ln += dlen_inc;  /* point past entry to children... */

        /* increment counter and skip children of subdirs... */
        while ((++i < max) && (ptr != NULL))
        {
            char *e_new = archive->db.Database.Files[i].Name;
            if ((strncmp(e, e_new, ln) != 0) || (e_new[ln] != '/'))
                break;
        } /* while */
    } /* while */
} /* LZMA_enumerateFiles */


static int LZMA_exists(dvoid *opaque, const char *name)
{
    return(lzma_find_entry((LZMAarchive *) opaque, name) != NULL);
} /* LZMA_exists */


static PHYSFS_sint64 LZMA_getLastModTime(dvoid *opaque,
                                        const char *name,
                                        int *fileExists)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1); /* !!! FIXME: Implement this! */
} /* LZMA_getLastModTime */


static int LZMA_isDirectory(dvoid *opaque, const char *name, int *fileExists)
{
    LZMAentry *entry = lzma_find_entry((LZMAarchive *) opaque, name);
    int isDir = entry->file->IsDirectory;

    *fileExists = (entry != NULL);
    allocator.Free(entry);

    return(isDir);
} /* LZMA_isDirectory */


static int LZMA_isSymLink(dvoid *opaque, const char *name, int *fileExists)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* LZMA_isSymLink */


static fvoid *LZMA_openRead(dvoid *opaque, const char *name, int *fileExists)
{
    LZMAarchive *archive = (LZMAarchive *) opaque;
    LZMAentry *entry = lzma_find_entry(archive, name);
    BAIL_IF_MACRO(entry == NULL, ERR_NO_SUCH_FILE, NULL);

    entry->archive = archive;
    entry->compressedPosition = 0;
    entry->position = 0;
    entry->folderIndex =
            entry->archive->db.FileIndexToFolderIndexMap[entry->index];

    entry->next = NULL;
    entry->previous = entry->archive->lastEntry;
    if (entry->previous != NULL)
        entry->previous->next = entry;
    entry->archive->lastEntry = entry;
    if (entry->archive->firstEntry == NULL)
        entry->archive->firstEntry = entry;

    entry->bufferPos = entry->buffer;
    entry->bufferedBytes = 0;
    entry->compressedSize = SzArDbGetFolderFullPackSize(&entry->archive->db,
                                                        entry->folderIndex);

    __PHYSFS_platformSeek(entry->archive->stream.File,
                          SzArDbGetFolderStreamPos(&entry->archive->db,
                                                   entry->folderIndex, 0));

    /* Create one CLzmaDecoderState per entry */
    CFolder *folder = entry->archive->db.Database.Folders + entry->folderIndex;
    CCoderInfo *coder = folder->Coders;

    if ((folder->NumPackStreams != 1) || (folder->NumCoders != 1))
    {
        LZMA_fileClose(entry);
        BAIL_MACRO(ERR_LZMA_NOTIMPL, NULL);
    } /* if */

    if (lzma_err(LzmaDecodeProperties(&entry->state.Properties, coder->Properties.Items, coder->Properties.Capacity)) != LZMA_RESULT_OK)
        return NULL;

    entry->state.Probs = (CProb *) allocator.Malloc(LzmaGetNumProbs(&entry->state.Properties) * sizeof(CProb));
    if (entry->state.Probs == NULL)
    {
        LZMA_fileClose(entry);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

	if (entry->state.Properties.DictionarySize == 0)
		entry->state.Dictionary = NULL;
	else
	{
		entry->state.Dictionary = (unsigned char *) allocator.Malloc(entry->state.Properties.DictionarySize);
		if (entry->state.Dictionary == NULL)
		{
			LZMA_fileClose(entry);
			BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
		} /* if */
	} /* else */
	LzmaDecoderInit(&entry->state);

	return(entry);
} /* LZMA_openRead */


static fvoid *LZMA_openWrite(dvoid *opaque, const char *filename)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* LZMA_openWrite */


static fvoid *LZMA_openAppend(dvoid *opaque, const char *filename)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* LZMA_openAppend */


static void LZMA_dirClose(dvoid *opaque)
{
	LZMAarchive *archive = (LZMAarchive *) opaque;
	LZMAentry *entry = archive->firstEntry;

	while (entry != NULL)
	{
		entry = entry->next;
		LZMA_fileClose(entry->previous);
	} /* while */

	SzArDbExFree(&archive->db, SzFreePhysicsFS);
	__PHYSFS_platformClose(archive->stream.File);
    allocator.Free(archive);
} /* LZMA_dirClose */


static int LZMA_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* LZMA_remove */


static int LZMA_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* LZMA_mkdir */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_LZMA =
{
    "7Z",
    LZMA_ARCHIVE_DESCRIPTION,
    "Dennis Schridde <devurandom@gmx.net>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_LZMA =
{
    &__PHYSFS_ArchiveInfo_LZMA,
    LZMA_isArchive,          /* isArchive() method      */
    LZMA_openArchive,        /* openArchive() method    */
    LZMA_enumerateFiles,     /* enumerateFiles() method */
    LZMA_exists,             /* exists() method         */
    LZMA_isDirectory,        /* isDirectory() method    */
    LZMA_isSymLink,          /* isSymLink() method      */
    LZMA_getLastModTime,     /* getLastModTime() method */
    LZMA_openRead,           /* openRead() method       */
    LZMA_openWrite,          /* openWrite() method      */
    LZMA_openAppend,         /* openAppend() method     */
    LZMA_remove,             /* remove() method         */
    LZMA_mkdir,              /* mkdir() method          */
    LZMA_dirClose,           /* dirClose() method       */
    LZMA_read,               /* read() method           */
    LZMA_write,              /* write() method          */
    LZMA_eof,                /* eof() method            */
    LZMA_tell,               /* tell() method           */
    LZMA_seek,               /* seek() method           */
    LZMA_fileLength,         /* fileLength() method     */
    LZMA_fileClose           /* fileClose() method      */
};

#endif  /* defined PHYSFS_SUPPORTS_LZMA */

/* end of lzma.c ... */

