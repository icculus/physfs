/*
 * Win32 support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon, and made sane by Gregory S. Read.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef WIN32

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if (defined _MSC_VER)
    #define alloca(x) _alloca(x)
#elif (defined MINGW)  /* scary...hopefully this is okay. */
    #define alloca(x) __builtin_alloca(x) 
#endif

#define LOWORDER_UINT64(pos) (PHYSFS_uint32) \
    (pos & 0x00000000FFFFFFFF)
#define HIGHORDER_UINT64(pos) (PHYSFS_uint32) \
    (((pos & 0xFFFFFFFF00000000) >> 32) & 0x00000000FFFFFFFF)

/* GetUserProfileDirectory() is only available on >= NT4 (no 9x/ME systems!) */
typedef BOOL (STDMETHODCALLTYPE FAR * LPFNGETUSERPROFILEDIR) (
      HANDLE hToken,
      LPTSTR lpProfileDir,
      LPDWORD lpcchSize);

/* GetFileAttributesEx() is only available on >= Win98 or WinNT4 ... */
typedef BOOL (STDMETHODCALLTYPE FAR * LPFNGETFILEATTRIBUTESEX) (
      LPCTSTR lpFileName,
      GET_FILEEX_INFO_LEVELS fInfoLevelId,
      LPVOID lpFileInformation);

typedef struct
{
    HANDLE handle;
    int readonly;
} win32file;

const char *__PHYSFS_platformDirSeparator = "\\";
static LPFNGETFILEATTRIBUTESEX pGetFileAttributesEx = NULL;
static HANDLE libKernel32 = NULL;
static char *userDir = NULL;

/*
 * Users without the platform SDK don't have this defined.  The original docs
 *  for SetFilePointer() just said to compare with 0xFFFFFFFF, so this should
 *  work as desired
 */
#define PHYSFS_INVALID_SET_FILE_POINTER  0xFFFFFFFF

/* just in case... */
#define PHYSFS_INVALID_FILE_ATTRIBUTES   0xFFFFFFFF



/*
 * Figure out what the last failing Win32 API call was, and
 *  generate a human-readable string for the error message.
 *
 * The return value is a static buffer that is overwritten with
 *  each call to this function.
 */
static const char *win32strerror(void)
{
    static TCHAR msgbuf[255];
    TCHAR *ptr = msgbuf;

    FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
        msgbuf,
        sizeof (msgbuf) / sizeof (TCHAR),
        NULL 
    );

        /* chop off newlines. */
    for (ptr = msgbuf; *ptr; ptr++)
    {
        if ((*ptr == '\n') || (*ptr == '\r'))
        {
            *ptr = ' ';
            break;
        } /* if */
    } /* for */

    return((const char *) msgbuf);
} /* win32strerror */


static char *getExePath(const char *argv0)
{
    DWORD buflen;
    int success = 0;
    char *ptr = NULL;
    char *retval = (char *) allocator.Malloc(sizeof (TCHAR) * (MAX_PATH + 1));

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    retval[0] = '\0';
    buflen = GetModuleFileName(NULL, retval, MAX_PATH + 1);
    if (buflen <= 0)
        __PHYSFS_setError(win32strerror());
    else
    {
        retval[buflen] = '\0';  /* does API always null-terminate this? */

            /* make sure the string was not truncated. */
        if (__PHYSFS_platformStricmp(&retval[buflen - 4], ".exe") != 0)
            __PHYSFS_setError(ERR_GETMODFN_TRUNC);
        else
        {
            ptr = strrchr(retval, '\\');
            if (ptr == NULL)
                __PHYSFS_setError(ERR_GETMODFN_NO_DIR);
            else
            {
                *(ptr + 1) = '\0';  /* chop off filename. */
                success = 1;
            } /* else */
        } /* else */
    } /* else */

    /* if any part of the previous approach failed, try SearchPath()... */

    if (!success)
    {
        if (argv0 == NULL)
            __PHYSFS_setError(ERR_ARGV0_IS_NULL);
        else
        {
            buflen = SearchPath(NULL, argv0, NULL, MAX_PATH+1, retval, &ptr);
            if (buflen == 0)
                __PHYSFS_setError(win32strerror());
            else if (buflen > MAX_PATH)
                __PHYSFS_setError(ERR_SEARCHPATH_TRUNC);
            else
                success = 1;
        } /* else */
    } /* if */

    if (!success)
    {
        allocator.Free(retval);
        return(NULL);  /* physfs error message will be set, above. */
    } /* if */

    /* free up the bytes we didn't actually use. */
    ptr = (char *) allocator.Realloc(retval, strlen(retval) + 1);
    if (ptr != NULL)
        retval = ptr;

    return(retval);   /* w00t. */
} /* getExePath */


