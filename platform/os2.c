/*
 * OS/2 support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined OS2)

#define INCL_DOSSEMAPHORES
#define INCL_DOSDATETIME
#define INCL_DOSFILEMGR
#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS
#define INCL_DOSPROCESS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSMISC
#include <os2.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

const char *__PHYSFS_platformDirSeparator = "\\";


static APIRET os2err(APIRET retval)
{
    const char *err = NULL;

    /*
     * The ones that say "OS/2 reported" are more for PhysicsFS developer
     *  debugging. We give more generic messages for ones that are likely to
     *  fall through to an application.
     */
    switch (retval)
    {
        case NO_ERROR: /* Don't set the PhysicsFS error message for these... */
        case ERROR_INTERRUPT:
        case ERROR_TIMEOUT:
            break;

        case ERROR_NOT_ENOUGH_MEMORY:
            err = ERR_OUT_OF_MEMORY;
            break;

        case ERROR_FILE_NOT_FOUND:
            err = "File not found";
            break;

        case ERROR_PATH_NOT_FOUND:
            err = "Path not found";
            break;

        case ERROR_ACCESS_DENIED:
            err = "Access denied";
            break;

        case ERROR_NOT_DOS_DISK:
            err = "Not a DOS disk";
            break;

        case ERROR_SHARING_VIOLATION:
            err = "Sharing violation";
            break;

        case ERROR_CANNOT_MAKE:
            err = "Cannot make";
            break;

        case ERROR_DEVICE_IN_USE:
            err = "Device already in use";
            break;

        case ERROR_OPEN_FAILED:
            err = "Open failed";
            break;

        case ERROR_DISK_FULL:
            err = "Disk is full";
            break;

        case ERROR_PIPE_BUSY:
            err = "Pipe busy";
            break;

        case ERROR_SHARING_BUFFER_EXCEEDED:
            err = "Sharing buffer exceeded";
            break;

        case ERROR_FILENAME_EXCED_RANGE:
        case ERROR_META_EXPANSION_TOO_LONG:
            err = "Filename too big";
            break;

        case ERROR_TOO_MANY_HANDLES:
        case ERROR_TOO_MANY_OPEN_FILES:
        case ERROR_NO_MORE_SEARCH_HANDLES:
            err = "Too many open handles";
            break;

        case ERROR_SEEK_ON_DEVICE:
            err = "Seek error";  /* Is that what this error means? */
            break;

        case ERROR_NEGATIVE_SEEK:
            err = "Seek past start of file";
            break;

        case ERROR_CURRENT_DIRECTORY:
            err = "Trying to delete current working directory";
            break;

        case ERROR_WRITE_PROTECT:
            err = "Write protect error";
            break;

        case ERROR_WRITE_FAULT:
            err = "Write fault";
            break;

        case ERROR_LOCK_VIOLATION:
            err = "Lock violation";
            break;

        case ERROR_GEN_FAILURE:
            err = "General failure";
            break;

        case ERROR_UNCERTAIN_MEDIA:
            err = "Uncertain media";
            break;

        case ERROR_PROTECTION_VIOLATION:
            err = "Protection violation";
            break;

        case ERROR_INVALID_PARAMETER:
        case ERROR_INVALID_NAME:
        case ERROR_INVALID_DRIVE:
        case ERROR_INVALID_HANDLE:
        case ERROR_INVALID_FUNCTION:
        case ERROR_INVALID_LEVEL:
        case ERROR_INVALID_CATEGORY:
        case ERROR_DUPLICATE_NAME:
        case ERROR_BUFFER_OVERFLOW:
        case ERROR_BAD_LENGTH:
        case ERROR_BAD_DRIVER_LEVEL:
        case ERROR_DIRECT_ACCESS_HANDLE:
            err = "OS/2 reported an invalid parameter to an API function";
            break;

        case ERROR_NO_VOLUME_LABEL:
            err = "OS/2 reported no volume label";
            break;

        case ERROR_NO_MORE_FILES:
            err = "OS/2 reported no more files";
            break;

        case ERROR_MONITORS_NOT_SUPPORTED:
            err = "OS/2 reported monitors not supported";
            break;

        case ERROR_BROKEN_PIPE:
            err = "OS/2 reported a broken pipe";
            break;

        case ERROR_MORE_DATA:
            err = "OS/2 reported \"more data\" (?)";
            break;

        case ERROR_EAS_DIDNT_FIT:
            err = "OS/2 reported Extended Attributes didn't fit";
            break;

        case ERROR_INVALID_EA_NAME:
            err = "OS/2 reported an invalid Extended Attribute name";
            break;

        case ERROR_EA_LIST_INCONSISTENT:
            err = "OS/2 reported an inconsistent Extended Attribute list";
            break;

        case ERROR_SEM_OWNER_DIED:
            err = "OS/2 reported that semaphore owner died";
            break;

        case ERROR_TOO_MANY_SEM_REQUESTS:
            err = "OS/2 reported too many semaphore requests";
            break;

        case ERROR_SEM_BUSY:
            err = "OS/2 reported a blocked semaphore";
            break;

        case ERROR_NOT_OWNER:
            err = "OS/2 reported that we used a resource we don't own.";
            break;

        default:
            err = "OS/2 reported back with unrecognized error code";
            break;
    } /* switch */

    if (err != NULL)
        __PHYSFS_setError(err);

    return(retval);
} /* os2err */


