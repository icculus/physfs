/**
 * PhysicsFS; a portable, flexible file i/o abstraction.
 *
 * Documentation is in physfs.h. It's verbose, honest.  :)
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "physfs.h"

/* This byteorder stuff was lifted from SDL. http://www.libsdl.org/ */
#define PHYSFS_LIL_ENDIAN  1234
#define PHYSFS_BIG_ENDIAN  4321

#if  defined(__i386__) || defined(__ia64__) || defined(WIN32) || \
    (defined(__alpha__) || defined(__alpha)) || \
     defined(__arm__) || \
    (defined(__mips__) && defined(__MIPSEL__)) || \
     defined(__SYMBIAN32__) || \
     defined(__LITTLE_ENDIAN__)
#define PHYSFS_BYTEORDER    PHYSFS_LIL_ENDIAN
#else
#define PHYSFS_BYTEORDER    PHYSFS_BIG_ENDIAN
#endif


/* The macros used to swap values */
/* Try to use superfast macros on systems that support them */
#ifdef linux
#include <asm/byteorder.h>
#ifdef __arch__swab16
#define PHYSFS_Swap16  __arch__swab16
#endif
#ifdef __arch__swab32
#define PHYSFS_Swap32  __arch__swab32
#endif
#endif /* linux */

#if (defined _MSC_VER)
#define __inline__ __inline
#endif

#ifndef PHYSFS_Swap16
static __inline__ PHYSFS_uint16 PHYSFS_Swap16(PHYSFS_uint16 D)
{
	return((D<<8)|(D>>8));
}
#endif
#ifndef PHYSFS_Swap32
static __inline__ PHYSFS_uint32 PHYSFS_Swap32(PHYSFS_uint32 D)
{
	return((D<<24)|((D<<8)&0x00FF0000)|((D>>8)&0x0000FF00)|(D>>24));
}
#endif
#ifndef PHYSFS_NO_64BIT_SUPPORT
#ifndef PHYSFS_Swap64
static __inline__ PHYSFS_uint64 PHYSFS_Swap64(PHYSFS_uint64 val) {
	PHYSFS_uint32 hi, lo;

	/* Separate into high and low 32-bit values and swap them */
	lo = (PHYSFS_uint32)(val&0xFFFFFFFF);
	val >>= 32;
	hi = (PHYSFS_uint32)(val&0xFFFFFFFF);
	val = PHYSFS_Swap32(lo);
	val <<= 32;
	val |= PHYSFS_Swap32(hi);
	return(val);
}
#endif
#else
#ifndef PHYSFS_Swap64
/* This is mainly to keep compilers from complaining in PHYSFS code.
   If there is no real 64-bit datatype, then compilers will complain about
   the fake 64-bit datatype that PHYSFS provides when it compiles user code.
*/
#define PHYSFS_Swap64(X)	(X)
#endif
#endif /* PHYSFS_NO_64BIT_SUPPORT */


/* Byteswap item from the specified endianness to the native endianness */
#if PHYSFS_BYTEORDER == PHYSFS_LIL_ENDIAN
PHYSFS_uint16 PHYSFS_swapULE16(PHYSFS_uint16 x) { return(x); }
PHYSFS_sint16 PHYSFS_swapSLE16(PHYSFS_sint16 x) { return(x); }
PHYSFS_uint32 PHYSFS_swapULE32(PHYSFS_uint32 x) { return(x); }
PHYSFS_sint32 PHYSFS_swapSLE32(PHYSFS_sint32 x) { return(x); }
PHYSFS_uint64 PHYSFS_swapULE64(PHYSFS_uint64 x) { return(x); }
PHYSFS_sint64 PHYSFS_swapSLE64(PHYSFS_sint64 x) { return(x); }

PHYSFS_uint16 PHYSFS_swapUBE16(PHYSFS_uint16 x) { return(PHYSFS_Swap16(x)); }
PHYSFS_sint16 PHYSFS_swapSBE16(PHYSFS_sint16 x) { return(PHYSFS_Swap16(x)); }
PHYSFS_uint32 PHYSFS_swapUBE32(PHYSFS_uint32 x) { return(PHYSFS_Swap32(x)); }
PHYSFS_sint32 PHYSFS_swapSBE32(PHYSFS_sint32 x) { return(PHYSFS_Swap32(x)); }
PHYSFS_uint64 PHYSFS_swapUBE64(PHYSFS_uint64 x) { return(PHYSFS_Swap64(x)); }
PHYSFS_sint64 PHYSFS_swapSBE64(PHYSFS_sint64 x) { return(PHYSFS_Swap64(x)); }
#else
PHYSFS_uint16 PHYSFS_swapULE16(PHYSFS_uint16 x) { return(PHYSFS_Swap16(x)); }
PHYSFS_sint16 PHYSFS_swapSLE16(PHYSFS_sint16 x) { return(PHYSFS_Swap16(x)); }
PHYSFS_uint32 PHYSFS_swapULE32(PHYSFS_uint32 x) { return(PHYSFS_Swap32(x)); }
PHYSFS_sint32 PHYSFS_swapSLE32(PHYSFS_sint32 x) { return(PHYSFS_Swap32(x)); }
PHYSFS_uint64 PHYSFS_swapULE64(PHYSFS_uint64 x) { return(PHYSFS_Swap64(x)); }
PHYSFS_sint64 PHYSFS_swapSLE64(PHYSFS_sint64 x) { return(PHYSFS_Swap64(x)); }

PHYSFS_uint16 PHYSFS_swapUBE16(PHYSFS_uint16 x) { return(x); }
PHYSFS_sint16 PHYSFS_swapSBE16(PHYSFS_sint16 x) { return(x); }
PHYSFS_uint32 PHYSFS_swapUBE32(PHYSFS_uint32 x) { return(x); }
PHYSFS_sint32 PHYSFS_swapSBE32(PHYSFS_sint32 x) { return(x); }
PHYSFS_uint64 PHYSFS_swapUBE64(PHYSFS_uint64 x) { return(x); }
PHYSFS_sint64 PHYSFS_swapSBE64(PHYSFS_sint64 x) { return(x); }
#endif

/* end of physfs_byteorder.c ... */

