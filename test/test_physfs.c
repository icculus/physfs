/**
 * Test program for PhysicsFS. May only work on Unix.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define _CRT_SECURE_NO_WARNINGS 1

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#if (defined __MWERKS__)
#include <SIOUX.h>
#endif

#if (defined PHYSFS_HAVE_READLINE)
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <time.h>

/* Define this, so the compiler doesn't complain about using old APIs. */
#define PHYSFS_DEPRECATED

#include "physfs.h"

#define TEST_VERSION_MAJOR  3
#define TEST_VERSION_MINOR  0
#define TEST_VERSION_PATCH  1

static FILE *history_file = NULL;
static PHYSFS_uint32 do_buffer_size = 0;

static void output_versions(void)
{
    PHYSFS_Version compiled;
    PHYSFS_Version linked;

    PHYSFS_VERSION(&compiled);
    PHYSFS_getLinkedVersion(&linked);

    printf("test_physfs version %d.%d.%d.\n"
           " Compiled against PhysicsFS version %d.%d.%d,\n"
           " and linked against %d.%d.%d.\n\n",
            TEST_VERSION_MAJOR, TEST_VERSION_MINOR, TEST_VERSION_PATCH,
            (int) compiled.major, (int) compiled.minor, (int) compiled.patch,
            (int) linked.major, (int) linked.minor, (int) linked.patch);
} /* output_versions */


static void output_archivers(void)
{
    const PHYSFS_ArchiveInfo **rc = PHYSFS_supportedArchiveTypes();
    const PHYSFS_ArchiveInfo **i;

    printf("Supported archive types:\n");
    if (*rc == NULL)
        printf(" * Apparently, NONE!\n");
    else
    {
        for (i = rc; *i != NULL; i++)
        {
            printf(" * %s: %s\n    Written by %s.\n    %s\n",
                    (*i)->extension, (*i)->description,
                    (*i)->author, (*i)->url);
            printf("    %s symbolic links.\n",
                    (*i)->supportsSymlinks ? "Supports" : "Does not support");
        } /* for */
    } /* else */

    printf("\n");
} /* output_archivers */


static int cmd_quit(char *args)
{
    return 0;
} /* cmd_quit */


