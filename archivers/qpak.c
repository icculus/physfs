/*
 * Quake PAK support routines for PhysicsFS.
 *
 * This driver handles id Software Quake PAK files.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ed Sinjiashvili.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined PHYSFS_SUPPORTS_QPAK)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#define QPAK_MAXDIRLEN 60

typedef struct
{
    char          name[56];
    PHYSFS_uint32 offset;
    PHYSFS_uint32 size;
} QPAKentry;

typedef struct tagQPAKdirentry
{
    char                   *name;
    QPAKentry              *entry;
    struct tagQPAKdirentry *next;
} QPAKdirentry;

typedef struct QPAKDirectory
{
    char name[QPAK_MAXDIRLEN];

    struct QPAKDirectory *dirs, *next;

    QPAKdirentry *files;
} QPAKdirectory;

typedef struct
{
    void               *handle;
    char               *filename;
    PHYSFS_uint32       dirOffset;
    PHYSFS_uint32       totalEntries;
    QPAKentry          *entries;
    QPAKdirectory      *root;
} QPAKinfo;

typedef struct
{
    void         *handle;
    QPAKentry    *entry;
    PHYSFS_sint64 curPos;
} QPAKfileinfo;


static int           QPAK_isArchive(const char *filename, int forWriting);
static DirHandle    *QPAK_openArchive(const char *name, int forWriting);
static void          QPAK_dirClose(DirHandle *h);
static LinkedStringList *QPAK_enumerateFiles(DirHandle *h, const char *dirname,
                                             int omitSymLinks);
static int           QPAK_exists(DirHandle *h, const char *name);
static int           QPAK_isDirectory(DirHandle *h, const char *name, int *e);
static int           QPAK_isSymLink(DirHandle *h, const char *name, int *e);
static PHYSFS_sint64 QPAK_getLastModTime(DirHandle *h, const char *n, int *e);
static FileHandle   *QPAK_openRead(DirHandle *h, const char *name, int *e);
static FileHandle   *QPAK_openWrite(DirHandle *h, const char *name);
static FileHandle   *QPAK_openAppend(DirHandle *h, const char *name);


static PHYSFS_sint64 QPAK_read(FileHandle *handle, void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static PHYSFS_sint64 QPAK_write(FileHandle *handle, const void *buffer,
                                PHYSFS_uint32 objSize, PHYSFS_uint32 objCount);
static int           QPAK_eof(FileHandle *handle);
static PHYSFS_sint64 QPAK_tell(FileHandle *handle);
static int           QPAK_seek(FileHandle *handle, PHYSFS_uint64 offset);
static PHYSFS_sint64 QPAK_fileLength(FileHandle *handle);
static int           QPAK_fileClose(FileHandle *handle);
static int           QPAK_remove(DirHandle *h, const char *name);
static int           QPAK_mkdir(DirHandle *h, const char *name);


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_QPAK =
{
    "PAK",
    "Quake PAK file format",
    "Ed Sinjiashvili <slimb@swes.saren.ru>",
    "http://icculus.org/physfs/",
};

static const FileFunctions __PHYSFS_FileFunctions_QPAK =
{
    QPAK_read,               /* read() method       */
    QPAK_write,              /* write() method      */
    QPAK_eof,                /* eof() method        */
    QPAK_tell,               /* tell() method       */
    QPAK_seek,               /* seek() method       */
    QPAK_fileLength,         /* fileLength() method */
    QPAK_fileClose           /* fileClose() method  */
};

const DirFunctions __PHYSFS_DirFunctions_QPAK =
{
    &__PHYSFS_ArchiveInfo_QPAK,
    QPAK_isArchive,          /* isArchive() method      */
    QPAK_openArchive,        /* openArchive() method    */
    QPAK_enumerateFiles,     /* enumerateFiles() method */
    QPAK_exists,             /* exists() method         */
    QPAK_isDirectory,        /* isDirectory() method    */
    QPAK_isSymLink,          /* isSymLink() method      */
    QPAK_getLastModTime,     /* getLastModTime() method */
    QPAK_openRead,           /* openRead() method       */
    QPAK_openWrite,          /* openWrite() method      */
    QPAK_openAppend,         /* openAppend() method     */
    QPAK_remove,             /* remove() method         */
    QPAK_mkdir,              /* mkdir() method          */
    QPAK_dirClose            /* dirClose() method       */
};


#define QPAK_MAGIC 0x4B434150  /* look like "PACK" in ascii. */


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


