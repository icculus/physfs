/*
 * Standard directory I/O support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "physfs.h"

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

extern const DirFunctions __PHYSFS_DirFunctions_DIR;
static const FileFunctions __PHYSFS_FileFunctions_DIR;
static const FileFunctions __PHYSFS_FileFunctions_DIRW;

static int DIR_read(FileHandle *handle, void *buffer,
                    unsigned int objSize, unsigned int objCount)
{
    FILE *h = (FILE *) (handle->opaque);
    int retval;

    errno = 0;
    retval = fread(buffer, objSize, objCount, h);
    BAIL_IF_MACRO((retval < objCount) && (ferror(h)),strerror(errno),retval);

    return(retval);
} /* DIR_read */


static int DIR_write(FileHandle *handle, void *buffer,
                     unsigned int objSize, unsigned int objCount)
{
    FILE *h = (FILE *) (handle->opaque);
    int retval;

    errno = 0;
    retval = fwrite(buffer, objSize, objCount, h);
    if ( (retval < objCount) && (ferror(h)) )
        __PHYSFS_setError(strerror(errno));

    return(retval);
} /* DIR_write */


static int DIR_eof(FileHandle *handle)
{
    return(feof((FILE *) (handle->opaque)));
} /* DIR_eof */


static int DIR_tell(FileHandle *handle)
{
    return(ftell((FILE *) (handle->opaque)));
} /* DIR_tell */


static int DIR_seek(FileHandle *handle, int offset)
{
    return(fseek((FILE *) (handle->opaque), offset, SEEK_SET) == 0);
} /* DIR_seek */


static int DIR_fileClose(FileHandle *handle)
{
    FILE *h = (FILE *) (handle->opaque);

    /*
     * we manually fflush() the buffer, since that's the place fclose() will
     *  most likely fail, but that will leave the file handle in an undefined
     *  state if it fails. fflush() failures we can recover from.
     */

    /* keep trying until there's success or an unrecoverable error... */
    do {
        __PHYSFS_platformTimeslice();
        errno = 0;
    } while ( (fflush(h) == EOF) && ((errno == EAGAIN) || (errno == EINTR)) );

    /* EBADF == "Not open for writing". That's fine. */
    BAIL_IF_MACRO((errno != 0) && (errno != EBADF), strerror(errno), 0);

    /* if fclose fails anyhow, we just have to pray that it's still usable. */
    errno = 0;
    BAIL_IF_MACRO(fclose(h) == EOF, strerror(errno), 0);  /* (*shrug*) */

    free(handle);
    return(1);
} /* DIR_fileClose */


static int DIR_isArchive(const char *filename, int forWriting)
{
    /* directories ARE archives in this driver... */
    return(__PHYSFS_platformIsDirectory(filename));
} /* DIR_isArchive */


static DirHandle *DIR_openArchive(const char *name, int forWriting)
{
    const char *dirsep = __PHYSFS_platformDirSeparator;
    DirHandle *retval = NULL;
    int namelen = strlen(name);
    int seplen = strlen(dirsep);

    BAIL_IF_MACRO(!DIR_isArchive(name, forWriting),
                    ERR_UNSUPPORTED_ARCHIVE, NULL);

    retval = malloc(sizeof (DirHandle));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    retval->opaque = malloc(namelen + seplen + 1);
    if (retval->opaque == NULL)
    {
        free(retval);
        BAIL_IF_MACRO(0, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

        /* make sure there's a dir separator at the end of the string */
    strcpy((char *) (retval->opaque), name);
    if (strcmp((name + namelen) - seplen, dirsep) != 0)
        strcat((char *) (retval->opaque), dirsep);

    retval->funcs = &__PHYSFS_DirFunctions_DIR;
    return(retval);
} /* DIR_openArchive */


static LinkedStringList *DIR_enumerateFiles(DirHandle *h, const char *dname)
{
    char *d = __PHYSFS_platformCvtToDependent((char *)(h->opaque),dname,NULL);
    LinkedStringList *retval;

    BAIL_IF_MACRO(d == NULL, NULL, NULL);
    retval = __PHYSFS_platformEnumerateFiles(d);
    free(d);
    return(retval);
} /* DIR_enumerateFiles */


static int DIR_exists(DirHandle *h, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *)(h->opaque), name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);
    retval = __PHYSFS_platformExists(f);
    free(f);
    return(retval);
} /* DIR_exists */


