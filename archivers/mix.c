/*
 * MIX support routines for PhysicsFS.
 *
 * This driver handles old archives used in the famous games
 * Command&Conquer Tiberium Dawn and Command&Conquer Red Alert.
 *
 * Newer MIX files as they are used in C&C Tiberium Sun and C&C Red Alert 2
 * aren't supported yet. Keep your eyes open for future updates.
 *
 * A MIX file has three parts:
 *   (1) Header
 *        16bit integer  -> number of files stored in this MIX
 *        32bit integer  -> filesize
 *   (2) "Directory"
 *        32bit integer  -> hash of the filename
 *        32bit integer  -> starting offset in the MIX
 *        32bit integer  -> end offset in the MIX
 *   (3) Data (BODY)
 *        All data comes here
 *
 * NOTES:
 *  The offsets are relative to the body. So offset 0 is directly after
 *  the directory.
 *
 *  Filenames only exist as hashes. So enumerate_files() will only report all
 *  hashes. Searching a filename in hashes is extremly quick so I decided not
 *  to include any sorting routines after then opening of the archive.
 *
 *
 * I found the structure of MIX files here: 
 * http://www.geocities.com/SiliconValley/8682/cncmap1f.txt
 *
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Sebastian Steinhauer <steini@steini-welt.de>
 */
 
#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined PHYSFS_SUPPORTS_MIX)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

typedef struct
{
    PHYSFS_uint16 num_files;
    PHYSFS_uint32 filesize;
} MIXheader;

typedef struct
{
    PHYSFS_uint32 hash;
    PHYSFS_uint32 start_offset;
    PHYSFS_uint32 end_offset;
} MIXentry;

typedef struct
{
    char *filename; /* filename of the archive */
    MIXentry *entry; /* list of entries */
    MIXheader header; /* the header of the MIX file */
    PHYSFS_uint32 delta; /* size of header + entries */
} MIXinfo;

typedef struct
{
    PHYSFS_uint64 size; /* filesize */
    PHYSFS_uint64 cur_pos; /* position in this file */
    MIXentry *entry; /* pointer to the MIX entry */
    MIXinfo *info; /* pointer to our MIXinfo */
    void *handle; /* filehandle */
} MIXfileinfo;


static PHYSFS_uint32 MIX_hash(const char *name)
{
    PHYSFS_uint32 id = 0;
    PHYSFS_uint32 a = 0;
    PHYSFS_uint32 i = 0;
    PHYSFS_uint32 l;
    PHYSFS_uint32 j;
    
    l = strlen(name);
    while (i < l)
    {
        a = 0;
        for(j = 0; j < 4; j++)
        {
            a >>= 8;
            if (i < l)
            {
                a += (unsigned int) (name[i]) << 24;
                i++;
            } /* if */
        } /* for */

        id = (id << 1 | id >> 31) + a;
    } /* while */

    /* a bit debuggin :)
    /printf("Filename %s -> %X\n",name,id); */

    return(id);
} /* MIX_hash */


static void MIX_dirClose(dvoid *opaque)
{
    MIXinfo *info = ((MIXinfo *) opaque);
    free(info->entry);
    free(info->filename);
} /* MIX_dirClose */


static PHYSFS_sint64 MIX_read(fvoid *opaque, void *buffer,
                              PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    MIXfileinfo *finfo = (MIXfileinfo *) opaque;
    MIXentry *entry = finfo->entry;
    PHYSFS_uint32 read;
    
    /* set position in the archive */
    __PHYSFS_platformSeek(finfo->handle,
                             finfo->info->delta +
                             entry->start_offset +
                             finfo->cur_pos);
    
    /* read n bytes */
    read = __PHYSFS_platformRead(finfo->handle, buffer, objSize, objCount);
    
    /* keep filepointer up to date */
    if (read)
        finfo->cur_pos += read * objSize;
    
    return(read);
} /* MIX_read */


static PHYSFS_sint64 MIX_write(fvoid *opaque, const void *buffer,
                               PHYSFS_uint32 objSize, PHYSFS_uint32 objCount)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, -1);
} /* MIX_write */


