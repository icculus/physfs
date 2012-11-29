/*
 * LZMA support routines for PhysicsFS.
 *
 * Please see the file lzma.txt in the lzma/ directory.
 *
 *  This file was written by Dennis Schridde, with some peeking at "7zMain.c"
 *   by Igor Pavlov.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_7Z

#include "lzma/C/7zCrc.h"
#include "lzma/C/Archive/7z/7zIn.h"
#include "lzma/C/Archive/7z/7zExtract.h"


/* 7z internal from 7zIn.c */
extern int TestSignatureCandidate(Byte *testBytes);


#ifdef _LZMA_IN_CB
# define BUFFER_SIZE (1 << 12)
#endif /* _LZMA_IN_CB */


/*
 * Carries filestream metadata through 7z
 */
typedef struct _FileInputStream
{
    ISzAlloc allocImp; /* Allocation implementation, used by 7z */
    ISzAlloc allocTempImp; /* Temporary allocation implementation, used by 7z */
    ISzInStream inStream; /* Input stream with read callbacks, used by 7z */
    PHYSFS_Io *io;  /* Filehandle, used by read implementation */
#ifdef _LZMA_IN_CB
    Byte buffer[BUFFER_SIZE]; /* Buffer, used by read implementation */
#endif /* _LZMA_IN_CB */
} FileInputStream;

/*
 * In the 7z format archives are splited into blocks, those are called folders
 * Set by LZMA_read()
*/
typedef struct _LZMAfolder
{
    PHYSFS_uint32 index; /* Index of folder in archive */
    PHYSFS_uint32 references; /* Number of files using this block */
    PHYSFS_uint8 *cache; /* Cached folder */
    size_t size; /* Size of folder */
} LZMAfolder;

/*
 * Set by LZMA_openArchive(), except folder which gets it's values
 *  in LZMA_read()
 */
typedef struct _LZMAarchive
{
    struct _LZMAfile *files; /* Array of files, size == archive->db.Database.NumFiles */
    LZMAfolder *folders; /* Array of folders, size == archive->db.Database.NumFolders */
    CArchiveDatabaseEx db; /* For 7z: Database */
    FileInputStream stream; /* For 7z: Input file incl. read and seek callbacks */
} LZMAarchive;

/* Set by LZMA_openArchive(), except offset which is set by LZMA_read() */
typedef struct _LZMAfile
{
    PHYSFS_uint32 index; /* Index of file in archive */
    LZMAarchive *archive; /* Link to corresponding archive */
    LZMAfolder *folder; /* Link to corresponding folder */
    CFileItem *item; /* For 7z: File info, eg. name, size */
    size_t offset; /* Offset in folder */
    size_t position; /* Current "virtual" position in file */
} LZMAfile;


/* Memory management implementations to be passed to 7z */

static void *SzAllocPhysicsFS(size_t size)
{
    return ((size == 0) ? NULL : allocator.Malloc(size));
} /* SzAllocPhysicsFS */


static void SzFreePhysicsFS(void *address)
{
    if (address != NULL)
        allocator.Free(address);
} /* SzFreePhysicsFS */


/* Filesystem implementations to be passed to 7z */

#ifdef _LZMA_IN_CB

/*
 * Read implementation, to be passed to 7z
 * WARNING: If the ISzInStream in 'object' is not contained in a valid FileInputStream this _will_ break horribly!
 */
SZ_RESULT SzFileReadImp(void *object, void **buffer, size_t maxReqSize,
                        size_t *processedSize)
{
    FileInputStream *s = (FileInputStream *)(object - offsetof(FileInputStream, inStream)); /* HACK! */
    PHYSFS_sint64 processedSizeLoc = 0;

    if (maxReqSize > BUFFER_SIZE)
        maxReqSize = BUFFER_SIZE;
    processedSizeLoc = s->io->read(s->io, s->buffer, maxReqSize);
    *buffer = s->buffer;
    if (processedSize != NULL)
        *processedSize = (size_t) processedSizeLoc;

    return SZ_OK;
} /* SzFileReadImp */

