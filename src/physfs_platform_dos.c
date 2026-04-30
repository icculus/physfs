/*
 * MS-DOS support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_DOS

#include <dirent.h>
#include <sys/types.h>
#include <sys/farptr.h>
#include <dpmi.h>
#include <go32.h>

#include "physfs_internal.h"

int __PHYSFS_platformInit(const char *argv0)
{
    return 1;  /* always succeed. */
} /* __PHYSFS_platformInit */


void __PHYSFS_platformDeinit(void)
{
    /* no-op */
} /* __PHYSFS_platformDeinit */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
#if (defined PHYSFS_NO_CDROM_SUPPORT)
    /* no-op. */
#else
    /* MSCDEX API calls go through interrupt 0x2F with AH=0x15: https://www.phatcode.net/res/220/files/mscdex21.txt */
    __dpmi_regs regs;
    int num_drives;

    regs.x.ax = 0x1500;  /* get number of CD drive letters */
    regs.x.bx = 0x0000;
    __dpmi_int(0x2F, &regs);

    num_drives = (int) regs.x.bx;  /* if zero, no drives or no MSCDEX installed. */
    if (num_drives > 0) {
        char drives[26];
        int i;
        regs.x.ax = 0x150D;  /* get CD drive letters */
        regs.x.es = __tb >> 4;     /* the transfer buffer is at least 2k large, if all possible drives were CD-ROMs, we'd need 26 bytes. */
        regs.x.bx = __tb & 0x0f;
        __dpmi_int(0x2F, &regs);
        dosmemget(__tb, num_drives, drives);
        for (i = 0; i < num_drives; i++) {
            const char path[] = { 'A' + drives[i], ':', '\\', '\0' };
            DIR *dirp = opendir(path);  /* if we can open the drive's root dir, there's a filesystem there. */
            if (dirp) {
                closedir(dirp);
                cb(data, path);
            }
        }
    }
#endif
} /* __PHYSFS_platformDetectAvailableCDs */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    /* As of MS-DOS 3.0, you can get the full path to the EXE from the very end of the
        environment table, which is discovered through the PSP:
        https://en.wikipedia.org/wiki/Program_Segment_Prefix

       An EXE is launched with the PSP's segment in DS, but we'll use a DOS API to obtain it
        later, since we don't control startup. https://stanislavs.org/helppc/int_21-62.html

       The table pointed to by the word at offset 0x2C in the PSP is just a collection
        of ASCIZ strings, with a blank string signifying the end. The EXE path is
        after that.

       https://stackoverflow.com/questions/60928997/how-to-get-environment-variables-in-a-dos-assembly-program
     */
    __dpmi_regs regs;

    char *lastbackslash;
    char *retval;

    unsigned long pspseg;
    unsigned long envsel;
    unsigned long i;
    int zero_count = 0;
    int slen = 0;
    int offset;

    regs.x.ax = 0x6200;  /* get number of CD drive letters */
    regs.x.bx = 0x0000;
    __dpmi_int(0x21, &regs);

    /* https://www.delorie.com/djgpp/doc/ug/dpmi/farptr-intro.html.en */
    pspseg = (unsigned long) regs.x.bx;
    envsel = (unsigned long) _farpeekw(_dos_ds, (pspseg * 16) + 0x2C);

    for (offset = 0; (offset < 0xFFFF) && (zero_count < 2); offset++) {
        const char ch = (char) _farpeekb(envsel, offset);
        if (ch == 0) {
            zero_count++;
        } else {
            zero_count = 0;
        }
    }

    if (zero_count != 2) {
        return NULL;  /* uhoh */
    }

    /* there's a Uint16 here that represents number of extension strings. In practice it's always 1 and that one string is the path we need. */
    offset += 2;

    for (i = offset; _farpeekb(envsel, i) != 0; i++) {
        slen++;
    }

    slen++;  /* count the null terminator. */

    lastbackslash = NULL;
    retval = (char *) allocator.Malloc(slen);
    if (retval) {
        int i = 0;
        do {
            const char ch = (char) _farpeekb(envsel, offset + i);
            retval[i] = ch;
            if (ch == '\\') {
                lastbackslash = &retval[i];
            }
            i++;
        } while (i < slen);
    }

    if (lastbackslash) {
        lastbackslash[1] = '\0';  /* chop off exe name, just leave path */
    }

    return retval;
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcUserDir(void)
{
    return __PHYSFS_platformCalcBaseDir(NULL);  /* !!! FIXME: ? */
}

char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    return __PHYSFS_platformCalcBaseDir(NULL);  /* !!! FIXME: ? */
} /* __PHYSFS_platformCalcPrefDir */


void *__PHYSFS_platformGetThreadID(void)
{
    return (void *) 0x1;
} /* __PHYSFS_platformGetThreadID */


void *__PHYSFS_platformCreateMutex(void)
{
    return (void *) 0x1;
} /* __PHYSFS_platformCreateMutex */


void __PHYSFS_platformDestroyMutex(void *mutex)
{
} /* __PHYSFS_platformDestroyMutex */


int __PHYSFS_platformGrabMutex(void *mutex)
{
    return (mutex == ((void *) 0x1));
} /* __PHYSFS_platformGrabMutex */


void __PHYSFS_platformReleaseMutex(void *mutex)
{
} /* __PHYSFS_platformReleaseMutex */

#endif /* PHYSFS_PLATFORM_DOS */

/* end of physfs_platform_dos.c ... */