static int cmd_init(char *args)
{
    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    if (PHYSFS_init(args))
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_init */


static int cmd_deinit(char *args)
{
    if (PHYSFS_deinit())
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_deinit */


static int cmd_addarchive(char *args)
{
    char *ptr = strrchr(args, ' ');
    int appending = atoi(ptr + 1);
    *ptr = '\0';

    if (*args == '\"')
    {
        args++;
        *(ptr - 1) = '\0';
    } /* if */

    /*printf("[%s], [%d]\n", args, appending);*/

    if (PHYSFS_mount(args, NULL, appending))
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_addarchive */


/* wrap free() to avoid calling convention wankery. */
static void freeBuf(void *buf)
{
    free(buf);
} /* freeBuf */

typedef enum
{
    MNTTYPE_PATH,
    MNTTYPE_MEMORY,
    MNTTYPE_HANDLE
} MountType;

static int cmd_mount_internal(char *args, const MountType mnttype)
{
    char *ptr;
    char *mntpoint = NULL;
    int appending = 0;
    int rc = 0;

    if (*args == '\"')
    {
        args++;
        ptr = strchr(args, '\"');
        if (ptr == NULL)
        {
            printf("missing string terminator in argument.\n");
            return 1;
        } /* if */
        *(ptr) = '\0';
    } /* if */
    else
    {
        ptr = strchr(args, ' ');
        *ptr = '\0';
    } /* else */

    mntpoint = ptr + 1;
    if (*mntpoint == '\"')
    {
        mntpoint++;
        ptr = strchr(mntpoint, '\"');
        if (ptr == NULL)
        {
            printf("missing string terminator in argument.\n");
            return 1;
        } /* if */
        *(ptr) = '\0';
    } /* if */
    else
    {
        ptr = strchr(mntpoint, ' ');
        *(ptr) = '\0';
    } /* else */
    appending = atoi(ptr + 1);

    /*printf("[%s], [%s], [%d]\n", args, mntpoint, appending);*/

    if (mnttype == MNTTYPE_PATH)
        rc = PHYSFS_mount(args, mntpoint, appending);

    else if (mnttype == MNTTYPE_HANDLE)
    {
        PHYSFS_File *f = PHYSFS_openRead(args);
        if (f == NULL)
        {
            printf("PHYSFS_openRead('%s') failed. reason: %s.\n", args, PHYSFS_getLastError());
            return 1;
        } /* if */

        rc = PHYSFS_mountHandle(f, args, mntpoint, appending);
        if (!rc)
            PHYSFS_close(f);
    } /* else if */

    else if (mnttype == MNTTYPE_MEMORY)
    {
        FILE *in = fopen(args, "rb");
        void *buf = NULL;
        long len = 0;

        if (in == NULL)
        {
            printf("Failed to open %s to read into memory: %s.\n", args, strerror(errno));
            return 1;
        } /* if */

        if ( (fseek(in, 0, SEEK_END) != 0) || ((len = ftell(in)) < 0) )
        {
            printf("Failed to find size of %s to read into memory: %s.\n", args, strerror(errno));
            fclose(in);
            return 1;
        } /* if */

        buf = malloc(len);
        if (buf == NULL)
        {
            printf("Failed to allocate space to read %s into memory: %s.\n", args, strerror(errno));
            fclose(in);
            return 1;
        } /* if */

        if ((fseek(in, 0, SEEK_SET) != 0) || (fread(buf, len, 1, in) != 1))
        {
            printf("Failed to read %s into memory: %s.\n", args, strerror(errno));
            fclose(in);
            free(buf);
            return 1;
        } /* if */

        fclose(in);

        rc = PHYSFS_mountMemory(buf, len, freeBuf, args, mntpoint, appending);
    } /* else */

    if (rc)
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_mount_internal */


static int cmd_mount(char *args)
{
    return cmd_mount_internal(args, MNTTYPE_PATH);
} /* cmd_mount */


static int cmd_mount_mem(char *args)
{
    return cmd_mount_internal(args, MNTTYPE_MEMORY);
} /* cmd_mount_mem */


static int cmd_mount_handle(char *args)
{
    return cmd_mount_internal(args, MNTTYPE_HANDLE);
} /* cmd_mount_handle */

static int cmd_getmountpoint(char *args)
{
    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    printf("Dir [%s] is mounted at [%s].\n", args, PHYSFS_getMountPoint(args));
    return 1;
} /* cmd_getmountpoint */

static int cmd_removearchive(char *args)
{
    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    if (PHYSFS_unmount(args))
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_removearchive */


static int cmd_enumerate(char *args)
{
    char **rc;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    rc = PHYSFS_enumerateFiles(args);

    if (rc == NULL)
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());
    else
    {
        int file_count;
        char **i;
        for (i = rc, file_count = 0; *i != NULL; i++, file_count++)
            printf("%s\n", *i);

        printf("\n total (%d) files.\n", file_count);
        PHYSFS_freeList(rc);
    } /* else */

    return 1;
} /* cmd_enumerate */


static int cmd_getdirsep(char *args)
{
    printf("Directory separator is [%s].\n", PHYSFS_getDirSeparator());
    return 1;
} /* cmd_getdirsep */


static int cmd_getlasterror(char *args)
{
    printf("last error is [%s].\n", PHYSFS_getLastError());
    return 1;
} /* cmd_getlasterror */


static int cmd_getcdromdirs(char *args)
{
    char **rc = PHYSFS_getCdRomDirs();

    if (rc == NULL)
        printf("Failure. Reason: [%s].\n", PHYSFS_getLastError());
    else
    {
        int dir_count;
        char **i;
        for (i = rc, dir_count = 0; *i != NULL; i++, dir_count++)
            printf("%s\n", *i);

        printf("\n total (%d) drives.\n", dir_count);
        PHYSFS_freeList(rc);
    } /* else */

    return 1;
} /* cmd_getcdromdirs */


static int cmd_getsearchpath(char *args)
{
    char **rc = PHYSFS_getSearchPath();

    if (rc == NULL)
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());
    else
    {
        int dir_count;
        char **i;
        for (i = rc, dir_count = 0; *i != NULL; i++, dir_count++)
            printf("%s\n", *i);

        printf("\n total (%d) directories.\n", dir_count);
        PHYSFS_freeList(rc);
    } /* else */

    return 1;
} /* cmd_getcdromdirs */


static int cmd_getbasedir(char *args)
{
    printf("Base dir is [%s].\n", PHYSFS_getBaseDir());
    return 1;
} /* cmd_getbasedir */


static int cmd_getuserdir(char *args)
{
    printf("User dir is [%s].\n", PHYSFS_getUserDir());
    return 1;
} /* cmd_getuserdir */


static int cmd_getprefdir(char *args)
{
    char *org;
    char *appName;
    char *ptr = args;

    org = ptr;
    ptr = strchr(ptr, ' '); *ptr = '\0'; ptr++; appName = ptr;
    printf("Pref dir is [%s].\n", PHYSFS_getPrefDir(org, appName));
    return 1;
} /* cmd_getprefdir */


static int cmd_getwritedir(char *args)
{
    printf("Write dir is [%s].\n", PHYSFS_getWriteDir());
    return 1;
} /* cmd_getwritedir */


static int cmd_setwritedir(char *args)
{
    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    if (PHYSFS_setWriteDir(args))
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_setwritedir */


static int cmd_permitsyms(char *args)
{
    int num;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    num = atoi(args);
    PHYSFS_permitSymbolicLinks(num);
    printf("Symlinks are now %s.\n", num ? "permitted" : "forbidden");
    return 1;
} /* cmd_permitsyms */


static int cmd_setbuffer(char *args)
{
    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    do_buffer_size = (unsigned int) atoi(args);
    if (do_buffer_size)
    {
        printf("Further tests will set a (%lu) size buffer.\n",
                (unsigned long) do_buffer_size);
    } /* if */

    else
    {
        printf("Further tests will NOT use a buffer.\n");
    } /* else */

    return 1;
} /* cmd_setbuffer */


static int cmd_stressbuffer(char *args)
{
    int num;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    num = atoi(args);
    if (num < 0)
        printf("buffer must be greater than or equal to zero.\n");
    else
    {
        PHYSFS_File *f;
        int rndnum;

        printf("Stress testing with (%d) byte buffer...\n", num);
        f = PHYSFS_openWrite("test.txt");
        if (f == NULL)
            printf("Couldn't open test.txt for writing: %s.\n", PHYSFS_getLastError());
        else
        {
            int i, j;
            char buf[37];
            char buf2[37];

            if (!PHYSFS_setBuffer(f, num))
            {
                printf("PHYSFS_setBuffer() failed: %s.\n", PHYSFS_getLastError());
                PHYSFS_close(f);
                PHYSFS_delete("test.txt");
                return 1;
            } /* if */

            strcpy(buf, "abcdefghijklmnopqrstuvwxyz0123456789");
            srand((unsigned int) time(NULL));

            for (i = 0; i < 10; i++)
            {
                for (j = 0; j < 10000; j++)
                {
                    PHYSFS_uint32 right = 1 + (PHYSFS_uint32) (35.0 * rand() / (RAND_MAX + 1.0));
                    PHYSFS_uint32 left = 36 - right;
                    if (PHYSFS_writeBytes(f, buf, left) != left)
                    {
                        printf("PHYSFS_writeBytes() failed: %s.\n", PHYSFS_getLastError());
                        PHYSFS_close(f);
                        return 1;
                    } /* if */

                    rndnum = 1 + (int) (1000.0 * rand() / (RAND_MAX + 1.0));
                    if (rndnum == 42)
                    {
                        if (!PHYSFS_flush(f))
                        {
                            printf("PHYSFS_flush() failed: %s.\n", PHYSFS_getLastError());
                            PHYSFS_close(f);
                            return 1;
                        } /* if */
                    } /* if */

                    if (PHYSFS_writeBytes(f, buf + left, right) != right)
                    {
                        printf("PHYSFS_writeBytes() failed: %s.\n", PHYSFS_getLastError());
                        PHYSFS_close(f);
                        return 1;
                    } /* if */

                    rndnum = 1 + (int) (1000.0 * rand() / (RAND_MAX + 1.0));
                    if (rndnum == 42)
                    {
                        if (!PHYSFS_flush(f))
                        {
                            printf("PHYSFS_flush() failed: %s.\n", PHYSFS_getLastError());
                            PHYSFS_close(f);
                            return 1;
                        } /* if */
                    } /* if */
                } /* for */

                if (!PHYSFS_flush(f))
                {
                    printf("PHYSFS_flush() failed: %s.\n", PHYSFS_getLastError());
                    PHYSFS_close(f);
                    return 1;
                } /* if */

            } /* for */

            if (!PHYSFS_close(f))
            {
                printf("PHYSFS_close() failed: %s.\n", PHYSFS_getLastError());
                return 1;  /* oh well. */
            } /* if */

            printf(" ... test file written ...\n");
            f = PHYSFS_openRead("test.txt");
            if (f == NULL)
            {
                printf("Failed to reopen stress file for reading: %s.\n", PHYSFS_getLastError());
                return 1;
            } /* if */

            if (!PHYSFS_setBuffer(f, num))
            {
                printf("PHYSFS_setBuffer() failed: %s.\n", PHYSFS_getLastError());
                PHYSFS_close(f);
                return 1;
            } /* if */

            for (i = 0; i < 10; i++)
            {
                for (j = 0; j < 10000; j++)
                {
                    PHYSFS_uint32 right = 1 + (PHYSFS_uint32) (35.0 * rand() / (RAND_MAX + 1.0));
                    PHYSFS_uint32 left = 36 - right;
                    if (PHYSFS_readBytes(f, buf2, left) != left)
                    {
                        printf("PHYSFS_readBytes() failed: %s.\n", PHYSFS_getLastError());
                        PHYSFS_close(f);
                        return 1;
                    } /* if */

                    rndnum = 1 + (int) (1000.0 * rand() / (RAND_MAX + 1.0));
                    if (rndnum == 42)
                    {
                        if (!PHYSFS_flush(f))
                        {
                            printf("PHYSFS_flush() failed: %s.\n", PHYSFS_getLastError());
                            PHYSFS_close(f);
                            return 1;
                        } /* if */
                    } /* if */

                    if (PHYSFS_readBytes(f, buf2 + left, right) != right)
                    {
                        printf("PHYSFS_readBytes() failed: %s.\n", PHYSFS_getLastError());
                        PHYSFS_close(f);
                        return 1;
                    } /* if */

                    rndnum = 1 + (int) (1000.0 * rand() / (RAND_MAX + 1.0));
                    if (rndnum == 42)
                    {
                        if (!PHYSFS_flush(f))
                        {
                            printf("PHYSFS_flush() failed: %s.\n", PHYSFS_getLastError());
                            PHYSFS_close(f);
                            return 1;
                        } /* if */
                    } /* if */

                    if (memcmp(buf, buf2, 36) != 0)
                    {
                        printf("readback is mismatched on iterations (%d, %d).\n", i, j);
                        printf("wanted: [");
                        for (i = 0; i < 36; i++)
                            printf("%c", buf[i]);
                        printf("]\n");

                        printf("   got: [");
                        for (i = 0; i < 36; i++)
                            printf("%c", buf2[i]);
                        printf("]\n");
                        PHYSFS_close(f);
                        return 1;
                    } /* if */
                } /* for */

                if (!PHYSFS_flush(f))
                {
                    printf("PHYSFS_flush() failed: %s.\n", PHYSFS_getLastError());
                    PHYSFS_close(f);
                    return 1;
                } /* if */

            } /* for */

            printf(" ... test file read ...\n");

            if (!PHYSFS_eof(f))
                printf("PHYSFS_eof() returned true! That's wrong.\n");

            if (!PHYSFS_close(f))
            {
                printf("PHYSFS_close() failed: %s.\n", PHYSFS_getLastError());
                return 1;  /* oh well. */
            } /* if */

            PHYSFS_delete("test.txt");
            printf("stress test completed successfully.\n");
        } /* else */
    } /* else */

    return 1;
} /* cmd_stressbuffer */


static int cmd_setsaneconfig(char *args)
{
    char *org;
    char *appName;
    char *arcExt;
    int inclCD;
    int arcsFirst;
    char *ptr = args;

        /* ugly. */
    org = ptr;
    ptr = strchr(ptr, ' '); *ptr = '\0'; ptr++; appName = ptr;
    ptr = strchr(ptr, ' '); *ptr = '\0'; ptr++; arcExt = ptr;
    ptr = strchr(ptr, ' '); *ptr = '\0'; ptr++; inclCD = atoi(arcExt);
    arcsFirst = atoi(ptr);

    if (strcmp(arcExt, "!") == 0)
        arcExt = NULL;

    if (PHYSFS_setSaneConfig(org, appName, arcExt, inclCD, arcsFirst))
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_setsaneconfig */


static int cmd_mkdir(char *args)
{
    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    if (PHYSFS_mkdir(args))
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_mkdir */


static int cmd_delete(char *args)
{
    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    if (PHYSFS_delete(args))
        printf("Successful.\n");
    else
        printf("Failure. reason: %s.\n", PHYSFS_getLastError());

    return 1;
} /* cmd_delete */


static int cmd_getrealdir(char *args)
{
    const char *rc;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    rc = PHYSFS_getRealDir(args);
    if (rc)
        printf("Found at [%s].\n", rc);
    else
        printf("Not found.\n");

    return 1;
} /* cmd_getrealdir */


static int cmd_exists(char *args)
{
    int rc;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    rc = PHYSFS_exists(args);
    printf("File %sexists.\n", rc ? "" : "does not ");
    return 1;
} /* cmd_exists */


static int cmd_isdir(char *args)
{
    PHYSFS_Stat statbuf;
    int rc;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    rc = PHYSFS_stat(args, &statbuf);
    if (rc)
        rc = (statbuf.filetype == PHYSFS_FILETYPE_DIRECTORY);
    printf("File %s a directory.\n", rc ? "is" : "is NOT");
    return 1;
} /* cmd_isdir */


static int cmd_issymlink(char *args)
{
    PHYSFS_Stat statbuf;
    int rc;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    rc = PHYSFS_stat(args, &statbuf);
    if (rc)
        rc = (statbuf.filetype == PHYSFS_FILETYPE_SYMLINK);
    printf("File %s a symlink.\n", rc ? "is" : "is NOT");
    return 1;
} /* cmd_issymlink */


static int cmd_cat(char *args)
{
    PHYSFS_File *f;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    f = PHYSFS_openRead(args);
    if (f == NULL)
        printf("failed to open. Reason: [%s].\n", PHYSFS_getLastError());
    else
    {
        if (do_buffer_size)
        {
            if (!PHYSFS_setBuffer(f, do_buffer_size))
            {
                printf("failed to set file buffer. Reason: [%s].\n",
                        PHYSFS_getLastError());
                PHYSFS_close(f);
                return 1;
            } /* if */
        } /* if */

        while (1)
        {
            char buffer[128];
            PHYSFS_sint64 rc;
            PHYSFS_sint64 i;
            rc = PHYSFS_readBytes(f, buffer, sizeof (buffer));

            for (i = 0; i < rc; i++)
                fputc((int) buffer[i], stdout);

            if (rc < sizeof (buffer))
            {
                printf("\n\n");
                if (!PHYSFS_eof(f))
                {
                    printf("\n (Error condition in reading. Reason: [%s])\n\n",
                           PHYSFS_getLastError());
                } /* if */
                PHYSFS_close(f);
                return 1;
            } /* if */
        } /* while */
    } /* else */

    return 1;
} /* cmd_cat */

static int cmd_cat2(char *args)
{
    PHYSFS_File *f1 = NULL;
    PHYSFS_File *f2 = NULL;
    char *fname1;
    char *fname2;
    char *ptr;

    fname1 = args;
    if (*fname1 == '\"')
    {
        fname1++;
        ptr = strchr(fname1, '\"');
        if (ptr == NULL)
        {
            printf("missing string terminator in argument.\n");
            return 1;
        } /* if */
        *(ptr) = '\0';
    } /* if */
    else
    {
        ptr = strchr(fname1, ' ');
        *ptr = '\0';
    } /* else */

    fname2 = ptr + 1;
    if (*fname2 == '\"')
    {
        fname2++;
        ptr = strchr(fname2, '\"');
        if (ptr == NULL)
        {
            printf("missing string terminator in argument.\n");
            return 1;
        } /* if */
        *(ptr) = '\0';
    } /* if */

    if ((f1 = PHYSFS_openRead(fname1)) == NULL)
        printf("failed to open '%s'. Reason: [%s].\n", fname1, PHYSFS_getLastError());
    else if ((f2 = PHYSFS_openRead(fname2)) == NULL)
        printf("failed to open '%s'. Reason: [%s].\n", fname2, PHYSFS_getLastError());
    else
    {
        char *buffer1 = NULL;
        size_t buffer1len = 0;
        char *buffer2 = NULL;
        size_t buffer2len = 0;
        char *ptr = NULL;
        size_t i;

        if (do_buffer_size)
        {
            if (!PHYSFS_setBuffer(f1, do_buffer_size))
            {
                printf("failed to set file buffer for '%s'. Reason: [%s].\n",
                        fname1, PHYSFS_getLastError());
                PHYSFS_close(f1);
                PHYSFS_close(f2);
                return 1;
            } /* if */
            else if (!PHYSFS_setBuffer(f2, do_buffer_size))
            {
                printf("failed to set file buffer for '%s'. Reason: [%s].\n",
                        fname2, PHYSFS_getLastError());
                PHYSFS_close(f1);
                PHYSFS_close(f2);
                return 1;
            } /* if */
        } /* if */


        do
        {
            int readlen = 128;
            PHYSFS_sint64 rc;

            ptr = realloc(buffer1, buffer1len + readlen);
            if (!ptr)
            {
                printf("(Out of memory.)\n\n");
                free(buffer1);
                free(buffer2);
                PHYSFS_close(f1);
                PHYSFS_close(f2);
                return 1;
            } /* if */

            buffer1 = ptr;
            rc = PHYSFS_readBytes(f1, buffer1 + buffer1len, readlen);
            if (rc < 0)
            {
                printf("(Error condition in reading '%s'. Reason: [%s])\n\n",
                       fname1, PHYSFS_getLastError());
                free(buffer1);
                free(buffer2);
                PHYSFS_close(f1);
                PHYSFS_close(f2);
                return 1;
            } /* if */
            buffer1len += (size_t) rc;

            ptr = realloc(buffer2, buffer2len + readlen);
            if (!ptr)
            {
                printf("(Out of memory.)\n\n");
                free(buffer1);
                free(buffer2);
                PHYSFS_close(f1);
                PHYSFS_close(f2);
                return 1;
            } /* if */

            buffer2 = ptr;
            rc = PHYSFS_readBytes(f2, buffer2 + buffer2len, readlen);
            if (rc < 0)
            {
                printf("(Error condition in reading '%s'. Reason: [%s])\n\n",
                       fname2, PHYSFS_getLastError());
                free(buffer1);
                free(buffer2);
                PHYSFS_close(f1);
                PHYSFS_close(f2);
                return 1;
            } /* if */
            buffer2len += (size_t) rc;
        } while (!PHYSFS_eof(f1) || !PHYSFS_eof(f2));

        printf("file '%s' ...\n\n", fname1);
        for (i = 0; i < buffer1len; i++)
            fputc((int) buffer1[i], stdout);
        free(buffer1);

        printf("\n\nfile '%s' ...\n\n", fname2);
        for (i = 0; i < buffer2len; i++)
            fputc((int) buffer2[i], stdout);
        free(buffer2);

        printf("\n\n");
    } /* else */

    if (f1)
        PHYSFS_close(f1);

    if (f2)
        PHYSFS_close(f2);

    return 1;
} /* cmd_cat2 */


#define CRC32_BUFFERSIZE 512
static int cmd_crc32(char *args)
{
    PHYSFS_File *f;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    f = PHYSFS_openRead(args);
    if (f == NULL)
        printf("failed to open. Reason: [%s].\n", PHYSFS_getLastError());
    else
    {
        PHYSFS_uint8 buffer[CRC32_BUFFERSIZE];
        PHYSFS_uint32 crc = -1;
        PHYSFS_sint64 bytesread;

        while ((bytesread = PHYSFS_readBytes(f, buffer, CRC32_BUFFERSIZE)) > 0)
        {
            PHYSFS_uint32 i, bit;
            for (i = 0; i < bytesread; i++)
            {
                for (bit = 0; bit < 8; bit++, buffer[i] >>= 1)
                    crc = (crc >> 1) ^ (((crc ^ buffer[i]) & 1) ? 0xEDB88320 : 0);
            } /* for */
        } /* while */

        if (bytesread < 0)
        {
            printf("error while reading. Reason: [%s].\n",
                   PHYSFS_getLastError());
            return 1;
        } /* if */

        PHYSFS_close(f);

        crc ^= -1;
        printf("CRC32 for %s: 0x%08X\n", args, crc);
    } /* else */

    return 1;
} /* cmd_crc32 */


static int cmd_filelength(char *args)
{
    PHYSFS_File *f;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    f = PHYSFS_openRead(args);
    if (f == NULL)
        printf("failed to open. Reason: [%s].\n", PHYSFS_getLastError());
    else
    {
        PHYSFS_sint64 len = PHYSFS_fileLength(f);
        if (len == -1)
            printf("failed to determine length. Reason: [%s].\n", PHYSFS_getLastError());
        else
            printf(" (cast to int) %d bytes.\n", (int) len);

        PHYSFS_close(f);
    } /* else */

    return 1;
} /* cmd_filelength */

#define WRITESTR "The cat sat on the mat.\n\n"

static int cmd_append(char *args)
{
    PHYSFS_File *f;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    f = PHYSFS_openAppend(args);
    if (f == NULL)
        printf("failed to open. Reason: [%s].\n", PHYSFS_getLastError());
    else
    {
        size_t bw;
        PHYSFS_sint64 rc;

        if (do_buffer_size)
        {
            if (!PHYSFS_setBuffer(f, do_buffer_size))
            {
                printf("failed to set file buffer. Reason: [%s].\n",
                        PHYSFS_getLastError());
                PHYSFS_close(f);
                return 1;
            } /* if */
        } /* if */

        bw = strlen(WRITESTR);
        rc = PHYSFS_writeBytes(f, WRITESTR, bw);
        if (rc != bw)
        {
            printf("Wrote (%d) of (%d) bytes. Reason: [%s].\n",
                   (int) rc, (int) bw, PHYSFS_getLastError());
        } /* if */
        else
        {
            printf("Successful.\n");
        } /* else */

        PHYSFS_close(f);
    } /* else */

    return 1;
} /* cmd_append */


static int cmd_write(char *args)
{
    PHYSFS_File *f;

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    f = PHYSFS_openWrite(args);
    if (f == NULL)
        printf("failed to open. Reason: [%s].\n", PHYSFS_getLastError());
    else
    {
        size_t bw;
        PHYSFS_sint64 rc;

        if (do_buffer_size)
        {
            if (!PHYSFS_setBuffer(f, do_buffer_size))
            {
                printf("failed to set file buffer. Reason: [%s].\n",
                        PHYSFS_getLastError());
                PHYSFS_close(f);
                return 1;
            } /* if */
        } /* if */

        bw = strlen(WRITESTR);
        rc = PHYSFS_writeBytes(f, WRITESTR, bw);
        if (rc != bw)
        {
            printf("Wrote (%d) of (%d) bytes. Reason: [%s].\n",
                   (int) rc, (int) bw, PHYSFS_getLastError());
        } /* if */
        else
        {
            printf("Successful.\n");
        } /* else */

        PHYSFS_close(f);
    } /* else */

    return 1;
} /* cmd_write */


static char* modTimeToStr(PHYSFS_sint64 modtime, char *modstr, size_t strsize)
{
    if (modtime < 0)
        strncpy(modstr, "Unknown\n", strsize);
    else
    {
        time_t t = (time_t) modtime;
        char *str = ctime(&t);
        strncpy(modstr, str, strsize);
    } /* else */

    modstr[strsize-1] = '\0';
    return modstr;
} /* modTimeToStr */


static int cmd_getlastmodtime(char *args)
{
    PHYSFS_Stat statbuf;
    if (!PHYSFS_stat(args, &statbuf))
        printf("Failed to determine. Reason: [%s].\n", PHYSFS_getLastError());
    else
    {
        char modstr[64];
        modTimeToStr(statbuf.modtime, modstr, sizeof (modstr));
        printf("Last modified: %s (%ld).\n", modstr, (long) statbuf.modtime);
    } /* else */

    return 1;
} /* cmd_getLastModTime */

static int cmd_stat(char *args)
{
    PHYSFS_Stat stat;
    char timestring[65];

    if (*args == '\"')
    {
        args++;
        args[strlen(args) - 1] = '\0';
    } /* if */

    if(!PHYSFS_stat(args, &stat))
    {
        printf("failed to stat. Reason [%s].\n", PHYSFS_getLastError());
        return 1;
    } /* if */

    printf("Filename: %s\n", args);
    printf("Size %d\n",(int) stat.filesize);

    if(stat.filetype == PHYSFS_FILETYPE_REGULAR)
        printf("Type: File\n");
    else if(stat.filetype == PHYSFS_FILETYPE_DIRECTORY)
        printf("Type: Directory\n");
    else if(stat.filetype == PHYSFS_FILETYPE_SYMLINK)
        printf("Type: Symlink\n");
    else
        printf("Type: Unknown\n");

    printf("Created at: %s", modTimeToStr(stat.createtime, timestring, 64));
    printf("Last modified at: %s", modTimeToStr(stat.modtime, timestring, 64));
    printf("Last accessed at: %s", modTimeToStr(stat.accesstime, timestring, 64));
    printf("Readonly: %s\n", stat.readonly ? "true" : "false");

    return 1;
} /* cmd_filelength */



/* must have spaces trimmed prior to this call. */
static int count_args(const char *str)
{
    int retval = 0;
    int in_quotes = 0;

    if (str != NULL)
    {
        for (; *str != '\0'; str++)
        {
            if (*str == '\"')
                in_quotes = !in_quotes;
            else if ((*str == ' ') && (!in_quotes))
                retval++;
        } /* for */
        retval++;
    } /* if */

    return retval;
} /* count_args */


static int cmd_help(char *args);

typedef struct
{
    const char *cmd;
    int (*func)(char *args);
    int argcount;
    const char *usage;
} command_info;

static const command_info commands[] =
{
    { "quit",           cmd_quit,           0, NULL                         },
    { "q",              cmd_quit,           0, NULL                         },
    { "help",           cmd_help,           0, NULL                         },
    { "init",           cmd_init,           1, "<argv0>"                    },
    { "deinit",         cmd_deinit,         0, NULL                         },
    { "addarchive",     cmd_addarchive,     2, "<archiveLocation> <append>" },
    { "mount",          cmd_mount,          3, "<archiveLocation> <mntpoint> <append>" },
    { "mountmem",       cmd_mount_mem,      3, "<archiveLocation> <mntpoint> <append>" },
    { "mounthandle",    cmd_mount_handle,   3, "<archiveLocation> <mntpoint> <append>" },
    { "removearchive",  cmd_removearchive,  1, "<archiveLocation>"          },
    { "unmount",        cmd_removearchive,  1, "<archiveLocation>"          },
    { "enumerate",      cmd_enumerate,      1, "<dirToEnumerate>"           },
    { "ls",             cmd_enumerate,      1, "<dirToEnumerate>"           },
    { "getlasterror",   cmd_getlasterror,   0, NULL                         },
    { "getdirsep",      cmd_getdirsep,      0, NULL                         },
    { "getcdromdirs",   cmd_getcdromdirs,   0, NULL                         },
    { "getsearchpath",  cmd_getsearchpath,  0, NULL                         },
    { "getbasedir",     cmd_getbasedir,     0, NULL                         },
    { "getuserdir",     cmd_getuserdir,     0, NULL                         },
    { "getprefdir",     cmd_getprefdir,     2, "<org> <app>"                },
    { "getwritedir",    cmd_getwritedir,    0, NULL                         },
    { "setwritedir",    cmd_setwritedir,    1, "<newWriteDir>"              },
    { "permitsymlinks", cmd_permitsyms,     1, "<1or0>"                     },
    { "setsaneconfig",  cmd_setsaneconfig,  5, "<org> <appName> <arcExt> <includeCdRoms> <archivesFirst>" },
    { "mkdir",          cmd_mkdir,          1, "<dirToMk>"                  },
    { "delete",         cmd_delete,         1, "<dirToDelete>"              },
    { "getrealdir",     cmd_getrealdir,     1, "<fileToFind>"               },
    { "exists",         cmd_exists,         1, "<fileToCheck>"              },
    { "isdir",          cmd_isdir,          1, "<fileToCheck>"              },
    { "issymlink",      cmd_issymlink,      1, "<fileToCheck>"              },
    { "cat",            cmd_cat,            1, "<fileToCat>"                },
    { "cat2",           cmd_cat2,           2, "<fileToCat1> <fileToCat2>"  },
    { "filelength",     cmd_filelength,     1, "<fileToCheck>"              },
    { "stat",           cmd_stat,           1, "<fileToStat>"               },
    { "append",         cmd_append,         1, "<fileToAppend>"             },
    { "write",          cmd_write,          1, "<fileToCreateOrTrash>"      },
    { "getlastmodtime", cmd_getlastmodtime, 1, "<fileToExamine>"            },
    { "setbuffer",      cmd_setbuffer,      1, "<bufferSize>"               },
    { "stressbuffer",   cmd_stressbuffer,   1, "<bufferSize>"               },
    { "crc32",          cmd_crc32,          1, "<fileToHash>"               },
    { "getmountpoint",  cmd_getmountpoint,  1, "<dir>"                      },
    { NULL,             NULL,              -1, NULL                         }
};


static void output_usage(const char *intro, const command_info *cmdinfo)
{
    if (cmdinfo->argcount == 0)
        printf("%s \"%s\" (no arguments)\n", intro, cmdinfo->cmd);
    else
        printf("%s \"%s %s\"\n", intro, cmdinfo->cmd, cmdinfo->usage);
} /* output_usage */


static int cmd_help(char *args)
{
    const command_info *i;

    printf("Commands:\n");
    for (i = commands; i->cmd != NULL; i++)
        output_usage("  -", i);

    return 1;
} /* output_cmd_help */


static void trim_command(const char *orig, char *copy)
{
    const char *i;
    char *writeptr = copy;
    int spacecount = 0;
    int have_first = 0;

    for (i = orig; *i != '\0'; i++)
    {
        if (*i == ' ')
        {
            if ((*(i + 1) != ' ') && (*(i + 1) != '\0'))
            {
                if ((have_first) && (!spacecount))
                {
                    spacecount++;
                    *writeptr = ' ';
                    writeptr++;
                } /* if */
            } /* if */
        } /* if */
        else
        {
            have_first = 1;
            spacecount = 0;
            *writeptr = *i;
            writeptr++;
        } /* else */
    } /* for */

    *writeptr = '\0';

    /*
    printf("\n command is [%s].\n", copy);
    */
} /* trim_command */


static int process_command(char *complete_cmd)
{
    const command_info *i;
    char *cmd_copy;
    char *args;
    int rc = 1;

    if (complete_cmd == NULL)  /* can happen if user hits CTRL-D, etc. */
    {
        printf("\n");
        return 0;
    } /* if */

    cmd_copy = (char *) malloc(strlen(complete_cmd) + 1);
    if (cmd_copy == NULL)
    {
        printf("\n\n\nOUT OF MEMORY!\n\n\n");
        return 0;
    } /* if */

    trim_command(complete_cmd, cmd_copy);
    args = strchr(cmd_copy, ' ');
    if (args != NULL)
    {
        *args = '\0';
        args++;
    } /* else */

    if (cmd_copy[0] != '\0')
    {
        for (i = commands; i->cmd != NULL; i++)
        {
            if (strcmp(i->cmd, cmd_copy) == 0)
            {
                if ((i->argcount >= 0) && (count_args(args) != i->argcount))
                    output_usage("usage:", i);
                else
                    rc = i->func(args);
                break;
            } /* if */
        } /* for */

        if (i->cmd == NULL)
            printf("Unknown command. Enter \"help\" for instructions.\n");

#if (defined PHYSFS_HAVE_READLINE)
        add_history(complete_cmd);
        if (history_file)
        {
            fprintf(history_file, "%s\n", complete_cmd);
            fflush(history_file);
        } /* if */
#endif

    } /* if */

    free(cmd_copy);
    return rc;
} /* process_command */


static void open_history_file(void)
{
#if (defined PHYSFS_HAVE_READLINE)
#if 0
    const char *envr = getenv("TESTPHYSFS_HISTORY");
    if (!envr)
        return;
#else
    char envr[256];
    strcpy(envr, PHYSFS_getUserDir());
    strcat(envr, ".testphys_history");
#endif

    if (access(envr, F_OK) == 0)
    {
        char buf[512];
        FILE *f = fopen(envr, "r");
        if (!f)
        {
            printf("\n\n"
                   "Could not open history file [%s] for reading!\n"
                   "  Will not have past history available.\n\n",
                    envr);
            return;
        } /* if */

        do
        {
            if (fgets(buf, sizeof (buf), f) == NULL)
                break;

            if (buf[strlen(buf) - 1] == '\n')
                buf[strlen(buf) - 1] = '\0';
            add_history(buf);
        } while (!feof(f));

        fclose(f);
    } /* if */

    history_file = fopen(envr, "ab");
    if (!history_file)
    {
        printf("\n\n"
               "Could not open history file [%s] for appending!\n"
               "  Will not be able to record this session's history.\n\n",
                envr);
    } /* if */
#endif
} /* open_history_file */


int main(int argc, char **argv)
{
    char *buf = NULL;
    int rc = 0;

#if (defined __MWERKS__)
    extern tSIOUXSettings SIOUXSettings;
    SIOUXSettings.asktosaveonclose = 0;
    SIOUXSettings.autocloseonquit = 1;
    SIOUXSettings.rows = 40;
    SIOUXSettings.columns = 120;
#endif

    printf("\n");

    if (!PHYSFS_init(argv[0]))
    {
        printf("PHYSFS_init() failed!\n  reason: %s.\n", PHYSFS_getLastError());
        return 1;
    } /* if */

    output_versions();
    output_archivers();

    open_history_file();

    printf("Enter commands. Enter \"help\" for instructions.\n");
    fflush(stdout);

    do
    {
#if (defined PHYSFS_HAVE_READLINE)
        buf = readline("> ");
#else
        int i;
        buf = (char *) malloc(512);
        memset(buf, '\0', 512);
        printf("> ");
        fflush(stdout);
        for (i = 0; i < 511; i++)
        {
            int ch = fgetc(stdin);
            if (ch == EOF)
            {
                strcpy(buf, "quit");
                break;
            } /* if */
            else if ((ch == '\n') || (ch == '\r'))
            {
                buf[i] = '\0';
                break;
            } /* else if */
            else if (ch == '\b')
            {
                if (i > 0)
                    i--;
            } /* else if */
            else
            {
                buf[i] = (char) ch;
            } /* else */
        } /* for */
#endif

        rc = process_command(buf);
        fflush(stdout);
        if (buf != NULL)
            free(buf);
    } while (rc);

    if (!PHYSFS_deinit())
        printf("PHYSFS_deinit() failed!\n  reason: %s.\n", PHYSFS_getLastError());

    if (history_file)
        fclose(history_file);

/*
    printf("\n\ntest_physfs written by ryan c. gordon.\n");
    printf(" it makes you shoot teh railgun bettar.\n");
*/

    return 0;
} /* main */

/* end of test_physfs.c ... */

