/*
* Windows Runtime support routines for PhysicsFS.
*
* Please see the file LICENSE.txt in the source's root directory.
*
*  This file is based on "platform_windows.c" by Ryan C. Gordon and Gregory S. Read.
*  Windows Runtime-specific additions and changes are made by Martin "T-Bone" Ahrnbom. 
*
* Instructions for setting up PhysFS in a WinRT Universal 8.1 app in Visual Studio 2013:
* (hopefully these instructions will be somewhat valid with Windows 10 Apps as well...)
*
*  1. In Visual Studio 2013 (or later?), hit File -> New -> Project...
*  2. On the left, navigate to Templates -> Visual C++ -> Store Apps -> Universal Apps
*  3. In the middle of the window, select "DLL (Universal Apps)"
*  4. Near the bottom, give the project a name (like PhysFS-WinRT) and choose a location
*  5. In the Solution Explorer (to the right typically), delete all .cpp and .h files in
*     PhysFS-WinRT.Shared except targetver.h.
*  6. In Windows Explorer, find the "src" folder of the PhysFS source code. Select all files
* 	  in the folder (ignore the "lzma" folder, we're not including 7Zip support because it's messy to compile).
*	  Drag and drop all of the source files onto PhysFS-WinRT-Shared in Visual Studio.
*  7. In Windows Explorer, find the file called "physfs.h". Copy this file into a folder of its own somewhere.
*	  I suggest naming that folder "include". This will be your "include" folder for any projects using PhysFS.
*  8. In the Solution Explorer, right click on PhysFS-WinRT.Windows and select Properties. Make sure that "Configuration" is set to 
*     "All Configurations" and "Platform" is set to "All Platforms" near the top of the window.
*  9. On the left, select "Precompiled Headers". Change "Precompiled Header" from "Use (/Yu)" to "Not Using Precompiled Headers".
* 10. On the left, navigate to Configuration Properties -> C/C++ -> Preprocessor. In Preprocessor Definitions, add "_CRT_SECURE_NO_WARNINGS"
* 11. Hit the OK button.
* 12. Repeat steps 8-11 but for PhysFS-WinRT.WindowsPhone.
* 13. In the Solution Explorer, find "platform_winrt.cpp" in PhysFS-WinRT.Shared. Right click on it and select "Properties". 
* 14. On the left, navigate to Configuration Properties -> C/C++ -> General. On the right, change "Consume Windows Runtime Extensions"
*	  from "No" to "Yes (/ZW)". Hit "OK".
* 15. Near the top of the Visual Studio window, select BUILD -> Batch Build... Hit "Select All", and then "Build".
* 16. Now you have a bunch of .dll and .lib files, as well as an include folder that you can use in your projects!
* 17. ???
* 18. Profit!
*/


#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_WINRT

#include "physfs_internal.h"

#include <windows.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

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


typedef BOOL(WINAPI *fnSTEM)(DWORD, LPDWORD b);

static DWORD pollDiscDrives(void)
{
	// We don't do discs
	return 0;
} /* pollDiscDrives */


static LRESULT CALLBACK detectCDWndProc(HWND hwnd, UINT msg,
	WPARAM wp, LPARAM lparam)
{
	return FALSE;
} /* detectCDWndProc */


static DWORD WINAPI detectCDThread(LPVOID lpParameter)
{
	return 0;
} /* detectCDThread */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
	return;
} /* __PHYSFS_platformDetectAvailableCDs */


static char *unicodeToUtf8Heap(const WCHAR *w_str)
{
	char *retval = NULL;
	if (w_str != NULL)
	{
		void *ptr = NULL;
		const PHYSFS_uint64 len = (wStrLen(w_str) * 4) + 1;
		retval = (char*)allocator.Malloc(len);
		BAIL_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
		PHYSFS_utf8FromUtf16((const PHYSFS_uint16 *)w_str, retval, len);
		ptr = allocator.Realloc(retval, strlen(retval) + 1); /* shrink. */
		if (ptr != NULL)
			retval = (char *)ptr;
	} /* if */
	return retval;
} /* unicodeToUtf8Heap */

