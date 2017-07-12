/*
 * Mac OS X support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#ifdef PHYSFS_PLATFORM_MACOSX

#include <CoreFoundation/CoreFoundation.h>

#if !defined(PHYSFS_NO_CDROM_SUPPORT)
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <sys/mount.h>
#endif

int __PHYSFS_platformInit(void)
{
    return 1;  /* success. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    return 1;  /* always succeed. */
} /* __PHYSFS_platformDeinit */



/* CD-ROM detection code... */

/*
 * Code based on sample from Apple Developer Connection:
 *  https://developer.apple.com/samplecode/Sample_Code/Devices_and_Hardware/Disks/VolumeToBSDNode/VolumeToBSDNode.c.htm
 */

#if !defined(PHYSFS_NO_CDROM_SUPPORT)

static int darwinIsWholeMedia(io_service_t service)
{
    int retval = 0;
    CFTypeRef wholeMedia;

    if (!IOObjectConformsTo(service, kIOMediaClass))
        return 0;
        
    wholeMedia = IORegistryEntryCreateCFProperty(service,
                                                 CFSTR(kIOMediaWholeKey),
                                                 NULL, 0);
    if (wholeMedia == NULL)
        return 0;

    retval = CFBooleanGetValue(wholeMedia);
    CFRelease(wholeMedia);

    return retval;
} /* darwinIsWholeMedia */


static int darwinIsMountedDisc(char *bsdName, mach_port_t masterPort)
{
    int retval = 0;
    CFMutableDictionaryRef matchingDict;
    kern_return_t rc;
    io_iterator_t iter;
    io_service_t service;

    if ((matchingDict = IOBSDNameMatching(masterPort, 0, bsdName)) == NULL)
        return 0;

    rc = IOServiceGetMatchingServices(masterPort, matchingDict, &iter);
    if ((rc != KERN_SUCCESS) || (!iter))
        return 0;

    service = IOIteratorNext(iter);
    IOObjectRelease(iter);
    if (!service)
        return 0;

    rc = IORegistryEntryCreateIterator(service, kIOServicePlane,
             kIORegistryIterateRecursively | kIORegistryIterateParents, &iter);
    
    if (!iter)
        return 0;

    if (rc != KERN_SUCCESS)
    {
        IOObjectRelease(iter);
        return 0;
    } /* if */

    IOObjectRetain(service);  /* add an extra object reference... */

    do
    {
        if (darwinIsWholeMedia(service))
        {
            if ( (IOObjectConformsTo(service, kIOCDMediaClass)) ||
                 (IOObjectConformsTo(service, kIODVDMediaClass)) )
            {
                retval = 1;
            } /* if */
        } /* if */
        IOObjectRelease(service);
    } while ((service = IOIteratorNext(iter)) && (!retval));
                
    IOObjectRelease(iter);
    IOObjectRelease(service);

    return retval;
} /* darwinIsMountedDisc */

#endif /* !defined(PHYSFS_NO_CDROM_SUPPORT) */


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
#if !defined(PHYSFS_NO_CDROM_SUPPORT)
    const char *devPrefix = "/dev/";
    const int prefixLen = strlen(devPrefix);
    mach_port_t masterPort = 0;
    struct statfs *mntbufp;
    int i, mounts;

    if (IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS)
        BAIL(PHYSFS_ERR_OS_ERROR, ) /*return void*/;

    mounts = getmntinfo(&mntbufp, MNT_WAIT);  /* NOT THREAD SAFE! */
    for (i = 0; i < mounts; i++)
    {
        char *dev = mntbufp[i].f_mntfromname;
        char *mnt = mntbufp[i].f_mntonname;
        if (strncmp(dev, devPrefix, prefixLen) != 0)  /* a virtual device? */
            continue;

        dev += prefixLen;
        if (darwinIsMountedDisc(dev, masterPort))
            cb(data, mnt);
    } /* for */
#endif /* !defined(PHYSFS_NO_CDROM_SUPPORT) */
} /* __PHYSFS_platformDetectAvailableCDs */


static char *convertCFString(CFStringRef cfstr)
{
    CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr),
                                                    kCFStringEncodingUTF8) + 1;
    char *retval = (char *) allocator.Malloc(len);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    if (CFStringGetCString(cfstr, retval, len, kCFStringEncodingUTF8))
    {
        /* shrink overallocated buffer if possible... */
        CFIndex newlen = strlen(retval) + 1;
        if (newlen < len)
        {
            void *ptr = allocator.Realloc(retval, newlen);
            if (ptr != NULL)
                retval = (char *) ptr;
        } /* if */
    } /* if */

    else  /* probably shouldn't fail, but just in case... */
    {
        allocator.Free(retval);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* else */

    return retval;
} /* convertCFString */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    CFURLRef cfurl = NULL;
    CFStringRef cfstr = NULL;
    CFMutableStringRef cfmutstr = NULL;
    char *retval = NULL;

    cfurl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    BAIL_IF(cfurl == NULL, PHYSFS_ERR_OS_ERROR, NULL);
    cfstr = CFURLCopyFileSystemPath(cfurl, kCFURLPOSIXPathStyle);
    CFRelease(cfurl);
    BAIL_IF(!cfstr, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    cfmutstr = CFStringCreateMutableCopy(NULL, 0, cfstr);
    CFRelease(cfstr);
    BAIL_IF(!cfmutstr, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    CFStringAppendCString(cfmutstr, "/", kCFStringEncodingUTF8);
    retval = convertCFString(cfmutstr);
    CFRelease(cfmutstr);

    return retval;  /* whew. */
} /* __PHYSFS_platformCalcBaseDir */


char *__PHYSFS_platformCalcPrefDir(const char *org, const char *app)
{
    /* !!! FIXME: there's a real API to determine this */
    const char *userdir = __PHYSFS_getUserDir();
    const char *append = "Library/Application Support/";
    const size_t len = strlen(userdir) + strlen(append) + strlen(app) + 2;
    char *retval = allocator.Malloc(len);
    BAIL_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    snprintf(retval, len, "%s%s%s/", userdir, append, app);
    return retval;
} /* __PHYSFS_platformCalcPrefDir */

#endif /* PHYSFS_PLATFORM_MACOSX */

/* end of macosx.c ... */

