/*
 * Win32 support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if (defined __STRICT_ANSI__)
#define __PHYSFS_DOING_STRICT_ANSI__
#endif

#include <windows.h>
#include <userenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#define LOWORDER_UINT64(pos)       (PHYSFS_uint32)(pos & 0x00000000FFFFFFFF)
#define HIGHORDER_UINT64(pos)      (PHYSFS_uint32)(pos & 0xFFFFFFFF00000000)

const char *__PHYSFS_platformDirSeparator = "\\";

static HANDLE ProcessHandle = NULL;     /* Current process handle */
static HANDLE AccessTokenHandle = NULL; /* Security handle to process */
static DWORD ProcessID;                 /* ID assigned to current process */
static int runningNT;                   /* TRUE if NT derived OS */
static OSVERSIONINFO OSVersionInfo;     /* Information about the OS */

/* NT specific information */
static char *ProfileDirectory = NULL;   /* User profile folder */

static const char *win32strerror(void)
{
    static TCHAR msgbuf[255];

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

    return((const char *) msgbuf);
} /* win32strerror */


char **__PHYSFS_platformDetectAvailableCDs(void)
{
    char **retval = (char **) malloc(sizeof (char *));
    int cd_count = 1;  /* We count the NULL entry. */
    char drive_str[4] = "x:\\";

    for (drive_str[0] = 'A'; drive_str[0] <= 'Z'; drive_str[0]++)
    {
        if (GetDriveType(drive_str) == DRIVE_CDROM)
        {
            char **tmp = realloc(retval, sizeof (char *) * cd_count + 1);
            if (tmp)
            {
                retval = tmp;
                retval[cd_count-1] = (char *) malloc(4);
                if (retval[cd_count-1])
                {
                    strcpy(retval[cd_count-1], drive_str);
                    cd_count++;
                } /* if */
            } /* if */
        } /* if */
    } /* for */

    retval[cd_count - 1] = NULL;
    return(retval);
} /* __PHYSFS_detectAvailableCDs */


static char *getExePath(const char *argv0)
{
    char *filepart = NULL;
    char *retval = (char *) malloc(sizeof (TCHAR) * (MAX_PATH + 1));
    DWORD buflen = GetModuleFileName(NULL, retval, MAX_PATH + 1);
    retval[buflen] = '\0';  /* does API always null-terminate the string? */

        /* make sure the string was not truncated. */
    if (__PHYSFS_platformStricmp(&retval[buflen - 4], ".exe") == 0)
    {
        char *ptr = strrchr(retval, '\\');
        if (ptr != NULL)
        {
            *(ptr + 1) = '\0';  /* chop off filename. */

            /* free up the bytes we didn't actually use. */            
            retval = (char *) realloc(retval, strlen(retval) + 1);
            if (retval != NULL)
                return(retval);
        } /* if */
    } /* if */

    /* if any part of the previous approach failed, try SearchPath()... */
    buflen = SearchPath(NULL, argv0, NULL, buflen, NULL, NULL);
    retval = (char *) realloc(retval, buflen);
    BAIL_IF_MACRO(!retval, ERR_OUT_OF_MEMORY, NULL);
    SearchPath(NULL, argv0, NULL, buflen, retval, &filepart);

    return(retval);
} /* getExePath */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    if (strchr(argv0, '\\') != NULL)   /* default behaviour can handle this. */
        return(NULL);

    return(getExePath(argv0));
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformGetUserName(void)
{
    DWORD bufsize = 0;
    LPTSTR retval = NULL;

    if (GetUserName(NULL, &bufsize) == 0)  /* This SHOULD fail. */
    {
        retval = (LPTSTR) malloc(bufsize);
        BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
        if (GetUserName(retval, &bufsize) == 0)  /* ?! */
        {
            free(retval);
            retval = NULL;
        } /* if */
    } /* if */

    return((char *) retval);
} /* __PHYSFS_platformGetUserName */


