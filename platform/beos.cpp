/*
 * BeOS platform-dependent support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef __BEOS__

#include <be/kernel/OS.h>
#include <be/app/Roster.h>
#include <be/storage/Volume.h>
#include <be/storage/VolumeRoster.h>
#include <be/storage/Directory.h>
#include <be/storage/Entry.h>
#include <be/storage/Path.h>
#include <be/kernel/fs_info.h>
#include <be/device/scsi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


const char *__PHYSFS_platformDirSeparator = "/";


int __PHYSFS_platformInit(void)
{
    return(1);  /* always succeed. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    return(1);  /* always succeed. */
} /* __PHYSFS_platformDeinit */



/* caller needs to malloc() mntpnt, and expect us to free() it. */
static void addDisc(char *mntpnt, char ***discs, int *disccount)
{
    char **tmp = (char **) realloc(*discs, sizeof (char *) * (*disccount + 1));
    if (tmp)
    {
        tmp[*disccount - 1] = mntpnt;
        *discs = tmp;
        (*disccount)++;
    } /* if */
} /* addDisc */


static char *getMountPoint(const char *devname)
{
    BVolumeRoster mounts;
    BVolume vol;

    mounts.Rewind();
    while (mounts.GetNextVolume(&vol) == B_NO_ERROR)
    {
        fs_info fsinfo;
        fs_stat_dev(vol.Device(), &fsinfo);
        if (strcmp(devname, fsinfo.device_name) == 0)
        {
            //char buf[B_FILE_NAME_LENGTH];
            BDirectory directory;
            BEntry entry;
            BPath path;
            status_t rc;
            rc = vol.GetRootDirectory(&directory);
            BAIL_IF_MACRO(rc < B_OK, strerror(rc), NULL);
            rc = directory.GetEntry(&entry);
            BAIL_IF_MACRO(rc < B_OK, strerror(rc), NULL);
            rc = entry.GetPath(&path);
            BAIL_IF_MACRO(rc < B_OK, strerror(rc), NULL);
            const char *str = path.Path();
            BAIL_IF_MACRO(str == NULL, ERR_OS_ERROR, NULL);  /* ?! */
            char *retval = (char *) malloc(strlen(str) + 1);
            BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
            strcpy(retval, str);
            return(retval);
        } /* if */
    } /* while */

    return(NULL);
} /* getMountPoint */


    /*
     * This function is lifted from Simple Directmedia Layer (SDL):
     *  http://www.libsdl.org/
     */
static void tryDir(const char *dirname, char ***discs, int *disccount)
{
    BDirectory dir;
    dir.SetTo(dirname);
    if (dir.InitCheck() != B_NO_ERROR)
        return;

    dir.Rewind();
    BEntry entry;
    while (dir.GetNextEntry(&entry) >= 0)
    {
        BPath path;
        const char *name;
        entry_ref e;

        if (entry.GetPath(&path) != B_NO_ERROR)
            continue;

        name = path.Path();

        if (entry.GetRef(&e) != B_NO_ERROR)
            continue;

        if (entry.IsDirectory())
        {
            if (strcmp(e.name, "floppy") != 0)
                tryDir(name, discs, disccount);
        } /* if */

        else
        {
            bool add_it = false;
            int devfd;
            device_geometry g;

            if (strcmp(e.name, "raw") == 0)  /* ignore partitions. */
            {
                int devfd = open(name, O_RDONLY);
                if (devfd >= 0)
                {
                    if (ioctl(devfd, B_GET_GEOMETRY, &g, sizeof(g)) >= 0)
                    {
                        if (g.device_type == B_CD)
                        {
                            char *mntpnt = getMountPoint(name);
                            if (mntpnt != NULL)
                                addDisc(mntpnt, discs, disccount);
                        } /* if */
                    } /* if */
                } /* if */
            } /* if */

            close(devfd);
        } /* else */
    } /* while */
} /* tryDir */


char **__PHYSFS_platformDetectAvailableCDs(void)
{
    char **retval = (char **) malloc(sizeof (char *));
    int cd_count = 1;  /* We count the NULL entry. */
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    tryDir("/dev/disk", &retval, &cd_count);
    retval[cd_count - 1] = NULL;
    return(retval);
} /* __PHYSFS_platformDetectAvailableCDs */


static team_id getTeamID(void)
{
    thread_info info;
    thread_id tid = find_thread(NULL);
    get_thread_info(tid, &info);
    return(info.team);
} /* getMyTeamID */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    /* in case there isn't a BApplication yet, we'll construct a roster. */
    BRoster roster; 
    app_info info;
    status_t rc = roster.GetRunningAppInfo(getTeamID(), &info);
    BAIL_IF_MACRO(rc < B_OK, strerror(rc), NULL);
    BEntry entry(&(info.ref), true);
    BPath path;
    rc = entry.GetPath(&path);  /* (path) now has binary's path. */
    assert(rc == B_OK);
    rc = path.GetParent(&path); /* chop filename, keep directory. */
    assert(rc == B_OK);
    const char *str = path.Path();
    assert(str != NULL);
    char *retval = (char *) malloc(strlen(str) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, str);
    return(retval);
} /* __PHYSFS_platformCalcBaseDir */


PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    return((PHYSFS_uint64) find_thread(NULL));
} /* __PHYSFS_platformGetThreadID */


/* Much like my college days, try to sleep for 10 milliseconds at a time... */
void __PHYSFS_platformTimeslice(void)
{
    snooze(10000);  /* put thread to sleep for 10 milliseconds. */
} /* __PHYSFS_platformTimeslice */


char *__PHYSFS_platformRealPath(const char *path)
{
    char *str = (char *) alloca(strlen(path) + 1);
    BAIL_IF_MACRO(str == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(str, path);
    char *leaf = strrchr(str, '/');
    if (leaf != NULL)
        *(leaf++) = '\0';

    BPath normalized(str, leaf, true);  /* force normalization of path. */
    const char *resolved_path = normalized.Path();
    BAIL_IF_MACRO(resolved_path == NULL, ERR_NO_SUCH_FILE, NULL);
    char *retval = (char *) malloc(strlen(resolved_path) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, resolved_path);
    return(retval);
} /* __PHYSFS_platformRealPath */


void *__PHYSFS_platformCreateMutex(void)
{
    sem_id *retval = (sem_id *) malloc(sizeof (sem_id));
    sem_id rc;

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    rc = create_sem(1, "PhysicsFS semaphore");
    if (rc < B_OK)
    {
        free(retval);
        BAIL_MACRO(strerror(rc), NULL);
    } // if

    *retval = rc;
    return(retval);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    delete_sem( *((sem_id *) mutex) );
    free(mutex);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    status_t rc = acquire_sem(*((sem_id *) mutex));
    BAIL_IF_MACRO(rc < B_OK, strerror(rc), 0);
    return(1);
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    release_sem(*((sem_id *) mutex));
} /* __PHYSFS_platformReleaseMutex */

#endif

/* end of beos.cpp ... */

