/*
 * Windows support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon, and made sane by Gregory S. Read.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_WINDOWS

/* Forcibly disable UNICODE macro, since we manage this ourselves. */
#ifdef UNICODE
#undef UNICODE
#endif

#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <userenv.h>
#include <shlobj.h>
#include <dbt.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "physfs_internal.h"

#define LOWORDER_UINT64(pos) ((PHYSFS_uint32) (pos & 0xFFFFFFFF))
#define HIGHORDER_UINT64(pos) ((PHYSFS_uint32) ((pos >> 32) & 0xFFFFFFFF))

/*
 * Users without the platform SDK don't have this defined.  The original docs
 *  for SetFilePointer() just said to compare with 0xFFFFFFFF, so this should
 *  work as desired.
 */
#define PHYSFS_INVALID_SET_FILE_POINTER  0xFFFFFFFF

/* just in case... */
#define PHYSFS_INVALID_FILE_ATTRIBUTES   0xFFFFFFFF

/* Not defined before the Vista SDK. */
#define PHYSFS_IO_REPARSE_TAG_SYMLINK    0xA000000C


#define UTF8_TO_UNICODE_STACK_MACRO(w_assignto, str) { \
    if (str == NULL) \
        w_assignto = NULL; \
    else { \
        const PHYSFS_uint64 len = (PHYSFS_uint64) ((strlen(str) + 1) * 2); \
        w_assignto = (WCHAR *) __PHYSFS_smallAlloc(len); \
        if (w_assignto != NULL) \
            PHYSFS_utf8ToUtf16(str, (PHYSFS_uint16 *) w_assignto, len); \
    } \
} \

/* Note this counts WCHARs, not codepoints! */
static PHYSFS_uint64 wStrLen(const WCHAR *wstr)
{
    PHYSFS_uint64 len = 0;
    while (*(wstr++))
        len++;
    return len;
} /* wStrLen */

static char *unicodeToUtf8Heap(const WCHAR *w_str)
{
    char *retval = NULL;
    if (w_str != NULL)
    {
        void *ptr = NULL;
        const PHYSFS_uint64 len = (wStrLen(w_str) * 4) + 1;
        retval = allocator.Malloc(len);
        BAIL_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
        PHYSFS_utf8FromUtf16((const PHYSFS_uint16 *) w_str, retval, len);
        ptr = allocator.Realloc(retval, strlen(retval) + 1); /* shrink. */
        if (ptr != NULL)
            retval = (char *) ptr;
    } /* if */
    return retval;
} /* unicodeToUtf8Heap */

/* !!! FIXME: do we really need readonly? If not, do we need this struct? */
typedef struct
{
    HANDLE handle;
    int readonly;
} WinApiFile;

static HANDLE detectCDThreadHandle = NULL;
static HWND detectCDHwnd = 0;
static volatile int initialDiscDetectionComplete = 0;
static volatile DWORD drivesWithMediaBitmap = 0;


