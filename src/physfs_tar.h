#ifndef _INCLUDE_PHYSFS_TAR_H_
#define _INCLUDE_PHYSFS_TAR_H_

#include <stdbool.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#if STDC_HEADERS
#  define IN_CTYPE_DOMAIN(c) 1
#else
#  define IN_CTYPE_DOMAIN(c) ((unsigned) (c) <= 0177)
#endif

#define LG_8 3
#define LG_64 6
#define LG_256 8
#define ISOCTAL(c) ((c)>='0'&&(c)<='7')
#define ISODIGIT(c) ((unsigned) (c) - '0' <= 7 )
#define ISPRINT(c) (IN_CTYPE_DOMAIN (c) && isprint(c))
#define ISSPACE(c) (IN_CTPYE_DOMAIN (c) && isspace(c))

#define BLOCKSIZE 512             /* tar basic block size */

#define TMAGIC   "ustar"          /* ustar and a null */
#define OLDGNU_MAGIC "ustar  "	/* 7 chars and a null */
#define TMAGLEN  6

#define TVERSION "00"             /* 00 and no null */
#define TVERSLEN 2

enum archive_format
{
	DEFAULT_FORMAT,		  /* format to be decided later */
	V7_FORMAT,		  /* old V7 tar format */
	OLDGNU_FORMAT,		  /* old GNU format */
	USTAR_FORMAT,             /* POSIX.1-1988 (ustar) format */
	POSIX_FORMAT,             /* POSIX.1-2001 format */
	STAR_FORMAT,              /* Star format defined in 1994 */
	GNU_FORMAT,               /* new GNU format */
};

enum typefield {
	REGTYPE          =   '0', /* regular file */
	AREGTYPE         =  '\0', /* regular file */
	LNKTYPE          =   '1', /* link */
	SYMTYPE          =   '2', /* reserved */
	CHRTYPE          =   '3', /* character special */
	BLKTYPE          =   '4', /* block special */
	DIRTYPE          =   '5', /* directory */
	FIFOTYPE         =   '6', /* FIFO special */
	CONTTYPE         =   '7', /* reserved */
	XHDTYPE          =   'x', /* Extended header referring to the next file in the archive */
	XGLTYPE          =   'g', /* Global extended header */
	GNUTYPE_DUMPDIR	 =   'D', /* names of files been in the dir at the time of the dump */
	GNUTYPE_LONGLINK =   'K', /* next file has long linkname.  */
	GNUTYPE_LONGNAME =   'L', /* next file has long name.  */
	GNUTYPE_MULTIVOL =   'M', /* continuation file started on another volume.  */
	GNUTYPE_SPARSE   =   'S', /* This is for sparse files.  */
	GNUTYPE_VOLHDR   =   'V', /* This file is a tape/volume header.  Ignore it on extraction.  */
	SOLARIS_XHDTYPE  =   'X', /* Solaris extended header */
};

enum mode {
	TOEXEC           = 1 << 0, /* execute/search by other */
	TOWRITE          = 1 << 1, /* write by other */
	TOREAD           = 1 << 2, /* read by other */
	TGEXEC           = 1 << 3, /* execute/search by group */
	TGWRITE          = 1 << 4, /* write by group */
	TGREAD           = 1 << 5, /* read by group */
	TUEXEC           = 1 << 6, /* execute/search by owner */
	TUWRITE          = 1 << 7, /* write by owner */
	TUREAD           = 1 << 8, /* read by owner */
	TSVTX            = 1 << 9, /* reserved */
	TSGID            = 1 << 10, /* set GID on execution */
	TSUID            = 1 << 11, /* set UID on execution */
};

/* POSIX header.  */
struct posix_header
{				  /* byte offset */
	char name[100];           /*   0 */
	char mode[8];             /* 100 */
	char uid[8];              /* 108 */
	char gid[8];              /* 116 */
	char size[12];            /* 124 */
	char mtime[12];           /* 136 */
	char chksum[8];           /* 148 */
	char typeflag;            /* 156 */
	char linkname[100];       /* 157 */
	char magic[TMAGLEN];      /* 257 */
	char version[2];          /* 263 */
	char uname[32];           /* 265 */
	char gname[32];           /* 297 */
	char devmajor[8];         /* 329 */
	char devminor[8];         /* 337 */
	char prefix[155];         /* 345 */
	                          /* 500 */
};

/* tar Header Block, from POSIX 1003.1-1990.  */
union block
{
  char buffer[BLOCKSIZE];
  struct posix_header header;
};

