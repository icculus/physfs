/*
 * Unix support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

/* BeOS uses beos.cpp and posix.c ... Cygwin and such use win32.c ... */
#if ((!defined __BEOS__) && (!defined WIN32))

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <sys/mount.h>

#if (!defined PHYSFS_NO_PTHREADS_SUPPORT)
#include <pthread.h>
#endif

#ifdef PHYSFS_HAVE_SYS_UCRED_H
#  ifdef PHYSFS_HAVE_MNTENT_H
#    undef PHYSFS_HAVE_MNTENT_H /* don't do both... */
#  endif
#  include <sys/ucred.h>
#endif

#ifdef PHYSFS_HAVE_MNTENT_H
#include <mntent.h>
#endif

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


#ifdef PHYSFS_NO_CDROM_SUPPORT

/* Stub version for platforms without CD-ROM support. */
char **__PHYSFS_platformDetectAvailableCDs(void)
{
    char **retval = (char **) malloc(sizeof (char *));
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    *retval = NULL;
    return(retval);
} /* __PHYSFS_platformDetectAvailableCDs */

#else


#ifdef PHYSFS_HAVE_SYS_UCRED_H

char **__PHYSFS_platformDetectAvailableCDs(void)
{
    char **retval = (char **) malloc(sizeof (char *));
    int cd_count = 1;  /* We count the NULL entry. */
    struct statfs *mntbufp = NULL;
    int mounts;
    int i;

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    mounts = getmntinfo(&mntbufp, MNT_WAIT);

    for (i = 0; i < mounts; i++)
    {
        int add_it = 0;

        if (strcmp(mntbufp[i].f_fstypename, "iso9660") == 0)
            add_it = 1;
        else if (strcmp( mntbufp[i].f_fstypename, "cd9660") == 0)
            add_it = 1;

        /* add other mount types here */

        if (add_it)
        {
            char **tmp = realloc(retval, sizeof (char *) * (cd_count + 1));
            if (tmp)
            {
                retval = tmp;
                retval[cd_count - 1] = (char *)
                                malloc(strlen(mntbufp[i].f_mntonname) + 1);
                if (retval[cd_count - 1])
                {
                    strcpy(retval[cd_count - 1], mntbufp[i].f_mntonname);
                    cd_count++;
                } /* if */
            } /* if */
        } /* if */
    } /* for */

    retval[cd_count - 1] = NULL;
    return(retval);
} /* __PHYSFS_platformDetectAvailableCDs */

#endif


#ifdef PHYSFS_HAVE_MNTENT_H

char **__PHYSFS_platformDetectAvailableCDs(void)
{
    char **retval = (char **) malloc(sizeof (char *));
    int cd_count = 1;  /* We count the NULL entry. */
    FILE *mounts = NULL;
    struct mntent *ent = NULL;

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    *retval = NULL;
    mounts = setmntent("/etc/mtab", "r");
    BAIL_IF_MACRO(mounts == NULL, ERR_IO_ERROR, retval);

    while ( (ent = getmntent(mounts)) != NULL )
    {
        int add_it = 0;
        if (strcmp(ent->mnt_type, "iso9660") == 0)
            add_it = 1;

        /* add other mount types here */

        if (add_it)
        {
            char **tmp = realloc(retval, sizeof (char *) * (cd_count + 1));
            if (tmp)
            {
                retval = tmp;
                retval[cd_count-1] = (char *) malloc(strlen(ent->mnt_dir) + 1);
                if (retval[cd_count - 1])
                {
                    strcpy(retval[cd_count - 1], ent->mnt_dir);
                    cd_count++;
                } /* if */
            } /* if */
        } /* if */
    } /* while */

    endmntent(mounts);

    retval[cd_count - 1] = NULL;
    return(retval);
} /* __PHYSFS_platformDetectAvailableCDs */

#endif

#endif  /* !PHYSFS_NO_CDROM_SUPPORT */


/* this is in posix.c ... */
extern char *__PHYSFS_platformCopyEnvironmentVariable(const char *varname);


/*
 * See where program (bin) resides in the $PATH specified by (envr).
 *  returns a copy of the first element in envr that contains it, or NULL
 *  if it doesn't exist or there were other problems. PHYSFS_SetError() is
 *  called if we have a problem.
 *
 * (envr) will be scribbled over, and you are expected to free() the
 *  return value when you're done with it.
 */