static PHYSFS_ErrorCode errcodeFromWinApiError(const DWORD err)
{
    /*
     * win32 error codes are sort of a tricky thing; Microsoft intentionally
     *  doesn't list which ones a given API might trigger, there are several
     *  with overlapping and unclear meanings...and there's 16 thousand of
     *  them in Windows 7. It looks like the ones we care about are in the
     *  first 500, but I can't say this list is perfect; we might miss
     *  important values or misinterpret others.
     *
     * Don't treat this list as anything other than a work in progress.
     */
    switch (err)
    {
        case ERROR_SUCCESS: return PHYSFS_ERR_OK;
        case ERROR_ACCESS_DENIED: return PHYSFS_ERR_PERMISSION;
        case ERROR_NETWORK_ACCESS_DENIED: return PHYSFS_ERR_PERMISSION;
        case ERROR_NOT_READY: return PHYSFS_ERR_IO;
        case ERROR_CRC: return PHYSFS_ERR_IO;
        case ERROR_SEEK: return PHYSFS_ERR_IO;
        case ERROR_SECTOR_NOT_FOUND: return PHYSFS_ERR_IO;
        case ERROR_NOT_DOS_DISK: return PHYSFS_ERR_IO;
        case ERROR_WRITE_FAULT: return PHYSFS_ERR_IO;
        case ERROR_READ_FAULT: return PHYSFS_ERR_IO;
        case ERROR_DEV_NOT_EXIST: return PHYSFS_ERR_IO;
        /* !!! FIXME: ?? case ELOOP: return PHYSFS_ERR_SYMLINK_LOOP; */
        case ERROR_BUFFER_OVERFLOW: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_INVALID_NAME: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_BAD_PATHNAME: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_DIRECTORY: return PHYSFS_ERR_BAD_FILENAME;
        case ERROR_FILE_NOT_FOUND: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_PATH_NOT_FOUND: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_DELETE_PENDING: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_INVALID_DRIVE: return PHYSFS_ERR_NOT_FOUND;
        case ERROR_HANDLE_DISK_FULL: return PHYSFS_ERR_NO_SPACE;
        case ERROR_DISK_FULL: return PHYSFS_ERR_NO_SPACE;
        /* !!! FIXME: ?? case ENOTDIR: return PHYSFS_ERR_NOT_FOUND; */
        /* !!! FIXME: ?? case EISDIR: return PHYSFS_ERR_NOT_A_FILE; */
        case ERROR_WRITE_PROTECT: return PHYSFS_ERR_READ_ONLY;
        case ERROR_LOCK_VIOLATION: return PHYSFS_ERR_BUSY;
        case ERROR_SHARING_VIOLATION: return PHYSFS_ERR_BUSY;
        case ERROR_CURRENT_DIRECTORY: return PHYSFS_ERR_BUSY;
        case ERROR_DRIVE_LOCKED: return PHYSFS_ERR_BUSY;
        case ERROR_PATH_BUSY: return PHYSFS_ERR_BUSY;
        case ERROR_BUSY: return PHYSFS_ERR_BUSY;
        case ERROR_NOT_ENOUGH_MEMORY: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ERROR_OUTOFMEMORY: return PHYSFS_ERR_OUT_OF_MEMORY;
        case ERROR_DIR_NOT_EMPTY: return PHYSFS_ERR_DIR_NOT_EMPTY;
        default: return PHYSFS_ERR_OS_ERROR;
    } /* switch */
} /* errcodeFromWinApiError */

static inline PHYSFS_ErrorCode errcodeFromWinApi(void)
{
    return errcodeFromWinApiError(GetLastError());
} /* errcodeFromWinApi */


typedef BOOL (WINAPI *fnSTEM)(DWORD, LPDWORD b);

static DWORD pollDiscDrives(void)
{
    /* Try to use SetThreadErrorMode(), which showed up in Windows 7. */
    HANDLE lib = LoadLibraryA("kernel32.dll");
    fnSTEM stem = NULL;
    char drive[4] = { 'x', ':', '\\', '\0' };
    DWORD oldErrorMode = 0;
    DWORD drives = 0;
    DWORD i;

    if (lib)
        stem = (fnSTEM) GetProcAddress(lib, "SetThreadErrorMode");

    if (stem)
        stem(SEM_FAILCRITICALERRORS, &oldErrorMode);
    else
        oldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
    
    /* Do detection. This may block if a disc is spinning up. */
    for (i = 'A'; i <= 'Z'; i++)
    {
        DWORD tmp = 0;
        drive[0] = (char) i;
        if (GetDriveTypeA(drive) != DRIVE_CDROM)
            continue;

        /* If this function succeeds, there's media in the drive */
        if (GetVolumeInformationA(drive, NULL, 0, NULL, NULL, &tmp, NULL, 0))
            drives |= (1 << (i - 'A'));
    } /* for */

    if (stem)
        stem(oldErrorMode, NULL);
    else
        SetErrorMode(oldErrorMode);

    if (lib)
        FreeLibrary(lib);

    return drives;
} /* pollDiscDrives */


static LRESULT CALLBACK detectCDWndProc(HWND hwnd, UINT msg,
                                        WPARAM wp, LPARAM lparam)
{
    PDEV_BROADCAST_HDR lpdb = (PDEV_BROADCAST_HDR) lparam;
    PDEV_BROADCAST_VOLUME lpdbv = (PDEV_BROADCAST_VOLUME) lparam;
    const int removed = (wp == DBT_DEVICEREMOVECOMPLETE);

    if (msg == WM_DESTROY)
        return 0;
    else if ((msg != WM_DEVICECHANGE) ||
             ((wp != DBT_DEVICEARRIVAL) && (wp != DBT_DEVICEREMOVECOMPLETE)) ||
             (lpdb->dbch_devicetype != DBT_DEVTYP_VOLUME) ||
             ((lpdbv->dbcv_flags & DBTF_MEDIA) == 0))
    {
        return DefWindowProcW(hwnd, msg, wp, lparam);
    } /* else if */

    if (removed)
        drivesWithMediaBitmap &= ~lpdbv->dbcv_unitmask;
    else
        drivesWithMediaBitmap |= lpdbv->dbcv_unitmask;

    return TRUE;
} /* detectCDWndProc */