static int MIX_eof(fvoid *opaque)
{
    MIXfileinfo *fifo = (MIXfileinfo *) opaque;
    return(fifo->cur_pos >= fifo->size);
} /* MIX_eof */


static PHYSFS_sint64 MIX_tell(fvoid *opaque)
{
    return(((MIXfileinfo *) opaque)->cur_pos);
} /* MIX_tell */


static int MIX_seek(fvoid *opaque, PHYSFS_uint64 offset)
{
    MIXfileinfo *h = (MIXfileinfo *) opaque;
    
    BAIL_IF_MACRO(offset < 0, ERR_INVALID_ARGUMENT, 0);
    BAIL_IF_MACRO(offset >= h->size, ERR_PAST_EOF, 0);
    h->cur_pos = offset;
    return(1);
} /* MIX_seek */


static PHYSFS_sint64 MIX_fileLength(fvoid *opaque)
{
    return (((MIXfileinfo *) opaque)->size);
} /* MIX_fileLength */


static int MIX_fileClose(fvoid *opaque)
{
    MIXfileinfo *finfo = (MIXfileinfo *) opaque;
    __PHYSFS_platformClose(finfo->handle);
    free(finfo);
    return(1);
} /* MIX_fileClose */


static int MIX_isArchive(const char *filename, int forWriting)
{
    /* !!! FIXME:
            write a simple detection routine for MIX files.
            Unfortunaly MIX files have no ID in the header.
    */
    return(1);
} /* MIX_isArchive */


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


static void *MIX_openArchive(const char *name, int forWriting)
{
    PHYSFS_uint32 i = 0;
    MIXinfo *info = NULL;
    void *handle = NULL;

    info = (MIXinfo *) malloc(sizeof (MIXinfo));
    BAIL_IF_MACRO(info == NULL, ERR_OUT_OF_MEMORY, 0);
    memset(info, '\0', sizeof (MIXinfo));

    info->filename = (char *) malloc(strlen(name) + 1);
    GOTO_IF_MACRO(!info->filename, ERR_OUT_OF_MEMORY, MIX_openArchive_failed);

    /* store filename */
    strcpy(info->filename, name);
    
    /* open the file */
    handle = __PHYSFS_platformOpenRead(name);
    if (!handle)
        goto MIX_openArchive_failed;

    /* read the MIX header */
    if ( (!readui16(handle, &info->header.num_files)) ||
         (!readui32(handle, &info->header.filesize)) )
        goto MIX_openArchive_failed;

    info->delta = 6 + (info->header.num_files * 12);

    /* allocate space for the entries and read the entries */
    info->entry = malloc(sizeof (MIXentry) * info->header.num_files);
    GOTO_IF_MACRO(!info->entry, ERR_OUT_OF_MEMORY, MIX_openArchive_failed);

    /* read the directory list */
    for (i = 0; i < header.num_files; i++)
    {
        if ( (!readui32(handle, &info->entry[i].hash)) ||
             (!readui32(handle, &info->entry[i].start_offset)) ||
             (!readui32(handle, &info->entry[i].end_offset)) )
            goto MIX_openArchive_failed;
    } /* for */

    __PHYSFS_platformClose(handle);

    return(info);

MIX_openArchive_failed:
    if (info != NULL)
    {
        if (info->filename != NULL)
            free(info->filename);
        if (info->entry != NULL)
            free(info->entry);
        free(info);
    } /* if */

    if (handle != NULL)
        __PHYSFS_platformClose(handle);

    return(NULL);
} /* MIX_openArchive */


static void MIX_enumerateFiles(dvoid *opaque, const char *dname,
                               int omitSymLinks, PHYSFS_StringCallback cb,
                               void *callbackdata)
{
    /* no directories in MIX files. */
    if (*dirname != '\0')
    {
        MIXinfo *info = (MIXinfo*) opaque;
        MIXentry *entry = info->entry;
        int i;
        char buffer[32];
    
        for (i = 0; i < info->header.num_files; i++, entry++)
        {
            sprintf(buffer, "%X", entry->hash);
            cb(callbackdata, buffer);
        } /* for */
    } /* if */
} /* MIX_enumerateFiles */