#else

/*
 * Read implementation, to be passed to 7z
 * WARNING: If the ISzInStream in 'object' is not contained in a valid FileInputStream this _will_ break horribly!
 */
SZ_RESULT SzFileReadImp(void *object, void *buffer, size_t size,
                        size_t *processedSize)
{
    FileInputStream *s = (FileInputStream *)((unsigned long)object - offsetof(FileInputStream, inStream)); /* HACK! */
    const size_t processedSizeLoc = s->io->read(s->io, buffer, size);
    if (processedSize != NULL)
        *processedSize = processedSizeLoc;
    return SZ_OK;
} /* SzFileReadImp */

#endif

/*
 * Seek implementation, to be passed to 7z
 * WARNING: If the ISzInStream in 'object' is not contained in a valid FileInputStream this _will_ break horribly!
 */
SZ_RESULT SzFileSeekImp(void *object, CFileSize pos)
{
    FileInputStream *s = (FileInputStream *)((unsigned long)object - offsetof(FileInputStream, inStream)); /* HACK! */
    if (s->io->seek(s->io, (PHYSFS_uint64) pos))
        return SZ_OK;
    return SZE_FAIL;
} /* SzFileSeekImp */


/*
 * Translate Microsoft FILETIME (used by 7zip) into UNIX timestamp
 */
static PHYSFS_sint64 lzma_filetime_to_unix_timestamp(CArchiveFileTime *ft)
{
    /* MS counts in nanoseconds ... */
    const PHYSFS_uint64 FILETIME_NANOTICKS_PER_SECOND = __PHYSFS_UI64(10000000);
    /* MS likes to count seconds since 01.01.1601 ... */
    const PHYSFS_uint64 FILETIME_UNIX_DIFF = __PHYSFS_UI64(11644473600);

    PHYSFS_uint64 filetime = ft->Low | ((PHYSFS_uint64)ft->High << 32);
    return filetime/FILETIME_NANOTICKS_PER_SECOND - FILETIME_UNIX_DIFF;
} /* lzma_filetime_to_unix_timestamp */


/*
 * Compare a file with a given name, C89 stdlib variant
 * Used for sorting
 */
static int lzma_file_cmp_stdlib(const void *key, const void *object)
{
    const char *name = (const char *) key;
    LZMAfile *file = (LZMAfile *) object;
    return strcmp(name, file->item->Name);
} /* lzma_file_cmp_posix */


/*
 * Compare two files with each other based on the name
 * Used for sorting
 */
static int lzma_file_cmp(void *_a, size_t one, size_t two)
{
    LZMAfile *files = (LZMAfile *) _a;
    return strcmp(files[one].item->Name, files[two].item->Name);
} /* lzma_file_cmp */


/*
 * Swap two entries in the file array
 */
static void lzma_file_swap(void *_a, size_t one, size_t two)
{
    LZMAfile tmp;
    LZMAfile *first = &(((LZMAfile *) _a)[one]);
    LZMAfile *second = &(((LZMAfile *) _a)[two]);
    memcpy(&tmp, first, sizeof (LZMAfile));
    memcpy(first, second, sizeof (LZMAfile));
    memcpy(second, &tmp, sizeof (LZMAfile));
} /* lzma_file_swap */


/*
 * Find entry 'name' in 'archive'
 */
static LZMAfile * lzma_find_file(const LZMAarchive *archive, const char *name)
{
    LZMAfile *file = bsearch(name, archive->files, archive->db.Database.NumFiles, sizeof(*archive->files), lzma_file_cmp_stdlib); /* FIXME: Should become __PHYSFS_search!!! */

    BAIL_IF_MACRO(file == NULL, PHYSFS_ERR_NOT_FOUND, NULL);

    return file;
} /* lzma_find_file */


/*
 * Load metadata for the file at given index
 */