static DWORD WINAPI detectCDThread(LPVOID lpParameter)
{
    const char *classname = "PhysicsFSDetectCDCatcher";
    const char *winname = "PhysicsFSDetectCDMsgWindow";
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    ATOM class_atom = 0;
    WNDCLASSEXA wce;
    MSG msg;

    memset(&wce, '\0', sizeof (wce));
    wce.cbSize = sizeof (wce);
    wce.lpfnWndProc = detectCDWndProc;
    wce.lpszClassName = classname;
    wce.hInstance = hInstance;
    class_atom = RegisterClassExA(&wce);
    if (class_atom == 0)
    {
        initialDiscDetectionComplete = 1;  /* let main thread go on. */
        return 0;
    } /* if */

    detectCDHwnd = CreateWindowExA(0, classname, winname, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        CW_USEDEFAULT, HWND_DESKTOP, NULL, hInstance, NULL);

    if (detectCDHwnd == NULL)
    {
        initialDiscDetectionComplete = 1;  /* let main thread go on. */
        UnregisterClassA(classname, hInstance);
        return 0;
    } /* if */

    /* We'll get events when discs come and go from now on. */

    /* Do initial detection, possibly blocking awhile... */
    drivesWithMediaBitmap = pollDiscDrives();
    initialDiscDetectionComplete = 1;  /* let main thread go on. */

    do
    {
        const BOOL rc = GetMessageW(&msg, detectCDHwnd, 0, 0);
        if ((rc == 0) || (rc == -1))
            break;  /* don't care if WM_QUIT or error break this loop. */
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    } while (1);

    /* we've been asked to quit. */
    DestroyWindow(detectCDHwnd);

    do
    {
        const BOOL rc = GetMessage(&msg, detectCDHwnd, 0, 0);
        if ((rc == 0) || (rc == -1))
            break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    } while (1);

    UnregisterClassA(classname, hInstance);

    return 0;
} /* detectCDThread */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    char drive_str[4] = { 'x', ':', '\\', '\0' };
    DWORD drives = 0;
    DWORD i;

    /*
     * If you poll a drive while a user is inserting a disc, the OS will
     *  block this thread until the drive has spun up. So we swallow the risk
     *  once for initial detection, and spin a thread that will get device
     *  events thereafter, for apps that use this interface to poll for
     *  disc insertion.
     */
    if (!detectCDThreadHandle)
    {
        initialDiscDetectionComplete = 0;
        detectCDThreadHandle = CreateThread(NULL,0,detectCDThread,NULL,0,NULL);
        if (detectCDThreadHandle == NULL)
            return;  /* oh well. */

        while (!initialDiscDetectionComplete)
            Sleep(50);
    } /* if */

    drives = drivesWithMediaBitmap; /* whatever the thread has seen, we take. */
    for (i = 'A'; i <= 'Z'; i++)
    {
        if (drives & (1 << (i - 'A')))
        {
            drive_str[0] = (char) i;
            cb(data, drive_str);
        } /* if */
    } /* for */
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    DWORD buflen = 64;
    LPWSTR modpath = NULL;
    char *retval = NULL;

    while (1)
    {
        DWORD rc;
        void *ptr;

        if ( (ptr = allocator.Realloc(modpath, buflen*sizeof(WCHAR))) == NULL )
        {
            allocator.Free(modpath);
            BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
        } /* if */
        modpath = (LPWSTR) ptr;

        rc = GetModuleFileNameW(NULL, modpath, buflen);
        if (rc == 0)
        {
            allocator.Free(modpath);
            BAIL_MACRO(errcodeFromWinApi(), NULL);
        } /* if */

        if (rc < buflen)
        {
            buflen = rc;
            break;
        } /* if */

        buflen *= 2;
    } /* while */

    if (buflen > 0)  /* just in case... */
    {
        WCHAR *ptr = (modpath + buflen) - 1;
        while (ptr != modpath)
        {
            if (*ptr == '\\')
                break;
            ptr--;
        } /* while */

        if ((ptr == modpath) && (*ptr != '\\'))
            PHYSFS_setErrorCode(PHYSFS_ERR_OTHER_ERROR);  /* oh well. */
        else
        {
            *(ptr+1) = '\0';  /* chop off filename. */
            retval = unicodeToUtf8Heap(modpath);
        } /* else */
    } /* else */
    allocator.Free(modpath);

    return retval;   /* w00t. */
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    /*
     * Vista and later has a new API for this, but SHGetFolderPath works there,
     *  and apparently just wraps the new API. This is the new way to do it:
     *
     *     SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE,
     *                          NULL, &wszPath);
     */

    WCHAR path[MAX_PATH];
    char *utf8 = NULL;
    size_t len = 0;
    char *retval = NULL;

    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                   NULL, 0, path)))
        BAIL_MACRO(PHYSFS_ERR_OS_ERROR, NULL);

    utf8 = unicodeToUtf8Heap(path);
    BAIL_IF_MACRO(!utf8, ERRPASS, NULL);
    len = strlen(utf8) + strlen(org) + strlen(app) + 4;
    retval = allocator.Malloc(len);
    if (!retval)
    {
        allocator.Free(utf8);
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    sprintf(retval, "%s\\%s\\%s\\", utf8, org, app);
    allocator.Free(utf8);
    return retval;
} /* __PHYSFS_platformCalcPrefDir */