static MIXentry *MIX_find_entry(MIXinfo *info, const char *name)
{
    MIXentry *entry = info->entry;
    PHYSFS_uint32 i, id;
    
    /* create hash */
    id = MIX_hash(name);
    
    /* look for this hash */
    for (i = 0; i < info->header.num_files; i++, entry++)
    {
        if (entry->hash == id)
            return(entry);
    } /* for */

    /* nothing found... :( */
    return(NULL);
} /* MIX_find_entry */


static int MIX_exists(dvoid *opaque, const char *name)
{
    return(MIX_find_entry(((MIXinfo *) opaque), name) != NULL);
} /* MIX_exists */


static int MIX_isDirectory(dvoid *opaque, const char *name, int *fileExists)
{
    *fileExists = MIX_exists(opaque, name);
    return(0);  /* never directories in a MIX */
} /* MIX_isDirectory */


static int MIX_isSymLink(dvoid *opaque, const char *name, int *fileExists)
{
    *fileExists = MIX_exists(opaque, name);
    return(0);  /* never symlinks in a MIX. */
} /* MIX_isSymLink */


static PHYSFS_sint64 MIX_getLastModTime(dvoid *opaque,
                                        const char *name,
                                        int *fileExists)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);  /* !!! FIXME: return .MIX's modtime. */
} /* MIX_getLastModTime */


static fvoid *MIX_openRead(dvoid *opaque, const char *fnm, int *fileExists)
{
    MIXinfo *info = ((MIXinfo*) opaque);
    MIXfileinfo *finfo;
    MIXentry *entry;
    
    /* try to find this file */
    entry = MIX_find_entry(info,fnm);
    BAIL_IF_MACRO(entry == NULL, ERR_NO_SUCH_FILE, NULL);
    
    /* allocate a MIX handle */
    finfo = (MIXfileinfo *) malloc(sizeof (MIXfileinfo));
    BAIL_IF_MACRO(finfo == NULL, ERR_OUT_OF_MEMORY, NULL);

    /* open the archive */
    finfo->handle = __PHYSFS_platformOpenRead(info->filename);
    if(!finfo->handle)
    {
        free(finfo);
        return(NULL);
    } /* if */
    
    /* setup structures */
    finfo->cur_pos = 0;
    finfo->info = info;
    finfo->entry = entry;
    finfo->size = entry->end_offset - entry->start_offset;

    return(finfo);
} /* MIX_openRead */


static fvoid *MIX_openWrite(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* MIX_openWrite */


static fvoid *MIX_openAppend(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, NULL);
} /* MIX_openAppend */


static int MIX_remove(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* MIX_remove */


static int MIX_mkdir(dvoid *opaque, const char *name)
{
    BAIL_MACRO(ERR_NOT_SUPPORTED, 0);
} /* MIX_mkdir */


const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_MIX =
{
    "MIX",
    "Westwood archive (Tiberian Dawn / Red Alert)",
    "Sebastian Steinhauer <steini@steini-welt.de>",
    "http://icculus.org/physfs/",
};


const PHYSFS_Archiver __PHYSFS_Archiver_MIX =
{
    &__PHYSFS_ArchiveInfo_MIX,
    MIX_isArchive,          /* isArchive() method      */
    MIX_openArchive,        /* openArchive() method    */
    MIX_enumerateFiles,     /* enumerateFiles() method */
    MIX_exists,             /* exists() method         */
    MIX_isDirectory,        /* isDirectory() method    */
    MIX_isSymLink,          /* isSymLink() method      */
    MIX_getLastModTime,     /* getLastModTime() method */
    MIX_openRead,           /* openRead() method       */
    MIX_openWrite,          /* openWrite() method      */
    MIX_openAppend,         /* openAppend() method     */
    MIX_remove,             /* remove() method         */
    MIX_mkdir,              /* mkdir() method          */
    MIX_dirClose,           /* dirClose() method       */
    MIX_read,               /* read() method           */
    MIX_write,              /* write() method          */
    MIX_eof,                /* eof() method            */
    MIX_tell,               /* tell() method           */
    MIX_seek,               /* seek() method           */
    MIX_fileLength,         /* fileLength() method     */
    MIX_fileClose           /* fileClose() method      */
};

#endif  /* defined PHYSFS_SUPPORTS_MIX */

/* end of mix.c ... */