char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
	const wchar_t* path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();
	wchar_t path2[1024];
	wcscpy_s(path2, path);
	wcscat_s(path2, L"\\");
	return unicodeToUtf8Heap(path2);

} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
	const wchar_t* path = Windows::Storage::ApplicationData::Current->LocalFolder->Path->Data();
	wchar_t path2[1024];
	wcscpy_s(path2, path);
	wcscat_s(path2, L"\\");
	return unicodeToUtf8Heap(path2);
} /* __PHYSFS_platformCalcPrefDir */


char *__PHYSFS_platformCalcUserDir(void)
{
	return __PHYSFS_platformCalcPrefDir(NULL, NULL);
} /* __PHYSFS_platformCalcUserDir */


void *__PHYSFS_platformGetThreadID(void)
{
	return ((void *)((size_t)GetCurrentThreadId()));
} /* __PHYSFS_platformGetThreadID */


static int isSymlinkAttrs(const DWORD attr, const DWORD tag)
{
	return ((attr & FILE_ATTRIBUTE_REPARSE_POINT) &&
		(tag == PHYSFS_IO_REPARSE_TAG_SYMLINK));
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
	searchPath = (char *)__PHYSFS_smallAlloc(len + 3);
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

	//dir = FindFirstFileW(wSearchPath, &entw);
	dir = FindFirstFileExW(wSearchPath, FindExInfoStandard, &entw, FindExSearchNameMatch, NULL, 0);

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
	return 1; /* It's all good */
} /* __PHYSFS_platformDeinit */


