/*
 * Mac OS X support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_platforms.h"

#ifdef PHYSFS_PLATFORM_MACOSX

#include <Carbon/Carbon.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IODVDMedia.h>
#include <sys/mount.h>
#include <sys/stat.h>

/* Seems to get defined in some system header... */
#ifdef Free
#undef Free
#endif

#include "physfs_internal.h"


/* Wrap PHYSFS_Allocator in a CFAllocator... */
static CFAllocatorRef cfallocator = NULL;

CFStringRef cfallocDesc(const void *info)
{
    return CFStringCreateWithCString(cfallocator, "PhysicsFS",
                                     kCFStringEncodingASCII);
} /* cfallocDesc */


static void *cfallocMalloc(CFIndex allocSize, CFOptionFlags hint, void *info)
{
    return allocator.Malloc(allocSize);
} /* cfallocMalloc */


static void cfallocFree(void *ptr, void *info)
{
    allocator.Free(ptr);
} /* cfallocFree */


static void *cfallocRealloc(void *ptr, CFIndex newsize,
                            CFOptionFlags hint, void *info)
{
    if ((ptr == NULL) || (newsize <= 0))
        return NULL;  /* ADC docs say you should always return NULL here. */
    return allocator.Realloc(ptr, newsize);
} /* cfallocRealloc */


int __PHYSFS_platformInit(void)
{
    /* set up a CFAllocator, so Carbon can use the physfs allocator, too. */
    CFAllocatorContext ctx;
    memset(&ctx, '\0', sizeof (ctx));
    ctx.copyDescription = cfallocDesc;
    ctx.allocate = cfallocMalloc;
    ctx.reallocate = cfallocRealloc;
    ctx.deallocate = cfallocFree;
    cfallocator = CFAllocatorCreate(kCFAllocatorUseContext, &ctx);
    BAIL_IF_MACRO(!cfallocator, PHYSFS_ERR_OUT_OF_MEMORY, 0);
    return 1;  /* success. */
} /* __PHYSFS_platformInit */


int __PHYSFS_platformDeinit(void)
{
    CFRelease(cfallocator);
    cfallocator = NULL;
    return 1;  /* always succeed. */
} /* __PHYSFS_platformDeinit */


/* CD-ROM detection code... */

/*
 * Code based on sample from Apple Developer Connection:
 *  http://developer.apple.com/samplecode/Sample_Code/Devices_and_Hardware/Disks/VolumeToBSDNode/VolumeToBSDNode.c.htm
 */

static int darwinIsWholeMedia(io_service_t service)
{
    int retval = 0;
    CFTypeRef wholeMedia;

    if (!IOObjectConformsTo(service, kIOMediaClass))
        return 0;
        
    wholeMedia = IORegistryEntryCreateCFProperty(service,
                                                 CFSTR(kIOMediaWholeKey),
                                                 cfallocator, 0);
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


void __PHYSFS_platformDetectAvailableCDs(PHYSFS_StringCallback cb, void *data)
{
    const char *devPrefix = "/dev/";
    const int prefixLen = strlen(devPrefix);
    mach_port_t masterPort = 0;
    struct statfs *mntbufp;
    int i, mounts;

    if (IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS)
        BAIL_MACRO(PHYSFS_ERR_OS_ERROR, ) /*return void*/;

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
} /* __PHYSFS_platformDetectAvailableCDs */


static char *convertCFString(CFStringRef cfstr)
{
    CFIndex len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr),
                                                    kCFStringEncodingUTF8) + 1;
    char *retval = (char *) allocator.Malloc(len);
    BAIL_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

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
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    } /* else */

    return retval;
} /* convertCFString */