char *__PHYSFS_platformCalcUserDir(void)
{
    typedef BOOL (WINAPI *fnGetUserProfDirW)(HANDLE, LPWSTR, LPDWORD);
    fnGetUserProfDirW pGetDir = NULL;
    HANDLE lib = NULL;
    HANDLE accessToken = NULL;       /* Security handle to process */
    char *retval = NULL;

    lib = LoadLibraryA("userenv.dll");
    BAIL_IF_MACRO(!lib, errcodeFromWinApi(), NULL);
    pGetDir=(fnGetUserProfDirW) GetProcAddress(lib,"GetUserProfileDirectoryW");
    GOTO_IF_MACRO(!pGetDir, errcodeFromWinApi(), done);

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &accessToken))
        GOTO_MACRO(errcodeFromWinApi(), done);
    else
    {
        DWORD psize = 0;
        WCHAR dummy = 0;
        LPWSTR wstr = NULL;
        BOOL rc = 0;

        /*
         * Should fail. Will write the size of the profile path in
         *  psize. Also note that the second parameter can't be
         *  NULL or the function fails.
         */
    	rc = pGetDir(accessToken, &dummy, &psize);
        assert(!rc);  /* !!! FIXME: handle this gracefully. */
        (void) rc;

        /* Allocate memory for the profile directory */
        wstr = (LPWSTR) __PHYSFS_smallAlloc((psize + 1) * sizeof (WCHAR));
        if (wstr != NULL)
        {
            if (pGetDir(accessToken, wstr, &psize))
            {
                /* Make sure it ends in a dirsep. We allocated +1 for this. */
                if (wstr[psize - 2] != '\\')
                {
                    wstr[psize - 1] = '\\';
                    wstr[psize - 0] = '\0';
                } /* if */
                retval = unicodeToUtf8Heap(wstr);
            } /* if */
            __PHYSFS_smallFree(wstr);
        } /* if */

        CloseHandle(accessToken);
    } /* if */

done:
    FreeLibrary(lib);
    return retval;  /* We made it: hit the showers. */
} /* __PHYSFS_platformCalcUserDir */


void *__PHYSFS_platformGetThreadID(void)
{
    return ( (void *) ((size_t) GetCurrentThreadId()) );
} /* __PHYSFS_platformGetThreadID */


static int isSymlinkAttrs(const DWORD attr, const DWORD tag)
{
    return ( (attr & FILE_ATTRIBUTE_REPARSE_POINT) && 
             (tag == PHYSFS_IO_REPARSE_TAG_SYMLINK) );
} /* isSymlinkAttrs */