static int lzma_file_init(LZMAarchive *archive, PHYSFS_uint32 fileIndex)
{
    LZMAfile *file = &archive->files[fileIndex];
    PHYSFS_uint32 folderIndex = archive->db.FileIndexToFolderIndexMap[fileIndex];

    file->index = fileIndex; /* Store index into 7z array, since we sort our own. */
    file->archive = archive;
    file->folder = (folderIndex != (PHYSFS_uint32)-1 ? &archive->folders[folderIndex] : NULL); /* Directories don't have a folder (they contain no own data...) */
    file->item = &archive->db.Database.Files[fileIndex]; /* Holds crucial data and is often referenced -> Store link */
    file->position = 0;
    file->offset = 0; /* Offset will be set by LZMA_read() */

    return 1;
} /* lzma_load_file */


/*
 * Load metadata for all files
 */
static int lzma_files_init(LZMAarchive *archive)
{
    PHYSFS_uint32 fileIndex = 0, numFiles = archive->db.Database.NumFiles;

    for (fileIndex = 0; fileIndex < numFiles; fileIndex++ )
    {
        if (!lzma_file_init(archive, fileIndex))
        {
            return 0; /* FALSE on failure */
        }
    } /* for */

   __PHYSFS_sort(archive->files, (size_t) numFiles, lzma_file_cmp, lzma_file_swap);

    return 1;
} /* lzma_load_files */


/*
 * Initialise specified archive
 */
static void lzma_archive_init(LZMAarchive *archive)
{
    memset(archive, 0, sizeof(*archive));

    /* Prepare callbacks for 7z */
    archive->stream.inStream.Read = SzFileReadImp;
    archive->stream.inStream.Seek = SzFileSeekImp;

    archive->stream.allocImp.Alloc = SzAllocPhysicsFS;
    archive->stream.allocImp.Free = SzFreePhysicsFS;

    archive->stream.allocTempImp.Alloc = SzAllocPhysicsFS;
    archive->stream.allocTempImp.Free = SzFreePhysicsFS;
}


/*
 * Deinitialise archive
 */
static void lzma_archive_exit(LZMAarchive *archive)
{
    /* Free arrays */
    allocator.Free(archive->folders);
    allocator.Free(archive->files);
    allocator.Free(archive);
}

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
            __PHYSFS_setError(PHYSFS_ERR_CORRUPT); /*!!!FIXME: was "PHYSFS_ERR_DATA_ERROR" */
            break;
        case SZE_OUTOFMEMORY:
            __PHYSFS_setError(PHYSFS_ERR_OUT_OF_MEMORY);
            break;
        case SZE_CRC_ERROR:
            __PHYSFS_setError(PHYSFS_ERR_CORRUPT);
            break;
        case SZE_NOTIMPL:
            __PHYSFS_setError(PHYSFS_ERR_UNSUPPORTED);
            break;
        case SZE_FAIL:
            __PHYSFS_setError(PHYSFS_ERR_OTHER_ERROR);  /* !!! FIXME: right? */
            break;
        case SZE_ARCHIVE_ERROR:
            __PHYSFS_setError(PHYSFS_ERR_CORRUPT);  /* !!! FIXME: right? */
            break;
        default:
            __PHYSFS_setError(PHYSFS_ERR_OTHER_ERROR);
    } /* switch */

    return rc;
} /* lzma_err */