char *__PHYSFS_platformCalcBaseDir(const char *argv0)
{
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    struct stat statbuf;
    FSRef fsref;
    CFRange cfrange;
    CFURLRef cfurl = NULL;
    CFStringRef cfstr = NULL;
    CFMutableStringRef cfmutstr = NULL;
    char *retval = NULL;
    char *cstr = NULL;
    int rc = 0;

    if (GetProcessBundleLocation(&psn, &fsref) != noErr)
        BAIL_MACRO(PHYSFS_ERR_OS_ERROR, NULL);
    cfurl = CFURLCreateFromFSRef(cfallocator, &fsref);
    BAIL_IF_MACRO(cfurl == NULL, PHYSFS_ERR_OS_ERROR, NULL);
    cfstr = CFURLCopyFileSystemPath(cfurl, kCFURLPOSIXPathStyle);
    CFRelease(cfurl);
    BAIL_IF_MACRO(!cfstr, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    cfmutstr = CFStringCreateMutableCopy(cfallocator, 0, cfstr);
    CFRelease(cfstr);
    BAIL_IF_MACRO(!cfmutstr, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    /* we have to decide if we got a binary's path, or the .app dir... */
    cstr = convertCFString(cfmutstr);
    if (cstr == NULL)
    {
        CFRelease(cfmutstr);
        return NULL;
    } /* if */

    rc = stat(cstr, &statbuf);
    allocator.Free(cstr);  /* done with this. */
    if (rc == -1)
    {
        CFRelease(cfmutstr);
        return NULL;  /* maybe default behaviour will work? */
    } /* if */

    if (S_ISREG(statbuf.st_mode))
    {
        /* Find last dirsep so we can chop the filename from the path. */
        cfrange = CFStringFind(cfmutstr, CFSTR("/"), kCFCompareBackwards);
        if (cfrange.location == kCFNotFound)
        {
            assert(0);  /* shouldn't ever hit this... */
            CFRelease(cfmutstr);
            return NULL;
        } /* if */

        /* chop the "/exename" from the end of the path string... */
        cfrange.length = CFStringGetLength(cfmutstr) - cfrange.location;
        CFStringDelete(cfmutstr, cfrange);

        /* If we're an Application Bundle, chop everything but the base. */
        cfrange = CFStringFind(cfmutstr, CFSTR("/Contents/MacOS"),
                               kCFCompareCaseInsensitive |
                               kCFCompareBackwards |
                               kCFCompareAnchored);

        if (cfrange.location != kCFNotFound)
            CFStringDelete(cfmutstr, cfrange);  /* chop that, too. */
    } /* if */

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
    BAIL_IF_MACRO(!retval, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    snprintf(retval, len, "%s%s%s/", userdir, append, app);
    return retval;
} /* __PHYSFS_platformCalcPrefDir */


/* Platform allocator uses default CFAllocator at PHYSFS_init() time. */

static CFAllocatorRef cfallocdef = NULL;

static int macosxAllocatorInit(void)
{
    int retval = 0;
    cfallocdef = CFAllocatorGetDefault();
    retval = (cfallocdef != NULL);
    if (retval)
        CFRetain(cfallocdef);
    return retval;
} /* macosxAllocatorInit */


static void macosxAllocatorDeinit(void)
{
    if (cfallocdef != NULL)
    {
        CFRelease(cfallocdef);
        cfallocdef = NULL;
    } /* if */
} /* macosxAllocatorDeinit */


static void *macosxAllocatorMalloc(PHYSFS_uint64 s)
{
    if (!__PHYSFS_ui64FitsAddressSpace(s))
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    return CFAllocatorAllocate(cfallocdef, (CFIndex) s, 0);
} /* macosxAllocatorMalloc */


static void *macosxAllocatorRealloc(void *ptr, PHYSFS_uint64 s)
{
    if (!__PHYSFS_ui64FitsAddressSpace(s))
        BAIL_MACRO(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    return CFAllocatorReallocate(cfallocdef, ptr, (CFIndex) s, 0);
} /* macosxAllocatorRealloc */


static void macosxAllocatorFree(void *ptr)
{
    CFAllocatorDeallocate(cfallocdef, ptr);
} /* macosxAllocatorFree */


int __PHYSFS_platformSetDefaultAllocator(PHYSFS_Allocator *a)
{
    allocator.Init = macosxAllocatorInit;
    allocator.Deinit = macosxAllocatorDeinit;
    allocator.Malloc = macosxAllocatorMalloc;
    allocator.Realloc = macosxAllocatorRealloc;
    allocator.Free = macosxAllocatorFree;
    return 1;  /* return non-zero: we're supplying custom allocator. */
} /* __PHYSFS_platformSetDefaultAllocator */

#endif /* PHYSFS_PLATFORM_MACOSX */

/* end of macosx.c ... */