static int openQPak(const char *filename, int forWriting, void **fileHandle)
{
    PHYSFS_uint32 sig;

    *fileHandle = NULL;
    BAIL_IF_MACRO(forWriting, ERR_ARC_IS_READ_ONLY, 0);

    *fileHandle = __PHYSFS_platformOpenRead(filename);
    BAIL_IF_MACRO(*fileHandle == NULL, NULL, 0);
    
    if (!readui32(*fileHandle, &sig))
        goto openPak_failed;
    
    if (sig != QPAK_MAGIC)
    {
        __PHYSFS_setError(ERR_UNSUPPORTED_ARCHIVE);
        goto openPak_failed;
    } /* if */

    return(1);

openPak_failed:
    if (*fileHandle != NULL)
        __PHYSFS_platformClose(*fileHandle);

    *fileHandle = NULL;
    return(0);
} /* openQPak */


static int QPAK_isArchive(const char *filename, int forWriting)
{
    void *fileHandle;
    int retval = openQPak(filename, forWriting, &fileHandle);

    if (fileHandle != NULL)
        __PHYSFS_platformClose(fileHandle);

    return(retval);
} /* QPAK_isArchive */


static int qpak_loadEntries(void *fh, int dirOffset, int numEntries,
                            QPAKentry *entries)
{
    PHYSFS_uint32 i;

    BAIL_IF_MACRO(__PHYSFS_platformSeek(fh, dirOffset) == 0, NULL, 0);

    for (i = 0; i < numEntries; i++, entries++)
    {
        PHYSFS_sint64 r = __PHYSFS_platformRead(fh, entries->name, 56, 1);
        BAIL_IF_MACRO(r == 0, NULL, 0);
        BAIL_IF_MACRO(!readui32(fh, &entries->offset), NULL, 0);
        BAIL_IF_MACRO(!readui32(fh, &entries->size), NULL, 0);
    } /* for */

    return(1);
} /* qpak_loadEntries */


static QPAKdirentry *qpak_newDirentry(char *name, QPAKentry *entry)
{
    QPAKdirentry *retval = (QPAKdirentry *) malloc(sizeof (QPAKdirentry));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, 0);

    retval->name  = name;
    retval->entry = entry;
    retval->next  = NULL;

    return(retval);
} /* qpak_newDirentry */


static void qpak_deleteDirentry(QPAKdirentry *e)
{
    while (e != NULL)
    {
        QPAKdirentry *next = e->next;
        free(e);
        e = next;
    } /* while */
} /* qpak_deleteDirentry */


static QPAKdirectory *qpak_newDirectory(char *name)
{
    QPAKdirectory *dir = (QPAKdirectory *) malloc(sizeof (QPAKdirectory));
    BAIL_IF_MACRO(dir == NULL, ERR_OUT_OF_MEMORY, 0);

    strcpy(dir->name, name);
    dir->dirs = NULL;
    dir->next = NULL;
    dir->files = NULL;

    return dir;
} /* qpak_newDirectory */


static void qpak_deleteDirectory(QPAKdirectory *d)
{
    while (d != NULL)
    {
        QPAKdirectory *next = d->next;
        qpak_deleteDirentry(d->files);
        qpak_deleteDirectory(d->dirs);
        free(d);
        d = next;
    } /* while */
} /* qpak_deleteDirectory */


static int qpak_addFile(QPAKdirectory *dir, char *name, QPAKentry *entry)
{
    QPAKdirentry *file = qpak_newDirentry(name, entry);
    if (file == NULL)
        return(0);

    /* !!! FIXME: Traversing a linkedlist gets slower with each added file. */
    if (dir->files == NULL)
    {
        dir->files = file;
    } /* if */
    else
    {
        QPAKdirentry *tail = dir->files;
        while (tail->next != NULL)
            tail = tail->next;

        tail->next = file;
    } /* else */

    return(1);
} /* qpak_addFile */


static QPAKdirectory *qpak_findDirectory(QPAKdirectory *root, const char *name)
{
    char *p = strchr(name, '/');

    if (p == NULL)
    {
        QPAKdirectory *thisDir = root->dirs;
        while (thisDir != NULL)
        {
            if (strcmp(thisDir->name, name) == 0)
                return(thisDir);
            thisDir = thisDir->next;
        } /* while */
    } /* if */

    else
    {
        char temp[QPAK_MAXDIRLEN];
        QPAKdirectory *thisDir = root->dirs;

        strncpy (temp, name, p - name);
        temp[p - name] = '\0';

        while (thisDir != NULL)
        {
            if (strcmp(thisDir->name, temp) == 0)
                return(qpak_findDirectory(thisDir, p + 1));
            thisDir = thisDir->next;
        } /* while */
    } /* else */

    BAIL_MACRO(ERR_NO_SUCH_PATH, 0);
} /* qpak_findDirectory */