void __PHYSFS_platformEnumerateFiles(const char *dirname,
                                     PHYSFS_EnumFilesCallback callback,
                                     const char *origdir,
                                     void *callbackdata)
{
    HANDLE dir = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW entw;
    size_t len = strlen(dirname);
    char *searchPath = NULL;
    WCHAR *wSearchPath = NULL;

    /* Allocate a new string for path, maybe '\\', "*", and NULL terminator */
    searchPath = (char *) __PHYSFS_smallAlloc(len + 3);
    if (searchPath == NULL)
        return;

    /* Copy current dirname */
    strcpy(searchPath, dirname);

    /* if there's no '\\' at the end of the path, stick one in there. */
    if (searchPath[len - 1] != '\\')
    {
        searchPath[len++] = '\\';
        searchPath[len] = '\0';
    } /* if */

    /* Append the "*" to the end of the string */
    strcat(searchPath, "*");

    UTF8_TO_UNICODE_STACK_MACRO(wSearchPath, searchPath);
    if (!wSearchPath)
        return;  /* oh well. */

    dir = FindFirstFileW(wSearchPath, &entw);

    __PHYSFS_smallFree(wSearchPath);
    __PHYSFS_smallFree(searchPath);
    if (dir == INVALID_HANDLE_VALUE)
        return;

    do
    {
        const DWORD attr = entw.dwFileAttributes;
        const DWORD tag = entw.dwReserved0;
        const WCHAR *fn = entw.cFileName;
        char *utf8;

        if ((fn[0] == '.') && (fn[1] == '\0'))
            continue;
        if ((fn[0] == '.') && (fn[1] == '.') && (fn[2] == '\0'))
            continue;

        utf8 = unicodeToUtf8Heap(fn);
        if (utf8 != NULL)
        {
            callback(callbackdata, origdir, utf8);
            allocator.Free(utf8);
        } /* if */
    } while (FindNextFileW(dir, &entw) != 0);

    FindClose(dir);
} /* __PHYSFS_platformEnumerateFiles */


int __PHYSFS_platformMkDir(const char *path)
{
    WCHAR *wpath;
    DWORD rc;
    UTF8_TO_UNICODE_STACK_MACRO(wpath, path);
    rc = CreateDirectoryW(wpath, NULL);
    __PHYSFS_smallFree(wpath);
    BAIL_IF_MACRO(rc == 0, errcodeFromWinApi(), 0);
    return 1;
} /* __PHYSFS_platformMkDir */


int __PHYSFS_platformInit(void)
{
    return 1;  /* It's all good */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    if (detectCDThreadHandle)
    {
        if (detectCDHwnd)
            PostMessageW(detectCDHwnd, WM_QUIT, 0, 0);
        CloseHandle(detectCDThreadHandle);
        detectCDThreadHandle = NULL;
        initialDiscDetectionComplete = 0;
        drivesWithMediaBitmap = 0;
    } /* if */

    return 1; /* It's all good */
} /* __PHYSFS_platformDeinit */