static char *copyEnvironmentVariable(const char *varname)
{
    const char *envr = getenv(varname);
    char *retval = NULL;

    if (envr != NULL)
    {
        retval = malloc(strlen(envr) + 1);
        if (retval != NULL)
            strcpy(retval, envr);
    } /* if */

    return(retval);
} /* copyEnvironmentVariable */


char *__PHYSFS_platformGetUserDir(void)
{
    char *userdir = NULL;

    if(runningNT)
    {
        userdir = ProfileDirectory;
    }
    else
    {
        /*!!!TODO - Need to return something for Win9x/ME */
    }

    return userdir;
} /* __PHYSFS_platformGetUserDir */


PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    return((PHYSFS_uint64)GetCurrentThreadId());
} /* __PHYSFS_platformGetThreadID */


/* ...make this Cygwin AND Visual C friendly... */
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
    return(GetFileAttributes(fname) != 0xffffffff);
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


/* Much like my college days, try to sleep for 10 milliseconds at a time... */
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

    dir = FindFirstFile(dirname, &ent);
    BAIL_IF_MACRO(dir == INVALID_HANDLE_VALUE, win32strerror(), NULL);

    while (FindNextFile(dir, &ent) != 0)
    {
        if (strcmp(ent.cFileName, ".") == 0)
            continue;

        if (strcmp(ent.cFileName, "..") == 0)
            continue;

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l == NULL)
            break;

        l->str = (char *) malloc(strlen(ent.cFileName) + 1);
        if (l->str == NULL)
        {
            free(l);
            break;
        } /* if */

        strcpy(l->str, ent.cFileName);

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* while */

    FindClose(dir);
    return(retval);
} /* __PHYSFS_platformEnumerateFiles */