static QPAKdirectory *qpak_addDir(QPAKdirectory *dir, char *name)
{
    QPAKdirectory *newDir = qpak_findDirectory(dir, name);
    if (newDir != 0)
        return(newDir);

    newDir = qpak_newDirectory(name);
    if (newDir == 0)
        return 0;

    if (dir->dirs == NULL)
    {
        dir->dirs = newDir;
    } /* if */

    else
    {
        QPAKdirectory *tail = dir->dirs;
        while (tail->next != NULL)
            tail = tail->next;

        tail->next = newDir;
    } /* else */

    return(newDir);
} /* qpak_addDir */


static int qpak_addEntry(QPAKdirectory *dir, char *name, QPAKentry *entry)
{
    char tempName[QPAK_MAXDIRLEN];
    QPAKdirectory *child;
    char *p = strchr(name, '/');
    if (p == NULL)
        return(qpak_addFile(dir, name, entry));

    strncpy(tempName, name, p - name);
    tempName[p - name] = '\0';

    child = qpak_addDir(dir, tempName);
    return(qpak_addEntry(child, p + 1, entry));
} /* qpak_addEntry */


static QPAKentry *qpak_findEntry(QPAKdirectory *root, const char *name)
{
    QPAKdirectory *dir = NULL;
    QPAKdirentry *thisFile = NULL;
    const char *t = strrchr (name, '/');

    if (t == NULL)
    {
        dir = root;
        t = name;
    } /* if */

    else
    {
        char temp[QPAK_MAXDIRLEN];
        strncpy(temp, name, t - name);
        temp[t - name] = '\0';
        dir = qpak_findDirectory(root, temp);
        t++;
    } /* else */

    if (dir == NULL)
        return(0);

    thisFile = dir->files;

    while (thisFile != NULL)
    {
        if (strcmp(thisFile->name, t) == 0)
            return(thisFile->entry);

        thisFile = thisFile->next;
    } /* while */

    BAIL_MACRO(ERR_NO_SUCH_FILE, 0);
} /* qpak_findEntry */


static int qpak_populateDirectories(QPAKentry *entries, int numEntries,
                                    QPAKdirectory *root)
{
    PHYSFS_uint32 i;
    QPAKentry *entry = entries;

    for (i = 0; i < numEntries; i++, entry++)
    {
        if (qpak_addEntry(root, entry->name, entry) == 0)
            return(0);
    } /* for */

    return(1);
} /* qpak_populateDirectories */


static void qpak_deletePakInfo(QPAKinfo *pakInfo)
{
    if (pakInfo->handle != NULL)
        __PHYSFS_platformClose(pakInfo->handle);

    if (pakInfo->filename != NULL)
        free(pakInfo->filename);

    if (pakInfo->entries != NULL)
        free(pakInfo->entries);

    qpak_deleteDirectory(pakInfo->root);

    free(pakInfo);
} /* qpak_deletePakInfo */


static int qpak_entry_cmp(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    QPAKentry *a = (QPAKentry *) _a;
    return(strcmp(a[one].name, a[two].name));
} /* qpak_entry_cmp */


static void qpak_entry_swap(void *_a, PHYSFS_uint32 one, PHYSFS_uint32 two)
{
    QPAKentry tmp;
    QPAKentry *first = &(((QPAKentry *) _a)[one]);
    QPAKentry *second = &(((QPAKentry *) _a)[two]);
    memcpy(&tmp, first, sizeof (QPAKentry));
    memcpy(first, second, sizeof (QPAKentry));
    memcpy(second, &tmp, sizeof (QPAKentry));
} /* qpak_entry_swap */