static char *baseDir = NULL;

int __PHYSFS_platformInit(void)
{
    char buf[CCHMAXPATH];
    APIRET rc;
    PTIB ptib;
    PPIB ppib;
    PHYSFS_sint32 len;

    assert(baseDir == NULL);

    BAIL_IF_MACRO(os2err(DosGetInfoBlocks(&ptib, &ppib)) != NO_ERROR, NULL, 0);
    rc = DosQueryModuleName(ppib->pib_hmte, sizeof (buf), (PCHAR) buf);
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, 0);

    /* chop off filename, leave path. */
    for (len = strlen(buf) - 1; len >= 0; len--)
    {
        if (buf[len] == '\\')
        {
            buf[++len] = '\0';
            break;
        } /* if */
    } /* for */

    assert(len > 0);  /* should have been an "x:\\" on the front on string. */

    baseDir = (char *) malloc(len + 1);
    BAIL_IF_MACRO(baseDir == NULL, ERR_OUT_OF_MEMORY, 0);
    strcpy(baseDir, buf);

    return(1);  /* success. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    assert(baseDir != NULL);
    free(baseDir);
    baseDir = NULL;
    return(1);  /* success. */
} /* __PHYSFS_platformDeinit */


static int disc_is_inserted(ULONG drive)
{
    int rc;
    char buf[20];
    DosError(FERR_DISABLEHARDERR | FERR_DISABLEEXCEPTION);
    rc = DosQueryFSInfo(drive + 1, FSIL_VOLSER, buf, sizeof (buf));
    DosError(FERR_ENABLEHARDERR | FERR_ENABLEEXCEPTION);
    return(rc == NO_ERROR);
} /* is_cdrom_inserted */


static int is_cdrom_drive(ULONG drive)
{
    PHYSFS_uint16 cmd = (((drive & 0xFF) << 8) | 0);
    BIOSPARAMETERBLOCK bpb;
    ULONG ul1, ul2;
    APIRET rc;

    rc = DosDevIOCtl((HFILE) -1, IOCTL_DISK,
                     DSK_GETDEVICEPARAMS,
                     &cmd, sizeof (cmd), &ul1,
                     &bpb, sizeof (bpb), &ul2);

    /*
     * !!! FIXME: Note that this tells us that the media is REMOVABLE...
     * !!! FIXME:  but it might not be a CD-ROM...check driver name?
     */
    return((rc == NO_ERROR) && ((bpb.fsDeviceAttr & 0x0001) == 0));
} /* is_cdrom_drive */