/*
 * Try to make use of GetUserProfileDirectory(), which isn't available on
 *  some common variants of Win32. If we can't use this, we just punt and
 *  use the physfs base dir for the user dir, too.
 *
 * On success, module-scope variable (userDir) will have a pointer to
 *  a malloc()'d string of the user's profile dir, and a non-zero value is
 *  returned. If we can't determine the profile dir, (userDir) will
 *  be NULL, and zero is returned.
 */
static int determineUserDir(void)
{
    DWORD psize = 0;
    char dummy[1];
    BOOL rc = 0;
    HANDLE processHandle;            /* Current process handle */
    HANDLE accessToken = NULL;       /* Security handle to process */
    LPFNGETUSERPROFILEDIR GetUserProfileDirectory;
    HMODULE lib;

    assert(userDir == NULL);

    /*
     * GetUserProfileDirectory() is only available on NT 4.0 and later.
     *  This means Win95/98/ME (and CE?) users have to do without, so for
     *  them, we'll default to the base directory when we can't get the
     *  function pointer.
     */

    lib = LoadLibrary("userenv.dll");
    if (lib)
    {
        /* !!! FIXME: Handle Unicode? */
        GetUserProfileDirectory = (LPFNGETUSERPROFILEDIR)
                              GetProcAddress(lib, "GetUserProfileDirectoryA");
        if (GetUserProfileDirectory)
        {
            processHandle = GetCurrentProcess();
            if (OpenProcessToken(processHandle, TOKEN_QUERY, &accessToken))
            {
                /*
                 * Should fail. Will write the size of the profile path in
                 *  psize. Also note that the second parameter can't be
                 *  NULL or the function fails.
                 */
                rc = GetUserProfileDirectory(accessToken, dummy, &psize);
                assert(!rc);  /* success?! */

                /* Allocate memory for the profile directory */
                userDir = (char *) allocator.Malloc(psize);
                if (userDir != NULL)
                {
                    if (!GetUserProfileDirectory(accessToken, userDir, &psize))
                    {
                        allocator.Free(userDir);
                        userDir = NULL;
                    } /* if */
                } /* else */
            } /* if */

            CloseHandle(accessToken);
        } /* if */

        FreeLibrary(lib);
    } /* if */

    if (userDir == NULL)  /* couldn't get profile for some reason. */
    {
        /* Might just be a non-NT system; resort to the basedir. */
        userDir = getExePath(NULL);
        BAIL_IF_MACRO(userDir == NULL, NULL, 0); /* STILL failed?! */
    } /* if */

    return(1);  /* We made it: hit the showers. */
} /* determineUserDir */