static void *doOpen(const char *fname, DWORD mode, DWORD creation, int rdonly)
{
    HANDLE fileh;
    WinApiFile *retval;
    WCHAR *wfname;

    UTF8_TO_UNICODE_STACK_MACRO(wfname, fname);
    BAIL_IF_MACRO(!wfname, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    fileh = CreateFileW(wfname, mode, FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
    __PHYSFS_smallFree(wfname);

    BAIL_IF_MACRO(fileh == INVALID_HANDLE_VALUE,errcodeFromWinApi(), NULL);

    retval = (WinApiFile *) allocator.Malloc(sizeof (WinApiFile));
    if (!retval)
    {
        CloseHandle(fileh);
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* if */

    retval->readonly = rdonly;
    retval->handle = fileh;
    return retval;
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return doOpen(filename, GENERIC_READ, OPEN_EXISTING, 1);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return doOpen(filename, GENERIC_WRITE, CREATE_ALWAYS, 0);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    void *retval = doOpen(filename, GENERIC_WRITE, OPEN_ALWAYS, 0);
    if (retval != NULL)
    {
        HANDLE h = ((WinApiFile *) retval)->handle;
        DWORD rc = SetFilePointer(h, 0, NULL, FILE_END);
        if (rc == PHYSFS_INVALID_SET_FILE_POINTER)
        {
            const PHYSFS_ErrorCode err = errcodeFromWinApi();
            CloseHandle(h);
            allocator.Free(retval);
            BAIL_MACRO(err, NULL);
        } /* if */
    } /* if */

    return retval;
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buf, PHYSFS_uint64 len)
{
    HANDLE Handle = ((WinApiFile *) opaque)->handle;
    PHYSFS_sint64 totalRead = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL_MACRO(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    while (len > 0)
    {
        const DWORD thislen = (len > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD) len;
        DWORD numRead = 0;
        if (!ReadFile(Handle, buf, thislen, &numRead, NULL))
            BAIL_MACRO(errcodeFromWinApi(), -1);
        len -= (PHYSFS_uint64) numRead;
        totalRead += (PHYSFS_sint64) numRead;
        if (numRead != thislen)
            break;
    } /* while */

    return totalRead;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint64 len)
{
    HANDLE Handle = ((WinApiFile *) opaque)->handle;
    PHYSFS_sint64 totalWritten = 0;

    if (!__PHYSFS_ui64FitsAddressSpace(len))
        BAIL_MACRO(PHYSFS_ERR_INVALID_ARGUMENT, -1);

    while (len > 0)
    {
        const DWORD thislen = (len > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD) len;
        DWORD numWritten = 0;
        if (!WriteFile(Handle, buffer, thislen, &numWritten, NULL))
            BAIL_MACRO(errcodeFromWinApi(), -1);
        len -= (PHYSFS_uint64) numWritten;
        totalWritten += (PHYSFS_sint64) numWritten;
        if (numWritten != thislen)
            break;
    } /* while */

    return totalWritten;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    HANDLE Handle = ((WinApiFile *) opaque)->handle;
    LONG HighOrderPos;
    PLONG pHighOrderPos;
    DWORD rc;

    /* Get the high order 32-bits of the position */
    HighOrderPos = HIGHORDER_UINT64(pos);

    /*
     * MSDN: "If you do not need the high-order 32 bits, this
     *         pointer must be set to NULL."
     */
    pHighOrderPos = (HighOrderPos) ? &HighOrderPos : NULL;

    /* Move pointer "pos" count from start of file */
    rc = SetFilePointer(Handle, LOWORDER_UINT64(pos),
                        pHighOrderPos, FILE_BEGIN);

    if ( (rc == PHYSFS_INVALID_SET_FILE_POINTER) &&
         (GetLastError() != NO_ERROR) )
    {
        BAIL_MACRO(errcodeFromWinApi(), 0);
    } /* if */
    
    return 1;  /* No error occured */
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    HANDLE Handle = ((WinApiFile *) opaque)->handle;
    LONG HighPos = 0;
    DWORD LowPos;
    PHYSFS_sint64 retval;

    /* Get current position */
    LowPos = SetFilePointer(Handle, 0, &HighPos, FILE_CURRENT);
    if ( (LowPos == PHYSFS_INVALID_SET_FILE_POINTER) &&
         (GetLastError() != NO_ERROR) )
    {
        BAIL_MACRO(errcodeFromWinApi(), -1);
    } /* if */
    else
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = (((PHYSFS_uint64) HighPos) << 32) | LowPos;
        assert(retval >= 0);
    } /* else */

    return retval;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    HANDLE Handle = ((WinApiFile *) opaque)->handle;
    DWORD SizeHigh;
    DWORD SizeLow;
    PHYSFS_sint64 retval;

    SizeLow = GetFileSize(Handle, &SizeHigh);
    if ( (SizeLow == PHYSFS_INVALID_SET_FILE_POINTER) &&
         (GetLastError() != NO_ERROR) )
    {
        BAIL_MACRO(errcodeFromWinApi(), -1);
    } /* if */
    else
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = (((PHYSFS_uint64) SizeHigh) << 32) | SizeLow;
        assert(retval >= 0);
    } /* else */

    return retval;
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
    WinApiFile *fh = ((WinApiFile *) opaque);
    if (!fh->readonly)
        BAIL_IF_MACRO(!FlushFileBuffers(fh->handle), errcodeFromWinApi(), 0);

    return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
    HANDLE Handle = ((WinApiFile *) opaque)->handle;
    (void) CloseHandle(Handle); /* ignore errors. You should have flushed! */
    allocator.Free(opaque);
} /* __PHYSFS_platformClose */


static int doPlatformDelete(LPWSTR wpath)
{
    const int isdir = (GetFileAttributesW(wpath) & FILE_ATTRIBUTE_DIRECTORY);
    const BOOL rc = (isdir) ? RemoveDirectoryW(wpath) : DeleteFileW(wpath);
    BAIL_IF_MACRO(!rc, errcodeFromWinApi(), 0);
    return 1;   /* if you made it here, it worked. */
} /* doPlatformDelete */


int __PHYSFS_platformDelete(const char *path)
{
    int retval = 0;
    LPWSTR wpath = NULL;
    UTF8_TO_UNICODE_STACK_MACRO(wpath, path);
    BAIL_IF_MACRO(!wpath, PHYSFS_ERR_OUT_OF_MEMORY, 0);
    retval = doPlatformDelete(wpath);
    __PHYSFS_smallFree(wpath);
    return retval;
} /* __PHYSFS_platformDelete */


void *__PHYSFS_platformCreateMutex(void)
{
    LPCRITICAL_SECTION lpcs;
    lpcs = (LPCRITICAL_SECTION) allocator.Malloc(sizeof (CRITICAL_SECTION));
    BAIL_IF_MACRO(!lpcs, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    InitializeCriticalSection(lpcs);
    return lpcs;
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    DeleteCriticalSection((LPCRITICAL_SECTION) mutex);
    allocator.Free(mutex);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    EnterCriticalSection((LPCRITICAL_SECTION) mutex);
    return 1;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    LeaveCriticalSection((LPCRITICAL_SECTION) mutex);
} /* __PHYSFS_platformReleaseMutex */


static PHYSFS_sint64 FileTimeToPhysfsTime(const FILETIME *ft)
{
    SYSTEMTIME st_utc;
    SYSTEMTIME st_localtz;
    TIME_ZONE_INFORMATION tzi;
    DWORD tzid;
    PHYSFS_sint64 retval;
    struct tm tm;
    BOOL rc;

    BAIL_IF_MACRO(!FileTimeToSystemTime(ft, &st_utc), errcodeFromWinApi(), -1);
    tzid = GetTimeZoneInformation(&tzi);
    BAIL_IF_MACRO(tzid == TIME_ZONE_ID_INVALID, errcodeFromWinApi(), -1);
    rc = SystemTimeToTzSpecificLocalTime(&tzi, &st_utc, &st_localtz);
    BAIL_IF_MACRO(!rc, errcodeFromWinApi(), -1);

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
    BAIL_IF_MACRO(retval == -1, PHYSFS_ERR_OS_ERROR, -1);
    return retval;
} /* FileTimeToPhysfsTime */


int __PHYSFS_platformStat(const char *filename, PHYSFS_Stat *st)
{
    WIN32_FILE_ATTRIBUTE_DATA winstat;
    WCHAR *wstr = NULL;
    DWORD err = 0;
    BOOL rc = 0;

    UTF8_TO_UNICODE_STACK_MACRO(wstr, filename);
    BAIL_IF_MACRO(!wstr, PHYSFS_ERR_OUT_OF_MEMORY, 0);
    rc = GetFileAttributesExW(wstr, GetFileExInfoStandard, &winstat);
    err = (!rc) ? GetLastError() : 0;
    __PHYSFS_smallFree(wstr);
    BAIL_IF_MACRO(!rc, errcodeFromWinApiError(err), 0);

    st->modtime = FileTimeToPhysfsTime(&winstat.ftLastWriteTime);
    st->accesstime = FileTimeToPhysfsTime(&winstat.ftLastAccessTime);
    st->createtime = FileTimeToPhysfsTime(&winstat.ftCreationTime);

    if(winstat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        st->filetype = PHYSFS_FILETYPE_DIRECTORY;
        st->filesize = 0;
    } /* if */

    else if(winstat.dwFileAttributes & (FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_DEVICE))
    {
        /* !!! FIXME: what are reparse points? */
        st->filetype = PHYSFS_FILETYPE_OTHER;
        /* !!! FIXME: don't rely on this */
        st->filesize = 0;
    } /* else if */

    /* !!! FIXME: check for symlinks on Vista. */

    else
    {
        st->filetype = PHYSFS_FILETYPE_REGULAR;
        st->filesize = (((PHYSFS_uint64) winstat.nFileSizeHigh) << 32) | winstat.nFileSizeLow;
    } /* else */

    st->readonly = ((winstat.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0);

    return 1;
} /* __PHYSFS_platformStat */


/* !!! FIXME: Don't use C runtime for allocators? */
int __PHYSFS_platformSetDefaultAllocator(PHYSFS_Allocator *a)
{
    return 0;  /* just use malloc() and friends. */
} /* __PHYSFS_platformSetDefaultAllocator */

#endif  /* PHYSFS_PLATFORM_WINDOWS */

/* end of windows.c ... */


