/*
 * Skeleton platform-dependent support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <windows.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#define INVALID_FILE_ATTRIBUTES		0xFFFFFFFF
#define INVALID_SET_FILE_POINTER	0xFFFFFFFF
typedef struct
{
    HANDLE handle;
    int readonly;
} winCEfile;


const char *__PHYSFS_platformDirSeparator = "\\";
static char *userDir = NULL;

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


static char *UnicodeToAsc(const wchar_t *w_str)
{
    char *str=NULL;

    if(w_str!=NULL)
    {
	int len=wcslen(w_str)+1;
	str=(char *)malloc(len);

	if(WideCharToMultiByte(CP_ACP,0,w_str,-1,str,len,NULL,NULL)==0)
	{	//Conversion failed
	    free(str);
	    return NULL;
	}
	else
	{	//Conversion successful
	    return(str);
	}

    }
    else
    {	//Given NULL string
	return NULL;
    }
}

static wchar_t *AscToUnicode(const char *str)
{
    wchar_t *w_str=NULL;
    if(str!=NULL)
    {
	int len=strlen(str)+1;
	w_str=(wchar_t *)malloc(sizeof(wchar_t)*len);
	if(MultiByteToWideChar(CP_ACP,0,str,-1,w_str,len)==0)
	{
	    free(w_str);
	    return NULL;
	}
	else
	{
	    return(w_str);
	}
    }
    else
    {
	return NULL;
    }
}


static char *getExePath()
{
    DWORD buflen;
    int success = 0;
    TCHAR *ptr = NULL;
    TCHAR *retval = (TCHAR *) malloc(sizeof (TCHAR) * (MAX_PATH + 1));
    char *charretval;
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    retval[0] = _T('\0');
    buflen = GetModuleFileName(NULL, retval, MAX_PATH + 1);
    if (buflen <= 0) {
        __PHYSFS_setError(win32strerror());
    } else {
        retval[buflen] = '\0';  /* does API always null-terminate this? */
	ptr = retval+buflen;
	while( ptr != retval )
	{
	    if( *ptr != _T('\\') ) {
		*ptr-- = _T('\0');
	    } else {
		break;
	    }
	}
	success = 1;
    } /* else */

    if (!success)
    {
        free(retval);
        return(NULL);  /* physfs error message will be set, above. */
    } /* if */

    /* free up the bytes we didn't actually use. */
    ptr = (TCHAR *) realloc(retval, sizeof(TCHAR) * _tcslen(retval) + 1);
    if (ptr != NULL)
        retval = ptr;

    charretval = UnicodeToAsc(retval);
    free(retval);
    if(charretval == NULL) {
	BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    }
    
    return(charretval);   /* w00t. */
} /* getExePath */

int __PHYSFS_platformInit(void)
{
    userDir = getExePath();
    BAIL_IF_MACRO(userDir == NULL, NULL, 0); /* failed? */
    return(1);  /* always succeed. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    free(userDir);
    return(1);  /* always succeed. */
} /* __PHYSFS_platformDeinit */


char **__PHYSFS_platformDetectAvailableCDs(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    return(getExePath());
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformGetUserName(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    return userDir;
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformGetUserDir */


PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    return(1);  /* single threaded. */
} /* __PHYSFS_platformGetThreadID */


int __PHYSFS_platformStricmp(const char *x, const char *y)
{    
    return(_stricmp(x, y));

} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformExists(const char *fname)
{
    int retval=0;

    wchar_t *w_fname=AscToUnicode(fname);
	
    if(w_fname!=NULL)
    {
	retval=(GetFileAttributes(w_fname) != INVALID_FILE_ATTRIBUTES);
	free(w_fname);
    }

    return(retval);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    int retval=0;

    wchar_t *w_fname=AscToUnicode(fname);

    if(w_fname!=NULL)
    {
	retval=((GetFileAttributes(w_fname) & FILE_ATTRIBUTE_DIRECTORY) != 0);
	free(w_fname);
    }

    return(retval);
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


void __PHYSFS_platformTimeslice(void)
{
    Sleep(10);
} /* __PHYSFS_platformTimeslice */


LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks)
{
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;
    HANDLE dir;
    WIN32_FIND_DATA ent;
    char *SearchPath;
    wchar_t *w_SearchPath;
    size_t len = strlen(dirname);

    /* Allocate a new string for path, maybe '\\', "*", and NULL terminator */
    SearchPath = (char *) alloca(len + 3);
    BAIL_IF_MACRO(SearchPath == NULL, ERR_OUT_OF_MEMORY, NULL);	

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

    w_SearchPath=AscToUnicode(SearchPath);

    dir = FindFirstFile(w_SearchPath, &ent);
    free(w_SearchPath);
    free(SearchPath);

    if(dir == INVALID_HANDLE_VALUE)
    {
	return NULL;
    }

    do
    {
        if (wcscmp(ent.cFileName, L".") == 0)
            continue;

        if (wcscmp(ent.cFileName, L"..") == 0)
            continue;

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l == NULL)
            break;

	l->str=UnicodeToAsc(ent.cFileName);

        if (l->str == NULL)
        {
            free(l);
            break;
        }


        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } while (FindNextFile(dir, &ent) != 0);

    FindClose(dir);
    return(retval);
} /* __PHYSFS_platformEnumerateFiles */


char *__PHYSFS_platformCurrentDir(void)
{
    return("\\");
} /* __PHYSFS_platformCurrentDir */