static BOOL mediaInDrive(const char *drive)
{
    UINT oldErrorMode;
    DWORD tmp;
    BOOL retval;

    /* Prevent windows warning message appearing when checking media size */
    oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
    
    /* If this function succeeds, there's media in the drive */
    retval = GetVolumeInformation(drive, NULL, 0, NULL, NULL, &tmp, NULL, 0);

    /* Revert back to old windows error handler */
    SetErrorMode(oldErrorMode);

    return(retval);
} /* mediaInDrive */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    char drive_str[4] = "x:\\";
    char ch;
    for (ch = 'A'; ch <= 'Z'; ch++)
    {
        drive_str[0] = ch;
        if (GetDriveType(drive_str) == DRIVE_CDROM && mediaInDrive(drive_str))
            cb(data, drive_str);
    } /* for */
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    if ((argv0 != NULL) && (strchr(argv0, '\\') != NULL))
        return(NULL); /* default behaviour can handle this. */

    return(getExePath(argv0));
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformGetUserName(void)
{
    DWORD bufsize = 0;
    LPTSTR retval = NULL;

    if (GetUserName(NULL, &bufsize) == 0)  /* This SHOULD fail. */
    {
        retval = (LPTSTR) allocator.Malloc(bufsize);
        BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
        if (GetUserName(retval, &bufsize) == 0)  /* ?! */
        {
            __PHYSFS_setError(win32strerror());
            allocator.Free(retval);
            retval = NULL;
        } /* if */
    } /* if */

    return((char *) retval);
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    char *retval = (char *) allocator.Malloc(strlen(userDir) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, userDir); /* calculated at init time. */
    return(retval);
} /* __PHYSFS_platformGetUserDir */


PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    return((PHYSFS_uint64) GetCurrentThreadId());
} /* __PHYSFS_platformGetThreadID */


/* ...make this Cygwin AND Visual C friendly... */
int __PHYSFS_platformStricmp(const char *x, const char *y)
{
#if (defined _MSC_VER)
    return(stricmp(x, y));
#else
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
#endif
} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformStrnicmp(const char *x, const char *y, PHYSFS_uint32 len)
{
#if (defined _MSC_VER)
    return(strnicmp(x, y, (int) len));
#else
    int ux, uy;

    if (!len)
        return(0);

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
        len--;
    } while ((ux) && (uy) && (len));

    return(0);
#endif
} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformExists(const char *fname)
{
    BAIL_IF_MACRO
    (
        GetFileAttributes(fname) == PHYSFS_INVALID_FILE_ATTRIBUTES,
        win32strerror(), 0
    );
    return(1);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
    return(0);  /* no symlinks on win32. */
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    return((GetFileAttributes(fname) & FILE_ATTRIBUTE_DIRECTORY) != 0);
} /* __PHYSFS_platformIsDirectory */