static PHYSFS_sint64 LZMA_read(PHYSFS_Io *io, void *outBuf, PHYSFS_uint64 len)
{
    LZMAfile *file = (LZMAfile *) io->opaque;

    size_t wantedSize = (size_t) len;
    const size_t remainingSize = file->item->Size - file->position;
    size_t fileSize = 0;

    BAIL_IF_MACRO(wantedSize == 0, ERRPASS, 0); /* quick rejection. */
    BAIL_IF_MACRO(remainingSize == 0, PHYSFS_ERR_PAST_EOF, 0);

    if (wantedSize > remainingSize)
        wantedSize = remainingSize;

    /* Only decompress the folder if it is not already cached */
    if (file->folder->cache == NULL)
    {
        const int rc = lzma_err(SzExtract(
            &file->archive->stream.inStream, /* compressed data */
            &file->archive->db, /* 7z's database, containing everything */
            file->index, /* Index into database arrays */
            /* Index of cached folder, will be changed by SzExtract */
            &file->folder->index,
            /* Cache for decompressed folder, allocated/freed by SzExtract */
            &file->folder->cache,
            /* Size of cache, will be changed by SzExtract */
            &file->folder->size,
            /* Offset of this file inside the cache, set by SzExtract */
            &file->offset,
            &fileSize, /* Size of this file */
            &file->archive->stream.allocImp,
            &file->archive->stream.allocTempImp));

        if (rc != SZ_OK)
            return -1;
    } /* if */

    /* Copy wanted bytes over from cache to outBuf */
    memcpy(outBuf, (file->folder->cache + file->offset + file->position),
            wantedSize);
    file->position += wantedSize; /* Increase virtual position */

    return wantedSize;
} /* LZMA_read */


static PHYSFS_sint64 LZMA_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, -1);
} /* LZMA_write */


static PHYSFS_sint64 LZMA_tell(PHYSFS_Io *io)
{
    LZMAfile *file = (LZMAfile *) io->opaque;
    return file->position;
} /* LZMA_tell */


static int LZMA_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    LZMAfile *file = (LZMAfile *) io->opaque;

    BAIL_IF_MACRO(offset > file->item->Size, PHYSFS_ERR_PAST_EOF, 0);

    file->position = offset; /* We only use a virtual position... */

    return 1;
} /* LZMA_seek */


static PHYSFS_sint64 LZMA_length(PHYSFS_Io *io)
{
    const LZMAfile *file = (LZMAfile *) io->opaque;
    return (file->item->Size);
} /* LZMA_length */


static PHYSFS_Io *LZMA_duplicate(PHYSFS_Io *_io)
{
    /* !!! FIXME: this archiver needs to be reworked to allow multiple
     * !!! FIXME:  opens before we worry about duplication. */
    BAIL_MACRO(PHYSFS_ERR_UNSUPPORTED, NULL);
} /* LZMA_duplicate */


static int LZMA_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }


static void LZMA_destroy(PHYSFS_Io *io)
{
    LZMAfile *file = (LZMAfile *) io->opaque;

    if (file->folder != NULL)
    {
        /* Only decrease refcount if someone actually requested this file... Prevents from overflows and close-on-open... */
        if (file->folder->references > 0)
            file->folder->references--;
        if (file->folder->references == 0)
        {
            /* Free the cache which might have been allocated by LZMA_read() */
            allocator.Free(file->folder->cache);
            file->folder->cache = NULL;
        }
        /* !!! FIXME: we don't free (file) or (file->folder)?! */
    } /* if */
} /* LZMA_destroy */


static const PHYSFS_Io LZMA_Io =
{
    CURRENT_PHYSFS_IO_API_VERSION, NULL,
    LZMA_read,
    LZMA_write,
    LZMA_seek,
    LZMA_tell,
    LZMA_length,
    LZMA_duplicate,
    LZMA_flush,
    LZMA_destroy
};