static PHYSFS_uint64 TAR_decodeOctal(char *data, size_t size) {
    unsigned char *currentPtr = (unsigned char*) data + size;
    PHYSFS_uint64 sum = 0;
    PHYSFS_uint64 currentMultiplier = 1;
    unsigned char *checkPtr = currentPtr;

    for (; checkPtr >= (unsigned char *) data; checkPtr--) {
        if ((*checkPtr) == 0 || (*checkPtr) == ' ') {
            currentPtr = checkPtr - 1;
        }
    }
    for (; currentPtr >= (unsigned char *) data; currentPtr--) {
        sum += (*currentPtr - 48) * currentMultiplier;
        currentMultiplier *= 8;
    }
    return sum;
}

static enum archive_format TAR_magic(union block *block) {
	if(strcmp(block->header.magic, OLDGNU_MAGIC) == 0 && strncmp(block->header.version, TVERSION, 2) == 0)
		return OLDGNU_FORMAT;
	if(strncmp(block->header.magic, TMAGIC, TMAGLEN - 1) == 0)
		return POSIX_FORMAT;
	return DEFAULT_FORMAT;
}

static PHYSFS_uint64 TAR_fileSize(union block *block) {
	return TAR_decodeOctal(block->header.size, sizeof(block->header.size));
}

static bool TAR_checksum(union block *block) {
	PHYSFS_sint64 unsigned_sum = 0;
	PHYSFS_sint64 signed_sum = 0;
	PHYSFS_uint64 reference_chksum = 0;
	char orig_chksum[8];
	int i = 0;

	memcpy(orig_chksum, block->header.chksum, 8);
	memset(block->header.chksum, ' ', 8);

	for(; i < BLOCKSIZE; i++) {
		unsigned_sum += ((unsigned char *) block->buffer)[i];
			signed_sum += ((signed char *) block->buffer)[i];
	}
	memcpy(block->header.chksum, orig_chksum, 8);
	reference_chksum = TAR_decodeOctal(orig_chksum, 12);
	return (reference_chksum == unsigned_sum || reference_chksum == signed_sum);
}

static PHYSFS_uint64 TAR_time(union block *block)
{
	return TAR_decodeOctal(block->header.mtime, 12);
}

static bool TAR_posix_block(PHYSFS_Io *io, void *arc, union block *block, PHYSFS_uint64 *count, bool *long_name)
{
	const PHYSFS_Allocator *allocator = PHYSFS_getAllocator();
        char name[PATH_MAX] = { 0 };
	PHYSFS_sint64 time  = 0;
	PHYSFS_uint64 size  = 0;
	PHYSFS_uint64 pos   = 0;
	PHYSFS_uint64 pad   = 0;
	char*         buf   = NULL;

	/* verify checksum */
	if(!TAR_checksum(block))
		return false;

	/* get time */ 
	time = TAR_time(block);

	memset(name, '\0', PATH_MAX);
	/* support prefix */
	if(strlen(block->header.prefix) > 0)
	{
		strcpy(name, block->header.prefix);
		name[strlen(name)] = '/';
	}
	strcpy(&name[strlen(name)], block->header.name);

	/* add file type entry */
	if (block->header.typeflag == REGTYPE || block->header.typeflag == 0) {
		/* support long file names */
		if(*long_name) {
			strcpy(&name[0], block->header.name);
			BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, block->buffer, BLOCKSIZE), 0);
			*long_name = false;
			(*count)++;
		}
		size = TAR_fileSize(block);
		pos  = ((*count) + 1) * BLOCKSIZE;
		pad  = (BLOCKSIZE - (size % BLOCKSIZE)) % BLOCKSIZE;
		buf  = allocator->Malloc(size + pad);
		/* add entry to arc */
		BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, buf, size + pad), 0);
		(*count) += ((size + pad) / BLOCKSIZE );
		allocator->Free(buf);
		BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 0, time, time, pos, size), 0);
	}
	/* add directory type entry */
	else if(block->header.typeflag == DIRTYPE)
	{
		BAIL_IF_ERRPASS(!UNPK_addEntry(arc, name, 1, time, time, 0, 0), 0);
	}
	/* long name mode */
	else if(block->header.typeflag == GNUTYPE_LONGNAME)
	{
		*long_name = true;
	}
	else
	{
		// UNHANDLED
	}
	return true;
}

#endif  /* _INCLUDE_PHYSFS_TAR_H_ */

/* end of physfs_tar.h ... */