char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append)
{
    int len = ((prepend) ? strlen(prepend) : 0) +
              ((append) ? strlen(append) : 0) +
              strlen(dirName) + 1;
    char *retval = (char *) allocator.Malloc(len);
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


/* Much like my college days, try to sleep for 10 milliseconds at a time... */
void __PHYSFS_platformTimeslice(void)
{
    Sleep(10);
} /* __PHYSFS_platformTimeslice */


void __PHYSFS_platformEnumerateFiles(const char *dirname,
                                     int omitSymLinks,
                                     PHYSFS_StringCallback callback,
                                     void *callbackdata)
{
    HANDLE dir;
    WIN32_FIND_DATA ent;
    size_t len = strlen(dirname);
    char *SearchPath;

    /* Allocate a new string for path, maybe '\\', "*", and NULL terminator */
    SearchPath = (char *) alloca(len + 3);
    if (SearchPath == NULL)
        return;

    /* Copy current dirname */
    strcpy(SearchPath, dirname);

    /* if there's no '\\' at the end of the path, stick one in there. */
    if (SearchPath[len - 1] != '\\')
    {
        SearchPath[len++] = '\\';
        SearchPath[len] = '\0';
    } /* if */

    /* Append the "*" to the end of the string */
    strcat(SearchPath, "*");

    dir = FindFirstFile(SearchPath, &ent);
    if (dir == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (strcmp(ent.cFileName, ".") == 0)
            continue;

        if (strcmp(ent.cFileName, "..") == 0)
            continue;

        callback(callbackdata, ent.cFileName);
    } while (FindNextFile(dir, &ent) != 0);

    FindClose(dir);
} /* __PHYSFS_platformEnumerateFiles */


char *__PHYSFS_platformCurrentDir(void)
{
    LPTSTR retval;
    DWORD buflen = 0;

    buflen = GetCurrentDirectory(buflen, NULL);
    retval = (LPTSTR) allocator.Malloc(sizeof (TCHAR) * (buflen + 2));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    GetCurrentDirectory(buflen, retval);

    if (retval[buflen - 2] != '\\')
        strcat(retval, "\\");

    return((char *) retval);
} /* __PHYSFS_platformCurrentDir */


/* this could probably use a cleanup. */
char *__PHYSFS_platformRealPath(const char *path)
{
    char *retval = NULL;
    char *p = NULL;

    BAIL_IF_MACRO(path == NULL, ERR_INVALID_ARGUMENT, NULL);
    BAIL_IF_MACRO(*path == '\0', ERR_INVALID_ARGUMENT, NULL);

    retval = (char *) allocator.Malloc(MAX_PATH);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

        /*
         * If in \\server\path format, it's already an absolute path.
         *  We'll need to check for "." and ".." dirs, though, just in case.
         */
    if ((path[0] == '\\') && (path[1] == '\\'))
        strcpy(retval, path);

    else
    {
        char *currentDir = __PHYSFS_platformCurrentDir();
        if (currentDir == NULL)
        {
            allocator.Free(retval);
            BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
        } /* if */

        if (path[1] == ':')   /* drive letter specified? */
        {
            /*
             * Apparently, "D:mypath" is the same as "D:\\mypath" if
             *  D: is not the current drive. However, if D: is the
             *  current drive, then "D:mypath" is a relative path. Ugh.
             */
            if (path[2] == '\\')  /* maybe an absolute path? */
                strcpy(retval, path);
            else  /* definitely an absolute path. */
            {
                if (path[0] == currentDir[0]) /* current drive; relative. */
                {
                    strcpy(retval, currentDir);
                    strcat(retval, path + 2);
                } /* if */

                else  /* not current drive; absolute. */
                {
                    retval[0] = path[0];
                    retval[1] = ':';
                    retval[2] = '\\';
                    strcpy(retval + 3, path + 2);
                } /* else */
            } /* else */
        } /* if */

        else  /* no drive letter specified. */
        {
            if (path[0] == '\\')  /* absolute path. */
            {
                retval[0] = currentDir[0];
                retval[1] = ':';
                strcpy(retval + 2, path);
            } /* if */
            else
            {
                strcpy(retval, currentDir);
                strcat(retval, path);
            } /* else */
        } /* else */

        allocator.Free(currentDir);
    } /* else */

    /* (whew.) Ok, now take out "." and ".." path entries... */

    p = retval;
    while ( (p = strstr(p, "\\.")) != NULL)
    {
            /* it's a "." entry that doesn't end the string. */
        if (p[2] == '\\')
            memmove(p + 1, p + 3, strlen(p + 3) + 1);

            /* it's a "." entry that ends the string. */
        else if (p[2] == '\0')
            p[0] = '\0';

            /* it's a ".." entry. */
        else if (p[2] == '.')
        {
            char *prevEntry = p - 1;
            while ((prevEntry != retval) && (*prevEntry != '\\'))
                prevEntry--;

            if (prevEntry == retval)  /* make it look like a "." entry. */
                memmove(p + 1, p + 2, strlen(p + 2) + 1);
            else
            {
                if (p[3] != '\0') /* doesn't end string. */
                    *prevEntry = '\0';
                else /* ends string. */
                    memmove(prevEntry + 1, p + 4, strlen(p + 4) + 1);

                p = prevEntry;
            } /* else */
        } /* else if */

        else
        {
            p++;  /* look past current char. */
        } /* else */
    } /* while */

        /* shrink the retval's memory block if possible... */
    p = (char *) allocator.Realloc(retval, strlen(retval) + 1);
    if (p != NULL)
        retval = p;

    return(retval);
} /* __PHYSFS_platformRealPath */


int __PHYSFS_platformMkDir(const char *path)
{
    DWORD rc = CreateDirectory(path, NULL);
    BAIL_IF_MACRO(rc == 0, win32strerror(), 0);
    return(1);
} /* __PHYSFS_platformMkDir */


/*
 * Get OS info and save the important parts.
 *
 * Returns non-zero if successful, otherwise it returns zero on failure.
 */
static int getOSInfo(void)
{
#if 0  /* we don't actually use this at the moment, but may in the future. */
    OSVERSIONINFO OSVersionInfo;     /* Information about the OS */
    OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVersionInfo);
    BAIL_IF_MACRO(!GetVersionEx(&OSVersionInfo), win32strerror(), 0);

    /* Set to TRUE if we are runnign a WinNT based OS 4.0 or greater */
    runningNT = ((OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) &&
                 (OSVersionInfo.dwMajorVersion >= 4));
#endif

    return(1);
} /* getOSInfo */