static DirHandle *QPAK_openArchive(const char *name, int forWriting)
{
    void *fh = NULL;
    PHYSFS_uint32 dirOffset, dirLength;
    QPAKinfo *pi;
    DirHandle *retval;

    retval = (DirHandle *) malloc(sizeof (DirHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    pi = (QPAKinfo *) malloc(sizeof (QPAKinfo));
    if (pi == NULL)
    {
        free(retval);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    retval->opaque = pi;

    pi->filename = (char *) malloc(strlen(name) + 1);
    if (pi->filename == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto QPAK_openArchive_failed;
    } /* if */

    if (!openQPak(name, forWriting, &fh))
        goto QPAK_openArchive_failed;

    if (!readui32(fh, &dirOffset))
        goto QPAK_openArchive_failed;

    if (!readui32(fh, &dirLength))
        goto QPAK_openArchive_failed;

    if (__PHYSFS_platformFileLength(fh) < dirOffset + dirLength)
        goto QPAK_openArchive_failed;

    strcpy(pi->filename, name);
    pi->handle = fh;
    pi->dirOffset = dirOffset;
    pi->totalEntries = dirLength / 64;

    pi->entries = (QPAKentry *) malloc(pi->totalEntries * sizeof (QPAKentry));
    if (pi->entries == NULL)
    {
        __PHYSFS_setError(ERR_OUT_OF_MEMORY);
        goto QPAK_openArchive_failed;
    } /* if */

    if (qpak_loadEntries(fh, dirOffset, pi->totalEntries, pi->entries) == 0)
        goto QPAK_openArchive_failed;

    __PHYSFS_sort(pi->entries, pi->totalEntries,
                  qpak_entry_cmp, qpak_entry_swap);

    pi->root = qpak_newDirectory("");
    if (pi->root == NULL)
        goto QPAK_openArchive_failed;

    if (qpak_populateDirectories(pi->entries, pi->totalEntries, pi->root) == 0)
        goto QPAK_openArchive_failed;

    retval->funcs = &__PHYSFS_DirFunctions_QPAK;
    return(retval);

QPAK_openArchive_failed:
    if (retval != NULL)
    {
        if (retval->opaque != NULL)
            qpak_deletePakInfo((QPAKinfo *) retval->opaque);

        free(retval);
    } /* if */

    if (fh != NULL)
        __PHYSFS_platformClose(fh);

    return(0);
} /* QPAK_openArchive */


static void QPAK_dirClose(DirHandle *dirHandle)
{
    qpak_deletePakInfo((QPAKinfo *) dirHandle->opaque);
    free(dirHandle);
} /* QPAK_dirClose */


static LinkedStringList *QPAK_enumerateFiles(DirHandle *h, const char *dirname,
                                             int omitSymLinks)
{
    LinkedStringList *retval = NULL, *p = NULL;
    QPAKdirectory *dir;
    QPAKinfo *info = (QPAKinfo *) h->opaque;

    if ((dirname == NULL) || (*dirname == '\0'))
        dir = info->root;
    else
        dir = qpak_findDirectory(info->root, dirname);

    if (dir != NULL)
    {
        QPAKdirectory *child = dir->dirs;
        QPAKdirentry *file = dir->files;

        while (child != NULL)
        {
            retval = __PHYSFS_addToLinkedStringList(retval, &p, child->name, -1);
            child = child->next;
        } /* while */

        while (file != NULL)
        {
            retval = __PHYSFS_addToLinkedStringList(retval, &p, file->name, -1);
            file = file->next;
        } /* while */
    } /* if */

    return(retval);
} /* QPAK_enumerateFiles */


static int QPAK_exists(DirHandle *h, const char *name)
{
    QPAKinfo *driver = (QPAKinfo *) h->opaque;

    if ((name == NULL) || (*name == '\0'))
        return(0);
    
    if (qpak_findDirectory(driver->root, name) != 0)
        return(1);

    if (qpak_findEntry(driver->root, name) != 0)
        return(1);

    return(0);
} /* QPAK_exists */


static int QPAK_isDirectory(DirHandle *h, const char *name, int *fileExists)
{
    QPAKinfo *info = (QPAKinfo *) h->opaque;
    *fileExists = (qpak_findDirectory(info->root, name) != 0);
    return(*fileExists);
} /* QPAK_isDirectory */


static int QPAK_isSymLink(DirHandle *h, const char *name, int *fileExists)
{
    *fileExists = QPAK_exists(h, name);
    return(0); /* we don't support symlinks for now */
} /* QPAK_isSymlink */


static int QPAK_remove(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* QPAK_remove */


static int QPAK_mkdir(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* QPAK_mkdir */


static PHYSFS_sint64 QPAK_getLastModTime(DirHandle *h,
                                         const char *name,
                                         int *fileExists)
{
    QPAKinfo *info = (QPAKinfo *) h->opaque;
    PHYSFS_sint64 retval = -1;

    *fileExists = QPAK_exists(h, name);
    if (*fileExists)
        retval = __PHYSFS_platformGetLastModTime(info->filename);

    return(retval);
} /* QPAK_getLastModTime */


static void *qpak_getFileHandle(const char *name, QPAKentry *entry)
{
    void *retval = __PHYSFS_platformOpenRead(name);
    if (retval == NULL)
        return(NULL);

    if (!__PHYSFS_platformSeek(retval, entry->offset))
    {
        __PHYSFS_platformClose(retval);
        return(NULL);
    } /* if */

    return(retval);
} /* qpak_getFileHandle */


static FileHandle *QPAK_openRead(DirHandle *h, const char *fnm, int *fileExists)
{
    QPAKinfo *driver = (QPAKinfo *) h->opaque;
    QPAKentry *entry = qpak_findEntry(driver->root, fnm);
    QPAKfileinfo *fileDriver = 0;
    FileHandle *result = 0;

    *fileExists = (entry != NULL);
    if (entry == NULL)
        return(NULL);
 
    fileDriver = (QPAKfileinfo *) malloc(sizeof (QPAKfileinfo));
    BAIL_IF_MACRO(fileDriver == NULL, ERR_OUT_OF_MEMORY, NULL);
    
    fileDriver->handle = qpak_getFileHandle(driver->filename, entry);
    if (fileDriver->handle == NULL)
    {
        free(fileDriver);
        return(NULL);
    } /* if */

    fileDriver->entry = entry;
    fileDriver->curPos = 0;

    result = (FileHandle *)malloc(sizeof (FileHandle));
    if (result == NULL)
    {
        __PHYSFS_platformClose(fileDriver->handle);
        free(fileDriver);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    result->opaque = fileDriver;
    result->dirHandle = h;
    result->funcs = &__PHYSFS_FileFunctions_QPAK;
    return(result);
} /* QPAK_openRead */


static FileHandle *QPAK_openWrite(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* QPAK_openWrite */


static FileHandle *QPAK_openAppend(DirHandle *h, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* QPAK_openAppend */


static PHYSFS_sint64 QPAK_read(FileHandle *handle, void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) (handle->opaque);
    QPAKentry *entry = finfo->entry;
    PHYSFS_uint64 bytesLeft = entry->size - finfo->curPos;
    PHYSFS_uint64 objsLeft = (bytesLeft / objSize);
    PHYSFS_sint64 rc;

    if (objsLeft < objCount)
        objCount = (PHYSFS_uint32) objsLeft;

    rc = __PHYSFS_platformRead(finfo->handle, buffer, objSize, objCount);
    if (rc > 0)
        finfo->curPos += (rc * objSize);

    return(rc);
} /* QPAK_read */


static PHYSFS_sint64 QPAK_write(FileHandle *handle, const void *buffer,
                                PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* QPAK_write */


static int QPAK_eof(FileHandle *handle)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) (handle->opaque);
    QPAKentry *entry = finfo->entry;
    
    return(finfo->curPos >= (PHYSFS_sint64) entry->size);
} /* QPAK_eof */


static PHYSFS_sint64 QPAK_tell(FileHandle *handle)
{
    return(((QPAKfileinfo *) handle->opaque)->curPos);
} /* QPAK_tell */


static int QPAK_seek(FileHandle *handle, PHYSFS_uint64 offset)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) handle->opaque;
    QPAKentry *entry = finfo->entry;
    PHYSFS_uint64 newPos = entry->offset + offset;
    int rc;

    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(newPos > entry->offset + entry->size, ERR_PAST_EOF, 0);
    rc = __PHYSFS_platformSeek(finfo->handle, newPos);
    if (rc)
        finfo->curPos = offset;

    return(rc);
}  /* QPAK_seek */


static PHYSFS_sint64 QPAK_fileLength(FileHandle *handle)
{
    return ((QPAKfileinfo *) handle->opaque)->entry->size;
} /* QPAK_fileLength */


static int QPAK_fileClose(FileHandle *handle)
{
    QPAKfileinfo *finfo = (QPAKfileinfo *) handle->opaque;
    BAIL_IF_MACRO(!__PHYSFS_platformClose(finfo->handle), NULL, 0);
    free(finfo);
    free(handle);
    return(1);
} /* QPAK_fileClose */

#endif  /* defined PHYSFS_SUPPORTS_QPAK */

/* end of qpak.c ... */