char *__PHYSFS_platformCurrentDir(void)
{
    LPTSTR retval;
    DWORD buflen = 0;

    buflen = GetCurrentDirectory(buflen, NULL);
    retval = (LPTSTR) malloc(sizeof (TCHAR) * (buflen + 2));
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

    retval = (char *) malloc(MAX_PATH);
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
            free(retval);
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

        free(currentDir);
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
    p = (char *) realloc(retval, strlen(retval) + 1);
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
 * Initialize any NT specific stuff.  This includes any OS based on NT.
 *
 * Return zero if there was a catastrophic failure and non-zero otherwise.
 */
static int doNTInit()
{
    DWORD pathsize = 0;
    char TempProfileDirectory[1];

    /* Create a process access token handle */
    if(!OpenProcessToken(ProcessHandle, TOKEN_QUERY, &AccessTokenHandle))
    {
        /* Access token is required by other win32 functions */
        return 0;
    }

    /* Should fail.  Will write the size of the profile path in pathsize*/
    /*!!! Second parameter can't be NULL or the function fails??? */
    if(!GetUserProfileDirectory(AccessTokenHandle, TempProfileDirectory, &pathsize))
    {
        const char *temp;
        temp = win32strerror();

        /* Allocate memory for the profile directory */
        ProfileDirectory = (char *)malloc(pathsize);
        BAIL_IF_MACRO(ProfileDirectory == NULL, ERR_OUT_OF_MEMORY, 0);
        /* Try to get the profile directory */
        if(!GetUserProfileDirectory(AccessTokenHandle, ProfileDirectory, &pathsize))
        {
            free(ProfileDirectory);
            return 0;
        }
    }

    /* Everything initialized okay */
    return 1;
}

/* 
 * Get OS info and save it.
 *
 * Returns non-zero if successful, otherwise it returns zero on failure.
 */
int getOSInfo(void)
{
    /* Get OS info */
    OSVersionInfo.dwOSVersionInfoSize = sizeof(OSVersionInfo);
    if(!GetVersionEx(&OSVersionInfo))
    {
        return 0;
    }

    /* Set to TRUE if we are runnign a WinNT based OS 4.0 or greater */
    runningNT = (OSVersionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) &&
        (OSVersionInfo.dwMajorVersion > 3);

    return 1;
}

int __PHYSFS_platformInit(void)
{
    if(!getOSInfo())
    {
        return 0;
    }

    /* Get Windows ProcessID associated with the current process */
    ProcessID = GetCurrentProcessId();
    /* Create a process handle associated with the current process ID */
    ProcessHandle = GetCurrentProcess();

    if(ProcessHandle == NULL)
    {
        /* Process handle is required by other win32 functions */
        return 0;
    }

    /* If running an NT system (NT/Win2k/XP, etc...) */
    if(runningNT)
    {
        if(!doNTInit())
        {
            /* Error initializing NT stuff */
            return 0;
        }
    }

    /* It's all good */
    return 1;
}

/*
 * Uninitialize any NT specific stuff done in doNTInit().
 *
 * Return zero if there was a catastrophic failure and non-zero otherwise.
 */
static int doNTDeinit()
{
    if(CloseHandle(AccessTokenHandle) != S_OK)
    {
        return 0;
    }

    free(ProfileDirectory);

    /* It's all good */
    return 1;
}

int __PHYSFS_platformDeinit(void)
{
    if(!doNTDeinit())
        return 0;

    if(CloseHandle(ProcessHandle) != S_OK)
        return 0;

    /* It's all good */
    return 1;
}

void *__PHYSFS_platformOpenRead(const char *filename)
{
    HANDLE FileHandle;
    
    /* Open an existing file for read only. File can be opened by others
       who request read access on the file only. */
    FileHandle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, 
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    /* If CreateFile() failed */
    if(FileHandle == INVALID_HANDLE_VALUE)
        return NULL;

    return (void *)FileHandle;
}

void *__PHYSFS_platformOpenWrite(const char *filename)
{
    HANDLE FileHandle;
    
    /* Open an existing file for write only.  File can be opened by others
       who request read access to the file only */
    FileHandle = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL, 
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    /* If CreateFile() failed */
    if(FileHandle == INVALID_HANDLE_VALUE)
        return NULL;

    return (void *)FileHandle;
}

void *__PHYSFS_platformOpenAppend(const char *filename)
{
    HANDLE FileHandle;
    
    /* Open an existing file for appending only.  File can be opened by others
       who request read access to the file only. */
    FileHandle = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_READ, NULL, 
        TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    /* If CreateFile() failed */
    if(FileHandle == INVALID_HANDLE_VALUE)
        /* CreateFile might have failed because the file doesn't exist, so
           we'll just create a new file for writing then */
        return __PHYSFS_platformOpenWrite(filename);

    return (void *)FileHandle;
}

PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HANDLE FileHandle;
    DWORD CountOfBytesRead;
    PHYSFS_sint64 retval;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)opaque;

    /* Read data from the file */
    /*!!! - uint32 might be a greater # than DWORD */
    if(!ReadFile(FileHandle, buffer, count * size, &CountOfBytesRead, NULL))
    {
        /* Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());
        /* We errored out */
        retval = -1;
    }
    else
    {
        /* Return the number of "objects" read. */
        /* !!! - What if not the right amount of bytes was read to make an object? */
        retval = CountOfBytesRead / size;
    }

    return retval;
}

PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    HANDLE FileHandle;
    DWORD CountOfBytesWritten;
    PHYSFS_sint64 retval;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)opaque;

    /* Read data from the file */
    /*!!! - uint32 might be a greater # than DWORD */
    if(!WriteFile(FileHandle, buffer, count * size, &CountOfBytesWritten, NULL))
    {
        /* Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());
        /* We errored out */
        retval = -1;
    }
    else
    {
        /* Return the number of "objects" read. */
        /*!!! - What if not the right number of bytes was written? */
        retval = CountOfBytesWritten / size;
    }

    return retval;
}