/*
 * Some things we want/need are in external DLLs that may or may not be
 *  available, based on the operating system, etc. This function loads those
 *  libraries and hunts down the needed pointers.
 *
 * Libraries that are one-shot deals, or better loaded as needed, are loaded
 *  elsewhere (see determineUserDir()).
 *
 * Returns zero if a needed library couldn't load, non-zero if we have enough
 *  to go on (which means some useful but non-crucial libraries may _NOT_ be
 *  loaded; check the related module-scope variables).
 */
static int loadLibraries(void)
{
    /* If this get unwieldy, make it table driven. */

    int allNeededLibrariesLoaded = 1;  /* flip to zero as needed. */

    libKernel32 = LoadLibrary("kernel32.dll");
    if (libKernel32)
    {
        pGetFileAttributesEx = (LPFNGETFILEATTRIBUTESEX)
                          GetProcAddress(libKernel32, "GetFileAttributesExA");
    } /* if */

    /* add other DLLs here... */


    /* see if there's any reason to keep kernel32.dll around... */
    if (libKernel32)
    {
        if ((pGetFileAttributesEx == NULL) /* && (somethingElse == NULL) */ )
        {
            FreeLibrary(libKernel32);
            libKernel32 = NULL;
        } /* if */
    } /* if */

    return(allNeededLibrariesLoaded);
} /* loadLibraries */


int __PHYSFS_platformInit(void)
{
    BAIL_IF_MACRO(!getOSInfo(), NULL, 0);
    BAIL_IF_MACRO(!loadLibraries(), NULL, 0);
    BAIL_IF_MACRO(!determineUserDir(), NULL, 0);

    return(1);  /* It's all good */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    if (userDir != NULL)
    {
        allocator.Free(userDir);
        userDir = NULL;
    } /* if */

    if (libKernel32)
    {
        FreeLibrary(libKernel32);
        libKernel32 = NULL;
    } /* if */

    return(1); /* It's all good */
} /* __PHYSFS_platformDeinit */