static void *LZMA_openArchive(PHYSFS_Io *io, const char *name, int forWriting)
{
    PHYSFS_uint8 sig[k7zSignatureSize];
    size_t len = 0;
    LZMAarchive *archive = NULL;

    assert(io != NULL);  /* shouldn't ever happen. */

    BAIL_IF_MACRO(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    if (io->read(io, sig, k7zSignatureSize) != k7zSignatureSize)
        return 0;
    BAIL_IF_MACRO(!TestSignatureCandidate(sig), PHYSFS_ERR_UNSUPPORTED, NULL);
    BAIL_IF_MACRO(!io->seek(io, 0), ERRPASS, NULL);

    archive = (LZMAarchive *) allocator.Malloc(sizeof (LZMAarchive));
    BAIL_IF_MACRO(archive == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    lzma_archive_init(archive);
    archive->stream.io = io;

    CrcGenerateTable();
    SzArDbExInit(&archive->db);
    if (lzma_err(SzArchiveOpen(&archive->stream.inStream,
                               &archive->db,
                               &archive->stream.allocImp,
                               &archive->stream.allocTempImp)) != SZ_OK)
    {
        SzArDbExFree(&archive->db, SzFreePhysicsFS);
        lzma_archive_exit(archive);
        return NULL; /* Error is set by lzma_err! */
    } /* if */

    len = archive->db.Database.NumFiles * sizeof (LZMAfile);
    archive->files = (LZMAfile *) allocator.Malloc(len);
    if (archive->files == NULL)
    {
        SzArDbExFree(&archive->db, SzFreePhysicsFS);
        lzma_archive_exit(archive);
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    }

    /*
     * Init with 0 so we know when a folder is already cached
     * Values will be set by LZMA_openRead()
     */
    memset(archive->files, 0, len);

    len = archive->db.Database.NumFolders * sizeof (LZMAfolder);
    archive->folders = (LZMAfolder *) allocator.Malloc(len);
    if (archive->folders == NULL)
    {
        SzArDbExFree(&archive->db, SzFreePhysicsFS);
        lzma_archive_exit(archive);
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    }

    /*
     * Init with 0 so we know when a folder is already cached
     * Values will be set by LZMA_read()
     */
    memset(archive->folders, 0, len);

    if(!lzma_files_init(archive))
    {
        SzArDbExFree(&archive->db, SzFreePhysicsFS);
        lzma_archive_exit(archive);
        BAIL_MACRO(PHYSFS_ERR_OTHER_ERROR, NULL);
    }

    return archive;
} /* LZMA_openArchive */


/*
 * Moved to seperate function so we can use alloca then immediately throw
 *  away the allocated stack space...
 */
static void doEnumCallback(PHYSFS_EnumFilesCallback cb, void *callbackdata,
                           const char *odir, const char *str, size_t flen)
{
    char *newstr = __PHYSFS_smallAlloc(flen + 1);
    if (newstr == NULL)
        return;

    memcpy(newstr, str, flen);
    newstr[flen] = '\0';
    cb(callbackdata, odir, newstr);
    __PHYSFS_smallFree(newstr);
} /* doEnumCallback */


static void LZMA_enumerateFiles(void *opaque, const char *dname,
                                PHYSFS_EnumFilesCallback cb,
                                const char *origdir, void *callbackdata)
{
    size_t dlen = strlen(dname),
           dlen_inc = dlen + ((dlen > 0) ? 1 : 0);
    LZMAarchive *archive = (LZMAarchive *) opaque;
    LZMAfile *file = NULL,
            *lastFile = &archive->files[archive->db.Database.NumFiles];
        if (dlen)
        {
            file = lzma_find_file(archive, dname);
            if (file != NULL) /* if 'file' is NULL it should stay so, otherwise errors will not be handled */
                file += 1;
        }
        else
        {
            file = archive->files;
        }

    BAIL_IF_MACRO(file == NULL, PHYSFS_ERR_NOT_FOUND, );

    while (file < lastFile)
    {
        const char * fname = file->item->Name;
        const char * dirNameEnd = fname + dlen_inc;

        if (strncmp(dname, fname, dlen) != 0) /* Stop after mismatch, archive->files is sorted */
            break;

        if (strchr(dirNameEnd, '/')) /* Skip subdirs */
        {
            file++;
            continue;
        }

        /* Do the actual callback... */
        doEnumCallback(cb, callbackdata, origdir, dirNameEnd, strlen(dirNameEnd));

        file++;
    }
} /* LZMA_enumerateFiles */


static PHYSFS_Io *LZMA_openRead(void *opaque, const char *name,
                                int *fileExists)
{
    LZMAarchive *archive = (LZMAarchive *) opaque;
    LZMAfile *file = lzma_find_file(archive, name);
    PHYSFS_Io *io = NULL;

    *fileExists = (file != NULL);
    BAIL_IF_MACRO(file == NULL, PHYSFS_ERR_NOT_FOUND, NULL);
    BAIL_IF_MACRO(file->folder == NULL, PHYSFS_ERR_NOT_A_FILE, NULL);

    file->position = 0;
    file->folder->references++; /* Increase refcount for automatic cleanup... */

    io = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    BAIL_IF_MACRO(io == NULL, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    memcpy(io, &LZMA_Io, sizeof (*io));
    io->opaque = file;

    return io;
} /* LZMA_openRead */


static PHYSFS_Io *LZMA_openWrite(void *opaque, const char *filename)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* LZMA_openWrite */


static PHYSFS_Io *LZMA_openAppend(void *opaque, const char *filename)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, NULL);
} /* LZMA_openAppend */


static void LZMA_closeArchive(void *opaque)
{
    LZMAarchive *archive = (LZMAarchive *) opaque;

#if 0  /* !!! FIXME: you shouldn't have to do this. */
    PHYSFS_uint32 fileIndex = 0, numFiles = archive->db.Database.NumFiles;
    for (fileIndex = 0; fileIndex < numFiles; fileIndex++)
    {
        LZMA_fileClose(&archive->files[fileIndex]);
    } /* for */
#endif

    SzArDbExFree(&archive->db, SzFreePhysicsFS);
    archive->stream.io->destroy(archive->stream.io);
    lzma_archive_exit(archive);
} /* LZMA_closeArchive */


static int LZMA_remove(void *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* LZMA_remove */


static int LZMA_mkdir(void *opaque, const char *name)
{
    BAIL_MACRO(PHYSFS_ERR_READ_ONLY, 0);
} /* LZMA_mkdir */

static int LZMA_stat(void *opaque, const char *filename,
                     int *exists, PHYSFS_Stat *stat)
{
    const LZMAarchive *archive = (const LZMAarchive *) opaque;
    const LZMAfile *file = lzma_find_file(archive, filename);

    *exists = (file != 0);
    if (!file)
        return 0;

    if(file->item->IsDirectory)
    {
        stat->filesize = 0;
        stat->filetype = PHYSFS_FILETYPE_DIRECTORY;
    } /* if */
    else
    {
        stat->filesize = (PHYSFS_sint64) file->item->Size;
        stat->filetype = PHYSFS_FILETYPE_REGULAR;
    } /* else */

    /* !!! FIXME: the 0's should be -1's? */
    if (file->item->IsLastWriteTimeDefined)
        stat->modtime = lzma_filetime_to_unix_timestamp(&file->item->LastWriteTime);
    else
        stat->modtime = 0;

    /* real create and accesstype are currently not in the lzma SDK */
    stat->createtime = stat->modtime;
    stat->accesstime = 0;

    stat->readonly = 1;  /* 7zips are always read only */

    return 1;
} /* LZMA_stat */


const PHYSFS_Archiver __PHYSFS_Archiver_LZMA =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "7Z",
        "LZMA (7zip) format",
        "Dennis Schridde <devurandom@gmx.net>",
        "http://icculus.org/physfs/",
    },
    0,  /* supportsSymlinks */
    LZMA_openArchive,        /* openArchive() method    */
    LZMA_enumerateFiles,     /* enumerateFiles() method */
    LZMA_openRead,           /* openRead() method       */
    LZMA_openWrite,          /* openWrite() method      */
    LZMA_openAppend,         /* openAppend() method     */
    LZMA_remove,             /* remove() method         */
    LZMA_mkdir,              /* mkdir() method          */
    LZMA_closeArchive,       /* closeArchive() method   */
    LZMA_stat                /* stat() method           */
};

#endif  /* defined PHYSFS_SUPPORTS_7Z */

/* end of lzma.c ... */

