/*
 * Unix support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if (defined __STRICT_ANSI__)
#define __PHYSFS_DOING_STRICT_ANSI__
#endif

/*
 * We cheat a little: I want the symlink version of stat() (lstat), and
 *  GCC/Linux will not declare it if compiled with the -ansi flag.
 *  If you are really lacking symlink support on your platform,
 *  you should #define __PHYSFS_NO_SYMLINKS__ before compiling this
 *  file. That will open a security hole, though, if you really DO have
 *  symlinks on your platform; it renders PHYSFS_permitSymbolicLinks(0)
 *  useless, since every symlink will be reported as a regular file/dir.
 */
#if (defined __PHYSFS_DOING_STRICT_ANSI__)
#undef __STRICT_ANSI__
#endif
#include <stdio.h>
#if (defined __PHYSFS_DOING_STRICT_ANSI__)
#define __STRICT_ANSI__
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <mntent.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


const char *__PHYSFS_platformDirSeparator = "/";

char **__PHYSFS_platformDetectAvailableCDs(void)
{
    char **retval = (char **) malloc(sizeof (char *));
    int cd_count = 1;  /* We count the NULL entry. */
    FILE *mounts = NULL;
    struct mntent *ent = NULL;

    *retval = NULL;
    mounts = setmntent("/etc/mtab", "r");
    BAIL_IF_MACRO(mounts == NULL, ERR_IO_ERROR, retval);

    while ( (ent = getmntent(mounts)) != NULL )
    {
        int add_it = 0;
        if (strcmp(ent->mnt_type, "iso9660") == 0)
            add_it = 1;
        /* !!! other mount types? */

        if (add_it)
        {
            char **tmp = realloc(retval, sizeof (char *) * cd_count + 1);
            if (tmp)
            {
                retval = tmp;
                retval[cd_count-1] = (char *) malloc(strlen(ent->mnt_dir) + 1);
                if (retval[cd_count-1])
                {
                    strcpy(retval[cd_count-1], ent->mnt_dir);
                    cd_count++;
                } /* if */
            } /* if */
        } /* if */
    } /* while */

    endmntent(mounts);

    retval[cd_count - 1] = NULL;
    return(retval);
} /* __PHYSFS_detectAvailableCDs */


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


/* !!! this is ugly. */
char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    /* If there isn't a path on argv0, then look through the $PATH for it. */

    char *retval = NULL;
    char *envr;
    char *start;
    char *ptr;
    char *exe;

    if (strchr(argv0, '/') != NULL)   /* default behaviour can handle this. */
        return(NULL);

    envr = copyEnvironmentVariable("PATH");
    BAIL_IF_MACRO(!envr, NULL, NULL);

    start = envr;
    do
    {
        ptr = strchr(start, ':');
        if (ptr)
            *ptr = '\0';

        exe = (char *) malloc(strlen(start) + strlen(argv0) + 2);
        if (!exe)
        {
            free(envr);
            BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
        } /* if */
        strcpy(exe, start);
        if (start[strlen(start) - 1] != '/')
            strcat(start, "/");
        strcat(start, argv0);
        if (access(exe, X_OK) != 0)
            free(exe);
        else
        {
            retval = exe;
            strcpy(retval, start);  /* i'm lazy. piss off. */
            break;
        } /* else */

        start = ptr + 1;
    } while (ptr != NULL);

    free(envr);
    return(retval);
} /* __PHYSFS_platformCalcBaseDir */


static char *getUserNameByUID(void)
{
    uid_t uid = getuid();
    struct passwd *pw;
    char *retval = NULL;

    pw = getpwuid(uid);
    if ((pw != NULL) && (pw->pw_name != NULL))
    {
        retval = malloc(strlen(pw->pw_name) + 1);
        if (retval != NULL)
            strcpy(retval, pw->pw_name);
    } /* if */
    
    return(retval);
} /* getUserNameByUID */


static char *getUserDirByUID(void)
{
    uid_t uid = getuid();
    struct passwd *pw;
    char *retval = NULL;

    pw = getpwuid(uid);
    if ((pw != NULL) && (pw->pw_dir != NULL))
    {
        retval = malloc(strlen(pw->pw_dir) + 1);
        if (retval != NULL)
            strcpy(retval, pw->pw_dir);
    } /* if */
    
    return(retval);
} /* getUserDirByUID */


char *__PHYSFS_platformGetUserName(void)
{
    char *retval = getUserNameByUID();
    if (retval == NULL)
        retval = copyEnvironmentVariable("USER");
    return(retval);
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    char *retval = copyEnvironmentVariable("HOME");
    if (retval == NULL)
        retval = getUserDirByUID();
    return(retval);
} /* __PHYSFS_platformGetUserDir */