int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    HANDLE FileHandle;
    int retval;
    DWORD HighOrderPos;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)opaque;

    /* Get the high order 32-bits of the position */
    HighOrderPos = HIGHORDER_UINT64(pos);

    /*!!! SetFilePointer needs a signed 64-bit value. */
    /* Move pointer "pos" count from start of file */
    if((SetFilePointer(FileHandle, LOWORDER_UINT64(pos), &HighOrderPos, FILE_BEGIN)
        == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    {
        /* An error occured.  Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());

        retval = 0;
    }
    else
    {
        /* No error occured */
        retval = 1;
    }

    return retval;
}

PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    HANDLE FileHandle;
    DWORD HighOrderPos = 0; 
    DWORD LowOrderPos;
    PHYSFS_sint64 retval;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)opaque;

    /* Get current position */
    if(((LowOrderPos = SetFilePointer(FileHandle, 0, &HighOrderPos, FILE_CURRENT))
        == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = HighOrderPos;
        retval = retval << 32;
        retval |= LowOrderPos;
    }
    else
    {
        /* Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());
        /* We errored out */
        retval = 0;
    }

    /*!!! Can't find a file pointer routine?!?!?!!?!?*/
    return retval;
}

PHYSFS_sint64 __PHYSFS_platformFileLength(void *handle)
{
    HANDLE FileHandle;
    DWORD FileSizeHigh;
    DWORD FileSizeLow;
    PHYSFS_sint64 retval;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)handle;
    
    if(((FileSizeLow = GetFileSize(FileHandle, &FileSizeHigh))
        == INVALID_SET_FILE_POINTER) && (GetLastError() != NO_ERROR))
    {
        /* Combine the high/low order to create the 64-bit position value */
        retval = FileSizeHigh;
        retval = retval << 32;
        retval |= FileSizeLow;
    }
    else
    {
        /* Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());

        retval = -1;
    }

    return retval;
}

int __PHYSFS_platformEOF(void *opaque)
{
    HANDLE FileHandle;
    PHYSFS_sint64 FilePosition;
    int retval = 0;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)opaque;

    /* Get the current position in the file */
    if((FilePosition = __PHYSFS_platformTell(opaque)) != 0)
    {
        /* Non-zero if EOF is equal to the file length - 1 */
        retval = FilePosition == __PHYSFS_platformFileLength(opaque) - 1;
    }

    return retval;
}

int __PHYSFS_platformFlush(void *opaque)
{
    HANDLE FileHandle;
    int retval;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)opaque;

    /* Close the file */
    if(!(retval = FlushFileBuffers(FileHandle)))
    {
        /* Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());
    }

    return retval;
}

int __PHYSFS_platformClose(void *opaque)
{
    HANDLE FileHandle;
    int retval;

    /* Cast the generic handle to a Win32 handle */
    FileHandle = (HANDLE)opaque;

    /* Close the file */
    if(!(retval = CloseHandle(FileHandle)))
    {
        /* Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());
    }

    return retval;
}

/*
 * Remove a file or directory entry in the actual filesystem. (path) is
 *  specified in platform-dependent notation. Note that this deletes files
 *  _and_ directories, so you might need to do some determination.
 *  Non-empty directories should report an error and not delete themselves
 *  or their contents.
 *
 * Deleting a symlink should remove the link, not what it points to.
 *
 * On error, return zero and set the error message. Return non-zero on success.
 */
int __PHYSFS_platformDelete(const char *path)
{
    int retval;

    /* If filename is a folder */
    if(GetFileAttributes(path) == FILE_ATTRIBUTE_DIRECTORY)
    {
        retval = RemoveDirectory(path);
    }
    else
    {
        retval = DeleteFile(path);
    }

    if(!retval)
    {
            /* Set the error to GetLastError */
        __PHYSFS_setError(win32strerror());
    }

    return retval;
}

void *__PHYSFS_platformCreateMutex(void)
{
    return (void *)CreateMutex(NULL, FALSE, NULL);
}

void __PHYSFS_platformDestroyMutex(void *mutex)
{
    CloseHandle((HANDLE)mutex);
}

int __PHYSFS_platformGrabMutex(void *mutex)
{
    int retval;

    if(WaitForSingleObject((HANDLE)mutex, INFINITE) == WAIT_FAILED)
    {
        /* Our wait failed for some unknown reason */
        retval = 1;
    }
    else
    {
        /* Good to go */
        retval = 0;
    }

    return retval;
}

void __PHYSFS_platformReleaseMutex(void *mutex)
{
    ReleaseMutex((HANDLE)mutex);
}

/* end of win32.c ... */

