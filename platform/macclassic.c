/*
 * MacOS Classic support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdlib.h>
#include <string.h>

/*
 * Please note that I haven't tried this code with CarbonLib or under
 *  MacOS X at all. The code in unix.c is known to work with Darwin,
 *  and you may or may not be better off using that.
 */
#ifdef __PHYSFS_CARBONIZED__   
#include <Carbon.h>
#else
#include <OSUtils.h>
#include <Processes.h>
#include <Files.h>
#include <TextUtils.h>
#include <Resources.h>
#include <MacMemory.h>
#include <Events.h>
#endif

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


const char *__PHYSFS_platformDirSeparator = ":";


int __PHYSFS_platformInit(void)
{
    return(1); /* always succeeds. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    return(1);  /* always succeed. */
} /* __PHYSFS_platformDeinit */


char **__PHYSFS_platformDetectAvailableCDs(void)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    char *ptr;
    char *retval = NULL;
    UInt32 retLength = 0;
    ProcessSerialNumber psn;
    struct ProcessInfoRec procInfo;
    FSSpec spec;
    CInfoPBRec infoPB;
    OSErr err;
    Str255 str255;
    
    /* Get the FSSpecPtr of the current process's binary... */
    BAIL_IF_MACRO(GetCurrentProcess(&psn) != noErr, ERR_OS_ERROR, NULL);
    memset(&procInfo, '\0', sizeof (procInfo));
    procInfo.processInfoLength = sizeof (procInfo);
    procInfo.processAppSpec = &spec;
    err = GetProcessInformation(&psn, &procInfo);
    BAIL_IF_MACRO(err != noErr, ERR_OS_ERROR, NULL);

    /* Get the name of the binary's parent directory. */
    memset(&infoPB, '\0', sizeof (CInfoPBRec));
    infoPB.dirInfo.ioNamePtr = str255;       /* put name in here.         */
    infoPB.dirInfo.ioVRefNum = spec.vRefNum;  /* ID of bin's volume.       */ 
    infoPB.dirInfo.ioDrParID = spec.parID;    /* ID of bin's dir.          */
    infoPB.dirInfo.ioFDirIndex = -1;          /* get dir (not file) info.  */

    /* walk the tree back to the root dir (volume), building path string... */
    do
    {
        /* check parent dir of what we last looked at... */
        infoPB.dirInfo.ioDrDirID = infoPB.dirInfo.ioDrParID;
        if (PBGetCatInfoAsync(&infoPB) != noErr)
        {
            if (retval != NULL)
                free(retval);
            BAIL_MACRO(ERR_OS_ERROR, NULL);
        } /* if */
        
        /* allocate more space for the retval... */
        retLength += str255[0] + 1; /* + 1 for a ':' or null char... */
        ptr = (char *) malloc(retLength);
        if (ptr == NULL)
        {
            if (retval != NULL)
                free(retval);
            BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
        } /* if */

        /* prepend new dir to retval and cleanup... */
        memcpy(ptr, &str255[1], str255[0]);
        ptr[str255[0]] = '\0';  /* null terminate it. */
        if (retval != NULL)
        {
            strcat(ptr, ":");
            strcat(ptr, retval);
            free(retval);
        } /* if */
        retval = ptr;
    } while (infoPB.dirInfo.ioDrDirID != fsRtDirID);

    return(retval);
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformGetUserName(void)
{
    char *retval = NULL;
    StringHandle strHandle;
    short origResourceFile = CurResFile();

    /* use the System resource file. */
    UseResFile(0);
    /* apparently, -16096 specifies the username. */
    strHandle = GetString(-16096);
    UseResFile(origResourceFile);
    BAIL_IF_MACRO(strHandle == NULL, ERR_OS_ERROR, NULL);

    HLock((Handle) strHandle);
    retval = (char *) malloc((*strHandle)[0] + 1);
    if (retval == NULL)
    {
        HUnlock((Handle) strHandle);
        BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
    } /* if */
    memcpy(retval, &(*strHandle)[1], (*strHandle)[0]);
    retval[(*strHandle)[0]] = '\0';  /* null-terminate it. */
    HUnlock((Handle) strHandle);

    return(retval);
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    return(NULL);  /* bah...use default behaviour, I guess. */
} /* __PHYSFS_platformGetUserDir */


PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    return(1);  /* single threaded. */
} /* __PHYSFS_platformGetThreadID */


int __PHYSFS_platformStricmp(const char *x, const char *y)
{
    extern int _stricmp(const char *, const char *);
    return(_stricmp(x, y));  /* (*shrug*) */
} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformExists(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
    return(0);  /* !!! FIXME: What happens if (fname) is an alias? */
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformIsDirectory */


char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append)
{
    int len = ((prepend) ? strlen(prepend) : 0) +
              ((append) ? strlen(append) : 0) +
              strlen(dirName) + 1;
    const char *src;
    char *dst;
    char *retval = malloc(len);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    if (prepend != NULL)
    {
        strcpy(retval, prepend);
        dst = retval + strlen(retval);
    } /* if */
    else
    {
        *retval = '\0';
        dst = retval;
    } /* else */

    for (src = dirName; *src; src++, dst++)
        *dst = ((*src == '/') ? ':' : *src);

    *dst = '\0';
    return(retval);
} /* __PHYSFS_platformCvtToDependent */


void __PHYSFS_platformTimeslice(void)
{
    SystemTask();
} /* __PHYSFS_platformTimeslice */


LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformEnumerateFiles */


char *__PHYSFS_platformCurrentDir(void)
{
    /*
     * I don't think MacOS has a concept of "current directory", beyond
     *  what is grafted on by a given standard C library implementation,
     *  so just return the base dir.
     * We don't use this for anything crucial at the moment anyhow.
     */
    return(__PHYSFS_platformCalcBaseDir(NULL));
} /* __PHYSFS_platformCurrentDir */


char *__PHYSFS_platformRealPath(const char *path)
{
    /* !!! FIXME: This isn't nearly right. */
    char *retval = (char *) malloc(strlen(path) + 1);
    strcpy(retval, path);
    return(retval);
} /* __PHYSFS_platformRealPath */


int __PHYSFS_platformMkDir(const char *path)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformMkDir */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, NULL);
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformEOF(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, -1);
} /* __PHYSFS_platformEOF */


int __PHYSFS_platformFlush(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformFlush */


int __PHYSFS_platformClose(void *opaque)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    BAIL_MACRO(ERR_NOT_IMPLEMENTED, 0);
} /* __PHYSFS_platformDelete */


void *__PHYSFS_platformCreateMutex(void)
{
    return((void *) 0x0001);  /* no mutexes on MacOS Classic. */
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    /* no mutexes on MacOS Classic. */
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    return(1);  /* no mutexes on MacOS Classic. */
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    /* no mutexes on MacOS Classic. */
} /* __PHYSFS_platformReleaseMutex */

/* end of unix.c ... */