char **__PHYSFS_platformDetectAvailableCDs(void)
{
    ULONG dummy;
    ULONG drivemap;
    ULONG i, bit;
    APIRET rc;
    char **retval;
    PHYSFS_uint32 cd_count = 0;

    retval = (char **) malloc(sizeof (char *));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    *retval = NULL;

    rc = DosQueryCurrentDisk(&dummy, &drivemap);
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, retval);

    /* !!! FIXME: the a, b, and c drives are almost certainly NOT cdroms... */
    for (i = 0, bit = 1; i < 26; i++, bit <<= 1)
    {
        if (drivemap & bit)  /* this logical drive exists. */
        {
            if ((is_cdrom_drive(i)) && (disc_is_inserted(i)))
            {
                char **tmp = realloc(retval, sizeof (char *) * (cd_count + 1));
                if (tmp)
                {
                    char *str = (char *) malloc(4);
                    retval = tmp;
                    retval[cd_count - 1] = str;
                    if (str)
                    {
                        str[0] = ('A' + i);
                        str[1] = ':';
                        str[2] = '\\';
                        str[3] = '\0';
                        cd_count++;
                    } /* if */
                } /* if */
            } /* if */
        } /* if */
    } /* for */

    return(retval);
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    char *retval = (char *) malloc(strlen(baseDir) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, baseDir); /* calculated at init time. */
    return(retval);
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformGetUserName(void)
{
    return(NULL);  /* (*shrug*) */
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    return(__PHYSFS_platformCalcBaseDir(NULL));
} /* __PHYSFS_platformGetUserDir */


int __PHYSFS_platformStricmp(const char *x, const char *y)
{
    int ux, uy;

    do
    {
        ux = toupper((int) *x);
        uy = toupper((int) *y);
        if (ux > uy)
            return(1);
        else if (ux < uy)
            return(-1);
        x++;
        y++;
    } while ((ux) && (uy));

    return(0);
} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformExists(const char *fname)
{
    FILESTATUS3 fs;
    APIRET rc = DosQueryPathInfo(fname, FIL_STANDARD, &fs, sizeof (fs));
    return(os2err(rc) == NO_ERROR);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
    return(0);  /* no symlinks in OS/2. */
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    FILESTATUS3 fs;
    APIRET rc = DosQueryPathInfo(fname, FIL_STANDARD, &fs, sizeof (fs));
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, 0)
    return((fs.attrFile & FILE_DIRECTORY) != 0);
} /* __PHYSFS_platformIsDirectory */


char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append)
{
    int len = ((prepend) ? strlen(prepend) : 0) +
              ((append) ? strlen(append) : 0) +
              strlen(dirName) + 1;
    char *retval = malloc(len);
    char *p;

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    if (prepend)
        strcpy(retval, prepend);
    else
        retval[0] = '\0';

    strcat(retval, dirName);

    if (append)
        strcat(retval, append);

    for (p = strchr(retval, '/'); p != NULL; p = strchr(p + 1, '/'))
        *p = '\\';

    return(retval);
} /* __PHYSFS_platformCvtToDependent */


LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks)
{
    char spec[CCHMAXPATH];
    LinkedStringList *retval = NULL, *p = NULL;
    FILEFINDBUF3 fb;
    HDIR hdir = HDIR_CREATE;
    ULONG count = 1;
    APIRET rc;

    BAIL_IF_MACRO(strlen(dirname) > sizeof (spec) - 4, ERR_OS_ERROR, NULL);
    strcpy(spec, dirname);
    strcat(spec, "*.*");

    rc = DosFindFirst(spec, &hdir,
                      FILE_DIRECTORY | FILE_ARCHIVED |
                      FILE_READONLY | FILE_HIDDEN | FILE_SYSTEM,
                      &fb, sizeof (fb), &count, FIL_STANDARD);
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, 0);
    while (count == 1)
    {
        if (strcmp(fb.achName, ".") == 0)
            continue;

        if (strcmp(fb.achName, "..") == 0)
            continue;

        retval = __PHYSFS_addToLinkedStringList(retval, &p, fb.achName, -1);

        DosFindNext(hdir, &fb, sizeof (fb), &count);
    } /* while */

    DosFindClose(hdir);
    return(retval);
} /* __PHYSFS_platformEnumerateFiles */


char *__PHYSFS_platformCurrentDir(void)
{
    char *retval;
    ULONG currentDisk;
    ULONG dummy;
    ULONG pathSize = 0;
    APIRET rc;
    BYTE byte;

    rc = DosQueryCurrentDisk(&currentDisk, &dummy);
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, NULL);

    /* The first call just tells us how much space we need for the string. */
    rc = DosQueryCurrentDir(currentDisk, &byte, &pathSize);
    pathSize++; /* Add space for null terminator. */
    retval = (char *) malloc(pathSize + 3);  /* plus "x:\\" */
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    /* Actually get the string this time. */
    rc = DosQueryCurrentDir(currentDisk, (PBYTE) (retval + 3), &pathSize);
    if (os2err(rc) != NO_ERROR)
    {
        free(retval);
        return(NULL);
    } /* if */

    retval[0] = ('A' + (currentDisk - 1));
    retval[1] = ':';
    retval[2] = '\\';
    return(retval);
} /* __PHYSFS_platformCurrentDir */