static void *doOpen(const char *fname, DWORD mode, DWORD creation, int rdonly)
{
    HANDLE fileHandle;
    win32file *retval;

    fileHandle = CreateFile(fname, mode, FILE_SHARE_READ, NULL,
                            creation, FILE_ATTRIBUTE_NORMAL, NULL);

    BAIL_IF_MACRO
    (
        fileHandle == INVALID_HANDLE_VALUE,
        win32strerror(), NULL
    );

    retval = (win32file *) allocator.Malloc(sizeof (win32file));
    if (retval == NULL)
    {
        CloseHandle(fileHandle);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    retval->readonly = rdonly;
    retval->handle = fileHandle;
    return(retval);
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return(doOpen(filename, GENERIC_READ, OPEN_EXISTING, 1));
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return(doOpen(filename, GENERIC_WRITE, CREATE_ALWAYS, 0));
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    void *retval = doOpen(filename, GENERIC_WRITE, OPEN_ALWAYS, 0);
    if (retval != NULL)
    {
        HANDLE h = ((win32file *) retval)->handle;
        DWORD rc = SetFilePointer(h, 0, NULL, FILE_END);
        if (rc == PHYSFS_INVALID_SET_FILE_POINTER)
        {
            const char *err = win32strerror();
            CloseHandle(h);
            allocator.Free(retval);
            BAIL_MACRO(err, NULL);
        } /* if */
    } /* if */

    return(retval);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HANDLE Handle = ((win32file *) opaque)->handle;
    DWORD CountOfBytesRead;
    PHYSFS_sint64 retval;

    /* Read data from the file */
    /* !!! FIXME: uint32 might be a greater # than DWORD */
    if(!ReadFile(Handle, buffer, count * size, &CountOfBytesRead, NULL))
    {
        BAIL_MACRO(win32strerror(), -1);
    } /* if */
    else
    {
        /* Return the number of "objects" read. */
        /* !!! FIXME: What if not the right amount of bytes was read to make an object? */
        retval = CountOfBytesRead / size;
    } /* else */

    return(retval);
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HANDLE Handle = ((win32file *) opaque)->handle;
    DWORD CountOfBytesWritten;
    PHYSFS_sint64 retval;

    /* Read data from the file */
    /* !!! FIXME: uint32 might be a greater # than DWORD */
    if(!WriteFile(Handle, buffer, count * size, &CountOfBytesWritten, NULL))
    {
        BAIL_MACRO(win32strerror(), -1);
    } /* if */
    else
    {
        /* Return the number of "objects" read. */
        /* !!! FIXME: What if not the right number of bytes was written? */
        retval = CountOfBytesWritten / size;
    } /* else */

    return(retval);
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    HANDLE Handle = ((win32file *) opaque)->handle;
    DWORD HighOrderPos;
    DWORD *pHighOrderPos;
    DWORD rc;

    /* Get the high order 32-bits of the position */
    HighOrderPos = HIGHORDER_UINT64(pos);

    /*
     * MSDN: "If you do not need the high-order 32 bits, this
     *         pointer must be set to NULL."
     */
    pHighOrderPos = (HighOrderPos) ? &HighOrderPos : NULL;

    /*
     * !!! FIXME: MSDN: "Windows Me/98/95:  If the pointer
     * !!! FIXME:  lpDistanceToMoveHigh is not NULL, then it must
     * !!! FIXME:  point to either 0, INVALID_SET_FILE_POINTER, or
     * !!! FIXME:  the sign extension of the value of lDistanceToMove.
     * !!! FIXME:  Any other value will be rejected."
     */

    /* Move pointer "pos" count from start of file */
    rc = SetFilePointer(Handle, LOWORDER_UINT64(pos),
                        pHighOrderPos, FILE_BEGIN);

    if ( (rc == PHYSFS_INVALID_SET_FILE_POINTER) &&
         (GetLastError() != NO_ERROR) )
    {
        BAIL_MACRO(win32strerror(), 0);
    } /* if */
    
    return(1);  /* No error occured */
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    HANDLE Handle = ((win32file *) opaque)->handle;
    DWORD HighPos = 0;
    DWORD LowPos;
    PHYSFS_sint64 retval;

    /* Get current position */
    LowPos = SetFilePointer(Handle, 0, &HighPos, FILE_CURRENT);
    if ( (LowPos == PHYSFS_INVALID_SET_FILE_POINTER) &&
         (GetLastError() != NO_ERROR) )
    {
        BAIL_MACRO(win32strerror(), 0);
    } /* if */
    else
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = (((PHYSFS_uint64) HighPos) << 32) | LowPos;
        assert(retval >= 0);
    } /* else */

    return(retval);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    HANDLE Handle = ((win32file *) opaque)->handle;
    DWORD SizeHigh;
    DWORD SizeLow;
    PHYSFS_sint64 retval;

    SizeLow = GetFileSize(Handle, &SizeHigh);
    if ( (SizeLow == PHYSFS_INVALID_SET_FILE_POINTER) &&
         (GetLastError() != NO_ERROR) )
    {
        BAIL_MACRO(win32strerror(), -1);
    } /* if */
    else
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = (((PHYSFS_uint64) SizeHigh) << 32) | SizeLow;
        assert(retval >= 0);
    } /* else */

    return(retval);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformEOF(void *opaque)
{
    PHYSFS_sint64 FilePosition;
    int retval = 0;

    /* Get the current position in the file */
    if ((FilePosition = __PHYSFS_platformTell(opaque)) != 0)
    {
        /* Non-zero if EOF is equal to the file length */
        retval = FilePosition == __PHYSFS_platformFileLength(opaque);
    } /* if */

    return(retval);
} /* __PHYSFS_platformEOF */