static int DIR_isDirectory(DirHandle *h, const char *name)
{
    char *d = __PHYSFS_platformCvtToDependent((char *)(h->opaque), name, NULL);
    int retval;

    BAIL_IF_MACRO(d == NULL, NULL, 0);
    retval = __PHYSFS_platformIsDirectory(d);
    free(d);
    return(retval);
} /* DIR_isDirectory */


static int DIR_isSymLink(DirHandle *h, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *)(h->opaque), name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0); /* !!! might be a problem. */
    retval = __PHYSFS_platformIsSymLink(f);
    free(f);
    return(retval);
} /* DIR_isSymLink */


static FileHandle *doOpen(DirHandle *h, const char *name, const char *mode)
{
    char *f = __PHYSFS_platformCvtToDependent((char *)(h->opaque), name, NULL);
    FILE *rc;
    FileHandle *retval;
    char *str;

    BAIL_IF_MACRO(f == NULL, NULL, NULL);

    retval = (FileHandle *) malloc(sizeof (FileHandle));
    if (!retval)
    {
        free(f);
        BAIL_IF_MACRO(0, ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    errno = 0;
    rc = fopen(f, mode);
    str = strerror(errno);
    free(f);

    if (!rc)
    {
        free(retval);
        BAIL_IF_MACRO(0, str, NULL);
    } /* if */

    retval->opaque = (void *) rc;
    retval->dirHandle = h;
    retval->funcs = &__PHYSFS_FileFunctions_DIR;
    return(retval);
} /* doOpen */


static FileHandle *DIR_openRead(DirHandle *h, const char *filename)
{
    return(doOpen(h, filename, "rb"));
} /* DIR_openRead */


static FileHandle *DIR_openWrite(DirHandle *h, const char *filename)
{
    return(doOpen(h, filename, "wb"));
} /* DIR_openWrite */


static FileHandle *DIR_openAppend(DirHandle *h, const char *filename)
{
    return(doOpen(h, filename, "ab"));
} /* DIR_openAppend */


static int DIR_remove(DirHandle *h, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *)(h->opaque), name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);

    errno = 0;
    retval = (remove(f) == 0);
    if (!retval)
        __PHYSFS_setError(strerror(errno));

    free(f);
    return(retval);
} /* DIR_remove */


static int DIR_mkdir(DirHandle *h, const char *name)
{
    char *f = __PHYSFS_platformCvtToDependent((char *)(h->opaque), name, NULL);
    int retval;

    BAIL_IF_MACRO(f == NULL, NULL, 0);

    errno = 0;
    retval = (mkdir(f, S_IRWXU) == 0);
    if (!retval)
        __PHYSFS_setError(strerror(errno));

    free(f);
    return(retval);
} /* DIR_mkdir */


static void DIR_dirClose(DirHandle *h)
{
    free(h->opaque);
    free(h);
} /* DIR_dirClose */



static const FileFunctions __PHYSFS_FileFunctions_DIR =
{
    DIR_read,       /* read() method      */
    NULL,           /* write() method     */
    DIR_eof,        /* eof() method       */
    DIR_tell,       /* tell() method      */
    DIR_seek,       /* seek() method      */
    DIR_fileClose,  /* fileClose() method */
};


static const FileFunctions __PHYSFS_FileFunctions_DIRW =
{
    NULL,           /* read() method      */
    DIR_write,      /* write() method     */
    DIR_eof,        /* eof() method       */
    DIR_tell,       /* tell() method      */
    DIR_seek,       /* seek() method      */
    DIR_fileClose   /* fileClose() method */
};


const DirFunctions __PHYSFS_DirFunctions_DIR =
{
    DIR_isArchive,          /* isArchive() method      */
    DIR_openArchive,        /* openArchive() method    */
    DIR_enumerateFiles,     /* enumerateFiles() method */
    DIR_exists,             /* exists() method         */
    DIR_isDirectory,        /* isDirectory() method    */
    DIR_isSymLink,          /* isSymLink() method      */
    DIR_openRead,           /* openRead() method       */
    DIR_openWrite,          /* openWrite() method      */
    DIR_openAppend,         /* openAppend() method     */
    DIR_remove,             /* remove() method         */
    DIR_mkdir,              /* mkdir() method          */
    DIR_dirClose            /* dirClose() method       */
};


/* This doesn't get listed, since it's technically not an archive... */
#if 0
const PHYSFS_ArchiveInfo __PHYSFS_ArchiveInfo_DIR =
{
    "DIR",
    "non-archive directory I/O"
};
#endif

/* end of dir.c ... */