char *__PHYSFS_platformRealPath(const char *path)
{
    char buf[CCHMAXPATH];
    char *retval;
    APIRET rc = DosQueryPathInfo(path, FIL_QUERYFULLNAME, buf, sizeof (buf));
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, NULL);
    retval = (char *) malloc(strlen(buf) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, buf);
    return(retval);
} /* __PHYSFS_platformRealPath */


int __PHYSFS_platformMkDir(const char *path)
{
    return(os2err(DosCreateDir(path, NULL)) == NO_ERROR);
} /* __PHYSFS_platformMkDir */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    ULONG actionTaken = 0;
    HFILE hfile = NULLHANDLE;

    /*
     * File must be opened SHARE_DENYWRITE and ACCESS_READONLY, otherwise
     *  DosQueryFileInfo() will fail if we try to get a file length, etc.
     */
    os2err(DosOpen(filename, &hfile, &actionTaken, 0, FILE_NORMAL,
                   OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                   OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NO_LOCALITY |
                   OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYWRITE |
                   OPEN_ACCESS_READONLY, NULL));

    return((void *) hfile);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    ULONG actionTaken = 0;
    HFILE hfile = NULLHANDLE;

    /*
     * File must be opened SHARE_DENYWRITE and ACCESS_READWRITE, otherwise
     *  DosQueryFileInfo() will fail if we try to get a file length, etc.
     */
    os2err(DosOpen(filename, &hfile, &actionTaken, 0, FILE_NORMAL,
                   OPEN_ACTION_REPLACE_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW,
                   OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NO_LOCALITY |
                   OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYWRITE |
                   OPEN_ACCESS_READWRITE, NULL));

    return((void *) hfile);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    ULONG dummy = 0;
    HFILE hfile = NULLHANDLE;
    APIRET rc;

    /*
     * File must be opened SHARE_DENYWRITE and ACCESS_READWRITE, otherwise
     *  DosQueryFileInfo() will fail if we try to get a file length, etc.
     */
    rc = os2err(DosOpen(filename, &hfile, &dummy, 0, FILE_NORMAL,
                   OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_CREATE_IF_NEW,
                   OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_NO_LOCALITY |
                   OPEN_FLAGS_NOINHERIT | OPEN_SHARE_DENYWRITE |
                   OPEN_ACCESS_READWRITE, NULL));

    if (rc == NO_ERROR)
    {
        if (os2err(DosSetFilePtr(hfile, 0, FILE_END, &dummy)) != NO_ERROR)
        {
            DosClose(hfile);
            hfile = NULLHANDLE;
        } /* if */
    } /* if */

    return((void *) hfile);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HFILE hfile = (HFILE) opaque;
    PHYSFS_sint64 retval;
    ULONG br;

    for (retval = 0; retval < count; retval++)
    {
        os2err(DosRead(hfile, buffer, size, &br));
        if (br < size)
        {
            DosSetFilePtr(hfile, -br, FILE_CURRENT, &br); /* try to cleanup. */
            return(retval);
        } /* if */

        buffer = (void *) ( ((char *) buffer) + size );
    } /* for */

    return(retval);
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HFILE hfile = (HFILE) opaque;
    PHYSFS_sint64 retval;
    ULONG bw;

    for (retval = 0; retval < count; retval++)
    {
        os2err(DosWrite(hfile, buffer, size, &bw));
        if (bw < size)
        {
            DosSetFilePtr(hfile, -bw, FILE_CURRENT, &bw); /* try to cleanup. */
            return(retval);
        } /* if */

        buffer = (void *) ( ((char *) buffer) + size );
    } /* for */

    return(retval);
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    ULONG dummy;
    HFILE hfile = (HFILE) opaque;
    LONG dist = (LONG) pos;

    /* hooray for 32-bit filesystem limits!  :) */
    BAIL_IF_MACRO((PHYSFS_uint64) dist != pos, ERR_SEEK_OUT_OF_RANGE, 0);

    return(os2err(DosSetFilePtr(hfile, dist, FILE_BEGIN, &dummy)) == NO_ERROR);
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    ULONG pos;
    HFILE hfile = (HFILE) opaque;
    APIRET rc = os2err(DosSetFilePtr(hfile, 0, FILE_CURRENT, &pos));
    BAIL_IF_MACRO(rc != NO_ERROR, NULL, -1);
    return((PHYSFS_sint64) pos);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    FILESTATUS3 fs;
    HFILE hfile = (HFILE) opaque;
    APIRET rc = DosQueryFileInfo(hfile, FIL_STANDARD, &fs, sizeof (fs));
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, -1);
    return((PHYSFS_sint64) fs.cbFile);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformEOF(void *opaque)
{
    PHYSFS_sint64 len, pos;

    len = __PHYSFS_platformFileLength(opaque);
    BAIL_IF_MACRO(len == -1, NULL, 1);  /* (*shrug*) */
    pos = __PHYSFS_platformTell(opaque);
    BAIL_IF_MACRO(pos == -1, NULL, 1);  /* (*shrug*) */

    return(pos >= len);
} /* __PHYSFS_platformEOF */