int __PHYSFS_platformFlush(void *opaque)
{
    win32file *fh = ((win32file *) opaque);
    if (!fh->readonly)
        BAIL_IF_MACRO(!FlushFileBuffers(fh->handle), win32strerror(), 0);

    return(1);
} /* __PHYSFS_platformFlush */


int __PHYSFS_platformClose(void *opaque)
{
    HANDLE Handle = ((win32file *) opaque)->handle;
    BAIL_IF_MACRO(!CloseHandle(Handle), win32strerror(), 0);
    allocator.Free(opaque);
    return(1);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    /* If filename is a folder */
    if (GetFileAttributes(path) == FILE_ATTRIBUTE_DIRECTORY)
    {
        BAIL_IF_MACRO(!RemoveDirectory(path), win32strerror(), 0);
    } /* if */
    else
    {
        BAIL_IF_MACRO(!DeleteFile(path), win32strerror(), 0);
    } /* else */

    return(1);  /* if you got here, it worked. */
} /* __PHYSFS_platformDelete */


void *__PHYSFS_platformCreateMutex(void)
{
    return((void *) CreateMutex(NULL, FALSE, NULL));
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    CloseHandle((HANDLE) mutex);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    return(WaitForSingleObject((HANDLE) mutex, INFINITE) != WAIT_FAILED);
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    ReleaseMutex((HANDLE) mutex);
} /* __PHYSFS_platformReleaseMutex */


static PHYSFS_sint64 FileTimeToPhysfsTime(const FILETIME *ft)
{
    SYSTEMTIME st_utc;
    SYSTEMTIME st_localtz;
    TIME_ZONE_INFORMATION tzi;
    DWORD tzid;
    PHYSFS_sint64 retval;
    struct tm tm;

    BAIL_IF_MACRO(!FileTimeToSystemTime(ft, &st_utc), win32strerror(), -1);
    tzid = GetTimeZoneInformation(&tzi);
    BAIL_IF_MACRO(tzid == TIME_ZONE_ID_INVALID, win32strerror(), -1);

        /* (This API is unsupported and fails on non-NT systems. */
    if (!SystemTimeToTzSpecificLocalTime(&tzi, &st_utc, &st_localtz))
    {
        /* do it by hand. Grumble... */
        ULARGE_INTEGER ui64;
        FILETIME new_ft;
        ui64.LowPart = ft->dwLowDateTime;
        ui64.HighPart = ft->dwHighDateTime;

        if (tzid == TIME_ZONE_ID_STANDARD)
            tzi.Bias += tzi.StandardBias;
        else if (tzid == TIME_ZONE_ID_DAYLIGHT)
            tzi.Bias += tzi.DaylightBias;

        /* convert from minutes to 100-nanosecond increments... */
        #if 0 /* For compilers that puke on 64-bit math. */
            /* goddamn this is inefficient... */
            while (tzi.Bias > 0)
            {
                DWORD tmp = ui64.LowPart - 60000000;
                if ((ui64.LowPart < tmp) && (tmp > 60000000))
                    ui64.HighPart--;
                ui64.LowPart = tmp;
                tzi.Bias--;
            } /* while */

            while (tzi.Bias < 0)
            {
                DWORD tmp = ui64.LowPart + 60000000;
                if ((ui64.LowPart > tmp) && (tmp < 60000000))
                    ui64.HighPart++;
                ui64.LowPart = tmp;
                tzi.Bias++;
            } /* while */
        #else
            ui64.QuadPart -= (((LONGLONG) tzi.Bias) * (600000000));
        #endif

        /* Move it back into a FILETIME structure... */
        new_ft.dwLowDateTime = ui64.LowPart;
        new_ft.dwHighDateTime = ui64.HighPart;

        /* Convert to something human-readable... */
        if (!FileTimeToSystemTime(&new_ft, &st_localtz))
            BAIL_MACRO(win32strerror(), -1);
    } /* if */

    /* Convert to a format that mktime() can grok... */
    tm.tm_sec = st_localtz.wSecond;
    tm.tm_min = st_localtz.wMinute;
    tm.tm_hour = st_localtz.wHour;
    tm.tm_mday = st_localtz.wDay;
    tm.tm_mon = st_localtz.wMonth - 1;
    tm.tm_year = st_localtz.wYear - 1900;
    tm.tm_wday = -1 /*st_localtz.wDayOfWeek*/;
    tm.tm_yday = -1;
    tm.tm_isdst = -1;

    /* Convert to a format PhysicsFS can grok... */
    retval = (PHYSFS_sint64) mktime(&tm);
    BAIL_IF_MACRO(retval == -1, strerror(errno), -1);
    return(retval);
} /* FileTimeToPhysfsTime */


PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname)
{
    PHYSFS_sint64 retval = -1;
    WIN32_FILE_ATTRIBUTE_DATA attrData;
    memset(&attrData, '\0', sizeof (attrData));

    /* GetFileAttributesEx didn't show up until Win98 and NT4. */
    if (pGetFileAttributesEx != NULL)
    {
        if (pGetFileAttributesEx(fname, GetFileExInfoStandard, &attrData))
        {
            /* 0 return value indicates an error or not supported */
            if ( (attrData.ftLastWriteTime.dwHighDateTime != 0) ||
                 (attrData.ftLastWriteTime.dwLowDateTime != 0) )
            {
                retval = FileTimeToPhysfsTime(&attrData.ftLastWriteTime);
            } /* if */
        } /* if */
    } /* if */

    /* GetFileTime() has been in the Win32 API since the start. */
    if (retval == -1)  /* try a fallback... */
    {
        FILETIME ft;
        BOOL rc;
        const char *err;
        win32file *f = (win32file *) __PHYSFS_platformOpenRead(fname);
        BAIL_IF_MACRO(f == NULL, NULL, -1)
        rc = GetFileTime(f->handle, NULL, NULL, &ft);
        err = win32strerror();
        CloseHandle(f->handle);
        allocator.Free(f);
        BAIL_IF_MACRO(!rc, err, -1);
        retval = FileTimeToPhysfsTime(&ft);
    } /* if */

    return(retval);
} /* __PHYSFS_platformGetLastModTime */


/* !!! FIXME: Don't use C runtime for allocators? */
int __PHYSFS_platformAllocatorInit(void)
{
    return(1);  /* always succeeds. */
} /* __PHYSFS_platformAllocatorInit */


void __PHYSFS_platformAllocatorDeinit(void)
{
    /* no-op */
} /* __PHYSFS_platformAllocatorInit */


void *__PHYSFS_platformAllocatorMalloc(size_t s)
{
    #undef malloc
    return(malloc(s));
} /* __PHYSFS_platformMalloc */


void *__PHYSFS_platformAllocatorRealloc(void *ptr, size_t s)
{
    #undef realloc
    return(realloc(ptr, s));
} /* __PHYSFS_platformRealloc */


void __PHYSFS_platformAllocatorFree(void *ptr)
{
    #undef free
    free(ptr);
} /* __PHYSFS_platformAllocatorFree */

#endif

/* end of win32.c ... */