char *__PHYSFS_platformRealPath(const char *path)
{
    char *retval=(char *)malloc(strlen(path)+1);

    strcpy(retval,path);

    return(retval);

} /* __PHYSFS_platformRealPath */


int __PHYSFS_platformMkDir(const char *path)
{
    wchar_t *w_path = AscToUnicode(path);
    if(w_path!=NULL)
    {
	DWORD rc = CreateDirectory(w_path, NULL);
	free(w_path);
	if(rc==0)
	{
	    return(0);
	}
	return(1);
    }
    else
    {
	return(0);
    }
} /* __PHYSFS_platformMkDir */


static void *doOpen(const char *fname, DWORD mode, DWORD creation, int rdonly)
{
    HANDLE fileHandle;
    winCEfile *retval;
    wchar_t *w_fname=AscToUnicode(fname);

    fileHandle = CreateFile(w_fname, mode, FILE_SHARE_READ, NULL,
                            creation, FILE_ATTRIBUTE_NORMAL, NULL);

    free(w_fname);

    if(fileHandle==INVALID_HANDLE_VALUE)
    {
	return NULL;
    }

    BAIL_IF_MACRO(fileHandle == INVALID_HANDLE_VALUE, win32strerror(), NULL);

    retval = malloc(sizeof (winCEfile));
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
        HANDLE h = ((winCEfile *) retval)->handle;
        if (SetFilePointer(h, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER)
        {
            const char *err = win32strerror();
            CloseHandle(h);
            free(retval);
            BAIL_MACRO(err, NULL);
        } /* if */
    } /* if */

    return(retval);

} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HANDLE FileHandle = ((winCEfile *) opaque)->handle;
    DWORD CountOfBytesRead;
    PHYSFS_sint64 retval;

    /* Read data from the file */
    /*!!! - uint32 might be a greater # than DWORD */
    if(!ReadFile(FileHandle, buffer, count * size, &CountOfBytesRead, NULL))
    {
	retval=-1;
    } /* if */
    else
    {
        /* Return the number of "objects" read. */
        /* !!! - What if not the right amount of bytes was read to make an object? */
        retval = CountOfBytesRead / size;
    } /* else */

    return(retval);

} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HANDLE FileHandle = ((winCEfile *) opaque)->handle;
    DWORD CountOfBytesWritten;
    PHYSFS_sint64 retval;

    /* Read data from the file */
    /*!!! - uint32 might be a greater # than DWORD */
    if(!WriteFile(FileHandle, buffer, count * size, &CountOfBytesWritten, NULL))
    {
	retval=-1;
    } /* if */
    else
    {
        /* Return the number of "objects" read. */
        /*!!! - What if not the right number of bytes was written? */
        retval = CountOfBytesWritten / size;
    } /* else */

    return(retval);

} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    HANDLE FileHandle = ((winCEfile *) opaque)->handle;
    DWORD HighOrderPos;
    DWORD rc;

    /* Get the high order 32-bits of the position */
    //HighOrderPos = HIGHORDER_UINT64(pos);
    HighOrderPos = (unsigned long)(pos>>32);

    /*!!! SetFilePointer needs a signed 64-bit value. */
    /* Move pointer "pos" count from start of file */
    rc = SetFilePointer(FileHandle, (unsigned long)(pos&0x00000000ffffffff),
                        &HighOrderPos, FILE_BEGIN);

    if ((rc == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    {
        BAIL_MACRO(win32strerror(), 0);
    }

    return(1);  /* No error occured */
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    HANDLE FileHandle = ((winCEfile *) opaque)->handle;
    DWORD HighPos = 0;
    DWORD LowPos;
    PHYSFS_sint64 retval;

    /* Get current position */
    LowPos = SetFilePointer(FileHandle, 0, &HighPos, FILE_CURRENT);
    if ((LowPos == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    {
        BAIL_MACRO(win32strerror(), 0);
    } /* if */
    else
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = (((PHYSFS_uint64) HighPos) << 32) | LowPos;
        //assert(retval >= 0);
    } /* else */

    return(retval);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    HANDLE FileHandle = ((winCEfile *) opaque)->handle;
    DWORD SizeHigh;
    DWORD SizeLow;
    PHYSFS_sint64 retval;

    SizeLow = GetFileSize(FileHandle, &SizeHigh);
    if ((SizeLow == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    {
        BAIL_MACRO(win32strerror(), -1);
    } /* if */
    else
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = (((PHYSFS_uint64) SizeHigh) << 32) | SizeLow;
        //assert(retval >= 0);
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
    winCEfile *fh = ((winCEfile *) opaque);
    if (!fh->readonly)
        BAIL_IF_MACRO(!FlushFileBuffers(fh->handle), win32strerror(), 0);

    return(1);
} /* __PHYSFS_platformFlush */


int __PHYSFS_platformClose(void *opaque)
{
    HANDLE FileHandle = ((winCEfile *) opaque)->handle;
    BAIL_IF_MACRO(!CloseHandle(FileHandle), win32strerror(), 0);
    free(opaque);
    return(1);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    wchar_t *w_path=AscToUnicode(path);

    /* If filename is a folder */
    if (GetFileAttributes(w_path) == FILE_ATTRIBUTE_DIRECTORY)
    {
	int retval=!RemoveDirectory(w_path);
	free(w_path);
        BAIL_IF_MACRO(retval, win32strerror(), 0);
    } /* if */
    else
    {
	int retval=!DeleteFile(w_path);
	free(w_path);
        BAIL_IF_MACRO(retval, win32strerror(), 0);
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


PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformGetLastModTime */

/* end of skeleton.c ... */