int __PHYSFS_platformFlush(void *opaque)
{
    return(os2err(DosResetBuffer((HFILE) opaque) == NO_ERROR));
} /* __PHYSFS_platformFlush */


int __PHYSFS_platformClose(void *opaque)
{
    return(os2err(DosClose((HFILE) opaque) == NO_ERROR));
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    if (__PHYSFS_platformIsDirectory(path))
        return(os2err(DosDeleteDir(path)) == NO_ERROR);

    return(os2err(DosDelete(path) == NO_ERROR));
} /* __PHYSFS_platformDelete */


PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname)
{
    PHYSFS_sint64 retval;
    struct tm tm;
    FILESTATUS3 fs;
    APIRET rc = DosQueryPathInfo(fname, FIL_STANDARD, &fs, sizeof (fs));
    BAIL_IF_MACRO(os2err(rc) != NO_ERROR, NULL, -1);

    /* Convert to a format that mktime() can grok... */
    tm.tm_sec = ((PHYSFS_uint32) fs.ftimeLastWrite.twosecs) * 2;
    tm.tm_min = fs.ftimeLastWrite.minutes;
    tm.tm_hour = fs.ftimeLastWrite.hours;
    tm.tm_mday = fs.fdateLastWrite.day;
    tm.tm_mon = fs.fdateLastWrite.month;
    tm.tm_year = ((PHYSFS_uint32) fs.fdateLastWrite.year) + 80;
    tm.tm_wday = -1 /*st_localtz.wDayOfWeek*/;
    tm.tm_yday = -1;
    tm.tm_isdst = -1;

    /* Convert to a format PhysicsFS can grok... */
    retval = (PHYSFS_sint64) mktime(&tm);
    BAIL_IF_MACRO(retval == -1, strerror(errno), -1);
    return(retval);
} /* __PHYSFS_platformGetLastModTime */


/* Much like my college days, try to sleep for 10 milliseconds at a time... */
void __PHYSFS_platformTimeslice(void)
{
    DosSleep(10);
} /* __PHYSFS_platformTimeslice(void) */


PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    PTIB ptib;
    PPIB ppib;

    /*
     * Allegedly, this API never fails, but we'll punt and return a
     *  default value (zero might as well do) if it does.
     */
    BAIL_IF_MACRO(os2err(DosGetInfoBlocks(&ptib, &ppib)) != NO_ERROR, 0, 0);
    return((PHYSFS_uint64) ptib->tib_ordinal);
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    HMTX hmtx = NULLHANDLE;
    os2err(DosCreateMutexSem(NULL, &hmtx, 0, 0));
    return((void *) hmtx);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    DosCloseMutexSem((HMTX) mutex);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    /* Do _NOT_ call os2err() (which sets the physfs error msg) in here! */
    return(DosRequestMutexSem((HMTX) mutex, SEM_INDEFINITE_WAIT) == NO_ERROR);
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    DosReleaseMutexSem((HMTX) mutex);
} /* __PHYSFS_platformReleaseMutex */

#endif  /* defined OS2 */

/* end of os2.c ... */