static void *doOpen(const char *fname, DWORD mode, DWORD creation, int rdonly)
{
	HANDLE fileh;
	WinApiFile *retval;
	WCHAR *wfname;

	UTF8_TO_UNICODE_STACK_MACRO(wfname, fname);
	BAIL_IF_MACRO(!wfname, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
	//fileh = CreateFileW(wfname, mode, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, creation, FILE_ATTRIBUTE_NORMAL, NULL);
	fileh = CreateFile2(wfname, mode, FILE_SHARE_READ | FILE_SHARE_WRITE, creation, NULL);
	__PHYSFS_smallFree(wfname);

	BAIL_IF_MACRO(fileh == INVALID_HANDLE_VALUE, errcodeFromWinApi(), NULL);

	retval = (WinApiFile *)allocator.Malloc(sizeof(WinApiFile));
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
		HANDLE h = ((WinApiFile *)retval)->handle;
		//DWORD rc = SetFilePointer(h, 0, NULL, FILE_END);
		const LARGE_INTEGER zero = { 0 };
		DWORD rc = SetFilePointerEx(h, zero, NULL, FILE_END);
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
	HANDLE Handle = ((WinApiFile *)opaque)->handle;
	PHYSFS_sint64 totalRead = 0;

	if (!__PHYSFS_ui64FitsAddressSpace(len))
		BAIL_MACRO(PHYSFS_ERR_INVALID_ARGUMENT, -1);

	while (len > 0)
	{
		const DWORD thislen = (len > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD)len;
		DWORD numRead = 0;
		if (!ReadFile(Handle, buf, thislen, &numRead, NULL))
			BAIL_MACRO(errcodeFromWinApi(), -1);
		len -= (PHYSFS_uint64)numRead;
		totalRead += (PHYSFS_sint64)numRead;
		if (numRead != thislen)
			break;
	} /* while */
	
	return totalRead;
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
	PHYSFS_uint64 len)
{
	HANDLE Handle = ((WinApiFile *)opaque)->handle;
	PHYSFS_sint64 totalWritten = 0;

	if (!__PHYSFS_ui64FitsAddressSpace(len))
		BAIL_MACRO(PHYSFS_ERR_INVALID_ARGUMENT, -1);

	while (len > 0)
	{
		const DWORD thislen = (len > 0xFFFFFFFF) ? 0xFFFFFFFF : (DWORD)len;
		DWORD numWritten = 0;
		if (!WriteFile(Handle, buffer, thislen, &numWritten, NULL))
			BAIL_MACRO(errcodeFromWinApi(), -1);
		len -= (PHYSFS_uint64)numWritten;
		totalWritten += (PHYSFS_sint64)numWritten;
		if (numWritten != thislen)
			break;
	} /* while */

	return totalWritten;
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
	HANDLE Handle = ((WinApiFile *)opaque)->handle;
	BOOL rc;

	LARGE_INTEGER li;
	li.LowPart = LOWORDER_UINT64(pos);
	li.HighPart = HIGHORDER_UINT64(pos);

	rc = SetFilePointerEx(Handle, li, NULL, FILE_BEGIN);

	if (!rc && (GetLastError() != NO_ERROR))
	{
		BAIL_MACRO(errcodeFromWinApi(), 0);
	} /* if */

	return 1;  /* No error occured */
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
	HANDLE Handle = ((WinApiFile *)opaque)->handle;
	PHYSFS_sint64 retval;
	BOOL rc;

	LARGE_INTEGER zero;
	zero.QuadPart = 0;
	LARGE_INTEGER out;

	rc = SetFilePointerEx(Handle, zero, &out, FILE_CURRENT);
	if (!rc)
	{
		BAIL_MACRO(errcodeFromWinApi(), -1);
	} /* if */
	else
	{
		retval = out.QuadPart;
		assert(retval >= 0);
	} /* else */

	return retval;
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
	HANDLE Handle = ((WinApiFile *)opaque)->handle;
	PHYSFS_sint64 retval;

	FILE_STANDARD_INFO file_info = { 0 };
	const BOOL res = GetFileInformationByHandleEx(Handle, FileStandardInfo, &file_info, sizeof(file_info));
	if (res) {
		retval = file_info.EndOfFile.QuadPart;
		assert(retval >= 0);
	}
	else {
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
	}

	
	return retval;
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformFlush(void *opaque)
{
	WinApiFile *fh = ((WinApiFile *)opaque);
	if (!fh->readonly)
		BAIL_IF_MACRO(!FlushFileBuffers(fh->handle), errcodeFromWinApi(), 0);

	return 1;
} /* __PHYSFS_platformFlush */


void __PHYSFS_platformClose(void *opaque)
{
	HANDLE Handle = ((WinApiFile *)opaque)->handle;
	(void)CloseHandle(Handle); /* ignore errors. You should have flushed! */
	allocator.Free(opaque);
} /* __PHYSFS_platformClose */


static int doPlatformDelete(LPWSTR wpath)
{
	//const int isdir = (GetFileAttributesW(wpath) & FILE_ATTRIBUTE_DIRECTORY);
	int isdir = 0;
	WIN32_FILE_ATTRIBUTE_DATA file_info;
	const BOOL res = GetFileAttributesEx(wpath, GetFileExInfoStandard, &file_info);
	if (res) {
		isdir = (file_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	}

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
	lpcs = (LPCRITICAL_SECTION)allocator.Malloc(sizeof(CRITICAL_SECTION));
	BAIL_IF_MACRO(!lpcs, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
	//InitializeCriticalSection(lpcs);
	InitializeCriticalSectionEx(lpcs, 2000, 0);
	return lpcs;
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
	DeleteCriticalSection((LPCRITICAL_SECTION)mutex);
	allocator.Free(mutex);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
	EnterCriticalSection((LPCRITICAL_SECTION)mutex);
	return 1;
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
	LeaveCriticalSection((LPCRITICAL_SECTION)mutex);
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
	retval = (PHYSFS_sint64)mktime(&tm);
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

	if (winstat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		st->filetype = PHYSFS_FILETYPE_DIRECTORY;
		st->filesize = 0;
	} /* if */

	else if (winstat.dwFileAttributes & (FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_DEVICE))
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
		st->filesize = (((PHYSFS_uint64)winstat.nFileSizeHigh) << 32) | winstat.nFileSizeLow;
	} /* else */

	st->readonly = ((winstat.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0);

	return 1;
} /* __PHYSFS_platformStat */


/* !!! FIXME: Don't use C runtime for allocators? */
int __PHYSFS_platformSetDefaultAllocator(PHYSFS_Allocator *a)
{
	return 0;  /* just use malloc() and friends. */
} /* __PHYSFS_platformSetDefaultAllocator */


#endif /* PHYSFS_PLATFORM_WINRT */