int __PHYSFS_platformGetThreadID(void)
{
    return((int) pthread_self());
} /* __PHYSFS_platformGetThreadID */


/* -ansi and -pedantic flags prevent use of strcasecmp() on Linux. */
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
    struct stat statbuf;
    return(stat(fname, &statbuf) == 0);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
#if (defined __PHYSFS_NO_SYMLINKS__)
    return(0);
#else

    struct stat statbuf;
    int retval = 0;

    if (lstat(fname, &statbuf) == 0)
    {
        if (S_ISLNK(statbuf.st_mode))
            retval = 1;
    } /* if */
    
    return(retval);

#endif
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    struct stat statbuf;
    int retval = 0;

    if (stat(fname, &statbuf) == 0)
    {
        if (S_ISDIR(statbuf.st_mode))
            retval = 1;
    } /* if */
    
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

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    /* platform-independent notation is Unix-style already.  :)  */

    if (prepend)
        strcpy(retval, prepend);
    else
        retval[0] = '\0';

    strcat(retval, dirName);

    if (append)
        strcat(retval, append);

    return(retval);
} /* __PHYSFS_platformCvtToDependent */


/* Much like my college days, try to sleep for 10 milliseconds at a time... */
void __PHYSFS_platformTimeslice(void)
{
    struct timespec napTime;
    napTime.tv_sec = 0;
    napTime.tv_nsec = 10 * 1000 * 1000;  /* specified in nanoseconds. */
    nanosleep(&napTime, NULL);           /* don't care if it fails. */
} /* __PHYSFS_platformTimeslice */


LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks)
{
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;
    DIR *dir;
    struct dirent *ent;
    int bufsize = 0;
    char *buf = NULL;
    int dlen = 0;

    if (omitSymLinks)
    {
        dlen = strlen(dirname);
        bufsize = dlen + 256;
        buf = malloc(bufsize);
        BAIL_IF_MACRO(buf == NULL, ERR_OUT_OF_MEMORY, NULL);
        strcpy(buf, dirname);
        if (buf[dlen - 1] != '/')
        {
            buf[dlen++] = '/';
            buf[dlen] = '\0';
        } /* if */
    } /* if */

    errno = 0;
    dir = opendir(dirname);
    if (dir == NULL)
    {
        if (buf != NULL)
            free(buf);
        BAIL_IF_MACRO(1, strerror(errno), NULL);
    } /* if */

    while (1)
    {
        ent = readdir(dir);
        if (ent == NULL)   /* we're done. */
            break;

        if (strcmp(ent->d_name, ".") == 0)
            continue;

        if (strcmp(ent->d_name, "..") == 0)
            continue;

        if (omitSymLinks)
        {
            char *p;
            int len = strlen(ent->d_name) + dlen + 1;
            if (len > bufsize)
            {
                p = realloc(buf, len);
                if (p == NULL)
                    continue;
                buf = p;
                bufsize = len;
            } /* if */

            strcpy(buf + dlen, ent->d_name);
            if (__PHYSFS_platformIsSymLink(buf))
                continue;
        } /* if */

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l == NULL)
            break;

        l->str = (char *) malloc(strlen(ent->d_name) + 1);
        if (l->str == NULL)
        {
            free(l);
            break;
        } /* if */

        strcpy(l->str, ent->d_name);

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* while */

    if (buf != NULL)
        free(buf);

    closedir(dir);
    return(retval);
} /* __PHYSFS_platformEnumerateFiles */


int __PHYSFS_platformFileLength(FILE *handle)
{
    struct stat statbuf;
    errno = 0;
    BAIL_IF_MACRO(fstat(fileno(handle), &statbuf) == -1, strerror(errno), -1);
    return(statbuf.st_size);
} /* __PHYSFS_platformFileLength */


char *__PHYSFS_platformCurrentDir(void)
{
    int allocSize = 0;
    char *retval = NULL;
    char *ptr;

    do
    {
        allocSize += 100;
        ptr = (char *) realloc(retval, allocSize);
        if (ptr == NULL)
        {
            if (retval != NULL)
                free(retval);
            BAIL_IF_MACRO(1, ERR_OUT_OF_MEMORY, NULL);
        } /* if */

        retval = ptr;
        ptr = getcwd(retval, allocSize);
    } while (ptr == NULL);

    return(retval);
} /* __PHYSFS_platformCurrentDir */


char *__PHYSFS_platformRealPath(const char *path)
{
    char resolved_path[MAXPATHLEN];
    char *retval = NULL;

    errno = 0;
    BAIL_IF_MACRO(!realpath(path, resolved_path), strerror(errno), NULL);
    retval = malloc(strlen(resolved_path) + 1);
    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);
    strcpy(retval, resolved_path);
    return(retval);
} /* __PHYSFS_platformRealPath */

/* end of unix.c ... */

