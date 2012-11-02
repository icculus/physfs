#ifndef _INCL_PHYSFS_PLATFORMS
#define _INCL_PHYSFS_PLATFORMS

#ifndef __PHYSICSFS_INTERNAL__
#error Do not include this header from your applications.
#endif

/*
 * These only define the platforms to determine which files in the platforms
 *  directory should be compiled. For example, technically BeOS can be called
 *  a "unix" system, but since it doesn't use unix.c, we don't define
 *  PHYSFS_PLATFORM_UNIX on that system.
 */

#if (defined __HAIKU__)
#  define PHYSFS_PLATFORM_HAIKU 1
#  define PHYSFS_PLATFORM_BEOS 1
#  define PHYSFS_PLATFORM_POSIX 1
#elif ((defined __BEOS__) || (defined __beos__))
#  define PHYSFS_PLATFORM_BEOS 1
#  define PHYSFS_PLATFORM_POSIX 1
#elif (defined _WIN32_WCE) || (defined _WIN64_WCE)
#  error PocketPC support was dropped from PhysicsFS 2.1. Sorry.
#elif (((defined _WIN32) || (defined _WIN64)) && (!defined __CYGWIN__))
#  define PHYSFS_PLATFORM_WINDOWS 1
#elif (defined OS2)
#  error OS/2 support was dropped from PhysicsFS 2.1. Sorry.
#elif ((defined __MACH__) && (defined __APPLE__))
/* To check if iphone or not, we need to include this file */
#  include <TargetConditionals.h>
#  if ((TARGET_IPHONE_SIMULATOR) || (TARGET_OS_IPHONE))
#     define PHYSFS_NO_CDROM_SUPPORT 1
#  endif
#  define PHYSFS_PLATFORM_MACOSX 1
#  define PHYSFS_PLATFORM_POSIX 1
#elif defined(macintosh)
#  error Classic Mac OS support was dropped from PhysicsFS 2.0. Move to OS X.
#elif defined(ANDROID)
#  define PHYSFS_PLATFORM_LINUX 1
#  define PHYSFS_PLATFORM_UNIX 1
#  define PHYSFS_PLATFORM_POSIX 1
#  define PHYSFS_NO_CDROM_SUPPORT 1
#elif defined(__linux)
#  define PHYSFS_PLATFORM_LINUX 1
#  define PHYSFS_PLATFORM_UNIX 1
#  define PHYSFS_PLATFORM_POSIX 1
#elif defined(__sun) || defined(sun)
#  define PHYSFS_PLATFORM_SOLARIS 1
#  define PHYSFS_PLATFORM_UNIX 1
#  define PHYSFS_PLATFORM_POSIX 1
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__bsdi__) || defined(__DragonFly__)
#  define PHYSFS_PLATFORM_BSD 1
#  define PHYSFS_PLATFORM_UNIX 1
#  define PHYSFS_PLATFORM_POSIX 1
#elif defined(unix) || defined(__unix__)
#  define PHYSFS_PLATFORM_UNIX 1
#  define PHYSFS_PLATFORM_POSIX 1
#else
#  error Unknown platform.
#endif

#endif  /* include-once blocker. */