static char *findBinaryInPath(const char *bin, char *envr)
{
    size_t alloc_size = 0;
    char *exe = NULL;
    char *start = envr;
    char *ptr;

    BAIL_IF_MACRO(bin == NULL, ERR_INVALID_ARGUMENT, NULL);
    BAIL_IF_MACRO(envr == NULL, ERR_INVALID_ARGUMENT, NULL);

    do
    {
        size_t size;
        ptr = strchr(start, ':');  /* find next $PATH separator. */
        if (ptr)
            *ptr = '\0';

        size = strlen(start) + strlen(bin) + 2;
        if (size > alloc_size)
        {
            char *x = (char *) realloc(exe, size);
            if (x == NULL)
            {
                if (exe != NULL)
                    free(exe);
                BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
            } /* if */

            alloc_size = size;
            exe = x;
        } /* if */

        /* build full binary path... */
        strcpy(exe, start);
        if (exe[strlen(exe) - 1] != '/')
            strcat(exe, "/");
        strcat(exe, bin);

        if (access(exe, X_OK) == 0)  /* Exists as executable? We're done. */
        {
            strcpy(exe, start);  /* i'm lazy. piss off. */
            return(exe);
        } /* if */

        start = ptr + 1;  /* start points to beginning of next element. */
    } while (ptr != NULL);

    if (exe != NULL)
        free(exe);

    return(NULL);  /* doesn't exist in path. */
} /* findBinaryInPath */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    /* If there isn't a path on argv0, then look through the $PATH for it. */

    char *retval;
    char *envr;

    if (strchr(argv0, '/') != NULL)   /* default behaviour can handle this. */
        return(NULL);

    envr = __PHYSFS_platformCopyEnvironmentVariable("PATH");
    BAIL_IF_MACRO(!envr, NULL, NULL);
    retval = findBinaryInPath(argv0, envr);
    free(envr);
    return(retval);
} /* __PHYSFS_platformCalcBaseDir */


/* Much like my college days, try to sleep for 10 milliseconds at a time... */
void __PHYSFS_platformTimeslice(void)
{
    usleep( 10 * 1000 );           /* don't care if it fails. */
} /* __PHYSFS_platformTimeslice */


char *__PHYSFS_platformRealPath(const char *path)
{
    char resolved_path[MAXPATHLEN];
    char *retval = NULL;

    errno = 0;
    BAIL_IF_MACRO(!realpath(path, resolved_path), strerror(errno), NULL);
    retval = (char *) malloc(strlen(resolved_path) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, resolved_path);
    return(retval);
} /* __PHYSFS_platformRealPath */


#if (!defined PHYSFS_NO_PTHREADS_SUPPORT)

PHYSFS_uint64 __PHYSFS_platformGetThreadID(void) { return(0x0001); }
void *__PHYSFS_platformCreateMutex(void) { return((void *) 0x0001); }
void __PHYSFS_platformDestroyMutex(void *mutex) {}
int __PHYSFS_platformGrabMutex(void *mutex) { return(1); }
void __PHYSFS_platformReleaseMutex(void *mutex) {}

#else

PHYSFS_uint64 __PHYSFS_platformGetThreadID(void)
{
    return((PHYSFS_uint64) pthread_self());
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    int rc;
    pthread_mutex_t *m = (pthread_mutex_t *) malloc(sizeof (pthread_mutex_t));
    BAIL_IF_MACRO(m == NULL, ERR_OUT_OF_MEMORY, NULL);
    rc = pthread_mutex_init(m, NULL);
    if (rc != 0)
    {
        free(m);
        BAIL_MACRO(strerror(rc), NULL);
    } /* if */

    return((void *) m);
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
    pthread_mutex_destroy((pthread_mutex_t *) mutex);
    free(mutex);
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    return(pthread_mutex_lock((pthread_mutex_t *) mutex) == 0);    
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
    pthread_mutex_unlock((pthread_mutex_t *) mutex);
} /* __PHYSFS_platformReleaseMutex */

#endif /* !PHYSFS_NO_PTHREADS_SUPPORT */


#endif /* !defined __BEOS__ && !defined WIN32 */

/* end of unix.c ... */

