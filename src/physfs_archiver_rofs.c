/*
 * Resident Evil 3 rofs<n>.dat support routines for PhysicsFS.
 *
 * This driver handles Resident Evil 3 archives.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 * This file written by Patrice Mandin, based on the QPAK archiver by
 *  Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_ROFS

typedef struct
{
	__PHYSFS_DirTreeEntry tree;	/* manages directory tree */

	PHYSFS_uint32 startPos;
	PHYSFS_uint32 size;
	char name[48];	/* room to store /dir1/dir2/filename */

	int compression;

	/* Block infos */
	int num_blocks;
	PHYSFS_uint32 *block_alloc;
	PHYSFS_uint32 *block_startkey;
	PHYSFS_uint32 *block_length;
} ROFSentry;

/* rofsinfo -> rofsentry level1 -> rofsentry level2 -> rofsentry files */

typedef struct
{
	__PHYSFS_DirTree tree;	/* manages directory tree. */
	PHYSFS_Io *io;			/* the i/o interface for this archive. */

	PHYSFS_uint32 entryCount;
	ROFSentry dirs[2];
	ROFSentry *entries;
} ROFSinfo;

typedef struct {
	/* Current decryption infos */
	PHYSFS_uint32 curblock, curoffset;
	PHYSFS_uint32 curkey, blockindex;
	PHYSFS_uint8 xorkey, baseindex;
} rofs_decrypt_t;

enum {
	ROFS_STATE_UNPACK_BYTE,
	ROFS_STATE_UNPACK_BLOCK,
	ROFS_STATE_COPY_BLOCK,
	ROFS_STATE_ERROR
};

typedef struct {
	int state;	/* above enum */

	int src_num_bit, src_byte_num;
	PHYSFS_uint8 src_bytes[2];
	int lzss_pos, lzss_start, lzss_length, copy_length;
	PHYSFS_uint8 *lzss_dict;
} rofs_depack_t;

typedef struct
{
	ROFSentry *entry;
	PHYSFS_Io *io;

	PHYSFS_uint32 curPos;

	rofs_decrypt_t decrypt;
	rofs_depack_t depack;
} ROFSfileinfo;

typedef struct {
	PHYSFS_uint16 offset;
	PHYSFS_uint16 num_keys;
	PHYSFS_uint32 length;
	char ident[8];
} rofs_dat_file_t;

static PHYSFS_sint64 ROFS_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 len);

/*
 * Read an unsigned 32-bit int and swap to native byte order.
 */
static int readui32(PHYSFS_Io *io, PHYSFS_uint32 *val)
{
	PHYSFS_uint32 v;
	BAIL_IF_ERRPASS(!__PHYSFS_readAll(io, &v, sizeof (v)), 0);
	*val = PHYSFS_swapULE32(v);
	return 1;
} /* readui32 */


/*--- Decryption ---*/

static const unsigned short base_array[64]={
	0x00e6, 0x01a4, 0x00e6, 0x01c5,
	0x0130, 0x00e8, 0x03db, 0x008b,
	0x0141, 0x018e, 0x03ae, 0x0139,
	0x00f0, 0x027a, 0x02c9, 0x01b0,
	0x01f7, 0x0081, 0x0138, 0x0285,
	0x025a, 0x015b, 0x030f, 0x0335,
	0x02e4, 0x01f6, 0x0143, 0x00d1,
	0x0337, 0x0385, 0x007b, 0x00c6,
	0x0335, 0x0141, 0x0186, 0x02a1,
	0x024d, 0x0342, 0x01fb, 0x03e5,
	0x01b0, 0x006d, 0x0140, 0x00c0,
	0x0386, 0x016b, 0x020b, 0x009a,
	0x0241, 0x00de, 0x015e, 0x035a,
	0x025b, 0x0154, 0x0068, 0x02e8,
	0x0321, 0x0071, 0x01b0, 0x0232,
	0x02d9, 0x0263, 0x0164, 0x0290
};

static PHYSFS_uint8 rofs_next_key(PHYSFS_uint32 *key)
{
	*key *= 0x5d588b65;
	*key += 0x8000000b;

	return (*key >> 24);
}

static PHYSFS_uint8 rofs_decrypt_byte(ROFSfileinfo *finfo, PHYSFS_uint8 value)
{
	if (finfo->decrypt.blockindex>base_array[finfo->decrypt.baseindex]) {
		finfo->decrypt.baseindex = rofs_next_key(&finfo->decrypt.curkey) % 0x3f;
		finfo->decrypt.xorkey = rofs_next_key(&finfo->decrypt.curkey);
		finfo->decrypt.blockindex = 0;
	}
	finfo->decrypt.blockindex++;

	return (value ^ finfo->decrypt.xorkey);
}

static void rofs_decrypt_block_init(ROFSfileinfo *finfo)
{
	ROFSentry *entry = finfo->entry;

	finfo->decrypt.curoffset = 0;
	finfo->decrypt.curkey = entry->block_startkey[finfo->decrypt.curblock];
	finfo->decrypt.blockindex = 0;
	finfo->decrypt.xorkey = rofs_next_key(&finfo->decrypt.curkey);
	finfo->decrypt.baseindex = rofs_next_key(&finfo->decrypt.curkey) % 0x3f;
}


/*--- Decompression ---*/

static void rofs_depack_block_init(ROFSfileinfo *finfo)
{
	int i;

	if (!finfo->depack.lzss_dict) {
		finfo->depack.lzss_dict = (PHYSFS_uint8 *) allocator.Malloc(4096+256);
	}

	if (finfo->depack.lzss_dict) {
		for (i=0; i<256; i++) {
			memset(&(finfo->depack.lzss_dict)[i*16], i, 16);
		}
		memset(&(finfo->depack.lzss_dict)[4096], 0, 256);
	}

	finfo->depack.state = ROFS_STATE_UNPACK_BYTE;
	finfo->depack.src_num_bit = 8;
	finfo->depack.src_byte_num = 1;
	finfo->depack.src_bytes[0] = 0;
	finfo->depack.src_bytes[1] = 0;
	finfo->depack.lzss_pos = 0;
	finfo->depack.lzss_start = 0;
	finfo->depack.lzss_length = 0;
}

/*	if (__PHYSFS_platformRead(finfo->handle, &dummy, 1) != 1) {	\*/

#define ROFS_READ_BYTE() \
{	\
	PHYSFS_uint8 dummy;	\
	\
	if (finfo->decrypt.curoffset >= entry->block_length[finfo->decrypt.curblock]) {	\
		finfo->depack.state = ROFS_STATE_ERROR;	\
		break;	\
	}	\
	\
	if (finfo->io->read(finfo->io, &dummy, 1) != 1) {	\
		finfo->depack.state = ROFS_STATE_ERROR;	\
		break;	\
	}	\
	\
	dummy = rofs_decrypt_byte(finfo, dummy);	\
	finfo->decrypt.curoffset++;	\
	\
	finfo->depack.src_byte_num ^= 1;	\
	finfo->depack.src_bytes[finfo->depack.src_byte_num] = dummy;	\
}

static int rofs_depack_block(ROFSfileinfo *finfo, void *buffer,
							 PHYSFS_uint32 srcLength, PHYSFS_uint32 dstLength)
{
	ROFSentry *entry = finfo->entry;
	PHYSFS_uint8 *dstBuffer = (PHYSFS_uint8 *) buffer;
	int dstIndex = 0;

	if (!finfo->depack.lzss_dict) {
		return(0);
	}

	while ((dstIndex < dstLength) && (finfo->depack.state!=ROFS_STATE_ERROR)) {
		switch(finfo->depack.state) {
			case ROFS_STATE_UNPACK_BYTE:
			{
				PHYSFS_uint16 src_bitfield;

				if (finfo->depack.lzss_pos > 4095) {
					finfo->depack.lzss_pos = 0;
				}

				if (finfo->depack.src_num_bit==8) {
					finfo->depack.src_num_bit = 0;
					ROFS_READ_BYTE();
				}

				finfo->depack.src_num_bit++;

				src_bitfield =
				finfo->depack.src_bytes[finfo->depack.src_byte_num] << finfo->depack.src_num_bit;

				ROFS_READ_BYTE();

				src_bitfield |=
				finfo->depack.src_bytes[finfo->depack.src_byte_num] >> (8-finfo->depack.src_num_bit);

				if (src_bitfield & (1<<8)) {
					/* Init copy from lzss dict */
					int value2;

					value2 =
					finfo->depack.src_bytes[finfo->depack.src_byte_num] << finfo->depack.src_num_bit;

					ROFS_READ_BYTE();

					value2 |=
					finfo->depack.src_bytes[finfo->depack.src_byte_num] >> (8-finfo->depack.src_num_bit);

					finfo->depack.lzss_start = (value2 >> 4) & 0xfff;
					finfo->depack.lzss_start |= (src_bitfield & 0xff) << 4;
					finfo->depack.lzss_length = (value2 & 0x0f)+2;
					finfo->depack.copy_length = finfo->depack.lzss_length;
					finfo->depack.state = ROFS_STATE_UNPACK_BLOCK;
				} else {
					/* Copy byte */
					dstBuffer[dstIndex++] =
					finfo->depack.lzss_dict[finfo->depack.lzss_pos++] =
					src_bitfield;
				}
			}
			break;
			case ROFS_STATE_UNPACK_BLOCK:
			{
				/* Copy from dictionnary */
				dstBuffer[dstIndex++] =
				finfo->depack.lzss_dict[finfo->depack.lzss_start++];
				if (--finfo->depack.lzss_length <= 0) {
					finfo->depack.state = ROFS_STATE_COPY_BLOCK;
				}
			}
			break;
			case ROFS_STATE_COPY_BLOCK:
			{
				int i;
				PHYSFS_uint8 *src =
				&(dstBuffer[dstIndex - finfo->depack.copy_length]);

				for (i=0; i<finfo->depack.copy_length; i++) {
					finfo->depack.lzss_dict[finfo->depack.lzss_pos++] = *src++;
				}
				finfo->depack.state = ROFS_STATE_UNPACK_BYTE;
			}
			break;
		}
	}

	return(dstIndex);
}


/*--- Physfs driver ---*/

static PHYSFS_sint64 ROFS_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 len)
{
	ROFSfileinfo *finfo = (ROFSfileinfo *) io->opaque;
	ROFSentry *entry = finfo->entry;
	PHYSFS_uint32 curPos = finfo->curPos;
	PHYSFS_uint8 *dstBuffer = (PHYSFS_uint8 *) buf;
	int srcLength, dstLength;
	PHYSFS_sint64 retval = 0;
	PHYSFS_sint64 maxread = (PHYSFS_sint64) len;
	PHYSFS_sint64 avail = entry->size - finfo->curPos;

	if (avail < maxread)
		maxread = avail;

	BAIL_IF_ERRPASS(maxread == 0, 0);    /* quick rejection. */

	dstLength = maxread;
	while (dstLength>0) {
		/* Limit to number of bytes to read */
		srcLength = entry->block_length[finfo->decrypt.curblock] - finfo->decrypt.curoffset;

		if (entry->compression) {
			/* read max srcLength, or depack max 32K */
			int dstDepacked;

			/* Read till end of current block */
			dstDepacked = rofs_depack_block(finfo, dstBuffer, srcLength, dstLength);

			dstBuffer += dstDepacked;
			finfo->curPos += dstDepacked;
			dstLength -= dstDepacked;
		} else {
			int i;

			if (srcLength > dstLength) {
				srcLength = dstLength;
			}

			retval = finfo->io->read(finfo->io, dstBuffer, srcLength);

			for (i=0; i<retval; i++) {
				PHYSFS_uint8 c = rofs_decrypt_byte(finfo, *dstBuffer);
				*dstBuffer++ = c;
			}

			finfo->decrypt.curoffset += (PHYSFS_uint32) retval;
			finfo->curPos += (PHYSFS_uint32) retval;
			dstLength -= (PHYSFS_uint32) retval;
		}

		/* Next block ? */
		if (finfo->decrypt.curoffset >= entry->block_length[finfo->decrypt.curblock]) {
			finfo->decrypt.curblock++;
			rofs_decrypt_block_init(finfo);
			if (entry->compression) {
				rofs_depack_block_init(finfo);
			}
		}
	}

	return finfo->curPos - curPos;
} /* ROFS_read */


static PHYSFS_sint64 ROFS_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
	BAIL(PHYSFS_ERR_READ_ONLY, -1);
} /* ROFS_write */


static PHYSFS_sint64 ROFS_tell(PHYSFS_Io *io)
{
	return ((ROFSfileinfo *) io->opaque)->curPos;
} /* ROFS_tell */


static int ROFS_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
	ROFSfileinfo *finfo = (ROFSfileinfo *) io->opaque;
	ROFSentry *entry = finfo->entry;
	int rc = 1, i, srcBlock, dstBlock, remaining;

	BAIL_IF(offset < 0, PHYSFS_ERR_INVALID_ARGUMENT, 0);
	BAIL_IF(offset > entry->size, PHYSFS_ERR_PAST_EOF, 0);

	/* Skip to a given 32KB block */
	srcBlock = finfo->curPos >> 15;
	dstBlock = offset >> 15;
	if ((srcBlock != dstBlock) || (offset < finfo->curPos)) {
		PHYSFS_uint64 startOffset;

		finfo->decrypt.curblock = dstBlock;

		rofs_decrypt_block_init(finfo);
		if (entry->compression) {
			rofs_depack_block_init(finfo);
		}

		/* Seek to start of block */
		startOffset = 0;
		for (i=0; i<dstBlock; i++) {
			startOffset += entry->block_length[i];
		}

		finfo->curPos = startOffset;

		finfo->io->seek(finfo->io, entry->startPos + startOffset);
	}

	if (!rc) {
		return(0);
	}

	/* Then advance in the block */
	remaining = offset - finfo->curPos;
	if (entry->compression) {
		for (i=0; i<remaining; i++) {
			PHYSFS_uint8 dummy;
			if (!rofs_depack_block(finfo, &dummy, 32768, 1)) {
				return(0);
			}
		}
	} else {
		/* Run decryption steps */
		for (i=0; i<remaining; i++) {
			PHYSFS_uint8 dummy;

			finfo->io->read(finfo->io, &dummy, 1);
			dummy = rofs_decrypt_byte(finfo, dummy);
		}
		finfo->curPos += i;
		finfo->decrypt.curoffset += i;
	}

	return(rc);
} /* ROFS_seek */


static PHYSFS_sint64 ROFS_length(PHYSFS_Io *io)
{
	ROFSfileinfo *finfo = (ROFSfileinfo *) io->opaque;
	return((PHYSFS_sint64) finfo->entry->size);
} /* ROFS_length */


static PHYSFS_Io *ROFS_duplicate(PHYSFS_Io *_io)
{
    ROFSfileinfo *origfinfo = (ROFSfileinfo *) _io->opaque;
    PHYSFS_Io *io = NULL;
    PHYSFS_Io *retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
    ROFSfileinfo *finfo = (ROFSfileinfo *) allocator.Malloc(sizeof (ROFSfileinfo));
    GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, ROFS_duplicate_failed);
    GOTO_IF(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, ROFS_duplicate_failed);

    io = origfinfo->io->duplicate(origfinfo->io);
    if (!io) goto ROFS_duplicate_failed;
    finfo->io = io;
    finfo->entry = origfinfo->entry;
    finfo->curPos = 0;

		/* Init decryption */
	finfo->decrypt.curblock = 0;
	rofs_decrypt_block_init(finfo);

	/* Init decompression */
	if (finfo->entry->compression) {
		rofs_depack_block_init(finfo);
	}

	memcpy(retval, _io, sizeof (PHYSFS_Io));
    retval->opaque = finfo;
    return retval;

ROFS_duplicate_failed:
    if (finfo != NULL) allocator.Free(finfo);
    if (retval != NULL) allocator.Free(retval);
    if (io != NULL) io->destroy(io);
    return NULL;
} /* ROFS_duplicate */


static int ROFS_flush(PHYSFS_Io *io) { return 1;  /* no write support. */ }


static void ROFS_destroy(PHYSFS_Io *io)
{
	ROFSfileinfo *finfo = (ROFSfileinfo *) io->opaque;
	finfo->io->destroy(finfo->io);

	if (finfo->depack.lzss_dict) {
		allocator.Free(finfo->depack.lzss_dict);
		finfo->depack.lzss_dict = NULL;
	}

	allocator.Free(finfo);
	allocator.Free(io);
} /* ROFS_destroy */


static const PHYSFS_Io ROFS_Io =
{
	0x12345678, NULL,
	ROFS_read,
	ROFS_write,
	ROFS_seek,
	ROFS_tell,
	ROFS_length,
	ROFS_duplicate,
	ROFS_flush,
	ROFS_destroy
};


/* All rofs<n>.dat files start with this */
static const char rofs_id[21]={
	3, 0, 0, 0,
	1, 0, 0, 0,
	4, 0, 0, 0,
	0, 1, 1, 0,
	0, 4, 0, 0,
	0
};

static int isRofs(PHYSFS_Io *io)
{
	PHYSFS_uint8 buf[21];

	/* Check if known header */
	BAIL_IF_ERRPASS(!io->seek(io, 0), 0);

	if (io->read(io, buf, sizeof(rofs_id)) != sizeof(rofs_id))
		return 0;

	if (memcmp(buf, rofs_id, sizeof(rofs_id)) != 0)
		return 0;

	return(1);
} /* isRofs */


static int rofs_read_filename(PHYSFS_Io *io, char *dst, PHYSFS_uint32 dst_len)
{
	int i = 0;

	memset(dst, '\0', dst_len);

	do {
		char c;

		if (!__PHYSFS_readAll(io, &c, 1))
			return(0);

		dst[i] = c;
	} while (dst[i++] != '\0');

	return(1);
}


static ROFSentry *rofs_load_entry(ROFSinfo *info)
{
    PHYSFS_Io *io = info->io;
	rofs_dat_file_t fileHeader;
	char shortname[16];
	PHYSFS_sint64 location;
	int i;
	ROFSentry entry, *retval;

    memset(&entry, '\0', sizeof (entry));

	BAIL_IF_ERRPASS(!readui32(io, &entry.startPos), 0);
	entry.startPos <<= 3;

	BAIL_IF_ERRPASS(!readui32(io, &entry.size), 0);
	entry.size = 0; /* This was compressed size */

	if (!rofs_read_filename(io, shortname, sizeof(shortname))) {
		return 0;
	}

	/* Generate long filename */
	i = snprintf(entry.name, sizeof(entry.name), "%s/%s/%s", info->dirs[0].name,
			info->dirs[1].name, shortname);

	/* Now go read real file size */
	location = io->tell(io);

	BAIL_IF_ERRPASS(!io->seek(io, entry.startPos), 0);

	io->read(io, &fileHeader, sizeof(fileHeader));

	fileHeader.offset = PHYSFS_swapULE16(fileHeader.offset);
	fileHeader.num_keys = PHYSFS_swapULE16(fileHeader.num_keys);
	fileHeader.length = PHYSFS_swapULE32(fileHeader.length);
	for (i=0; i<8; i++) {
		fileHeader.ident[i] ^= fileHeader.ident[7];
	}

	entry.size = fileHeader.length;

	/*--- Decryption info ---*/
	entry.num_blocks = fileHeader.num_keys;
	entry.block_alloc = (PHYSFS_uint32 *) allocator.Malloc(fileHeader.num_keys*2*sizeof(PHYSFS_uint32));
    BAIL_IF(!entry.block_alloc, PHYSFS_ERR_OUT_OF_MEMORY, 0);

	io->read(io, entry.block_alloc, fileHeader.num_keys*2*sizeof(PHYSFS_uint32));

	entry.block_startkey = entry.block_alloc;
	entry.block_length = &entry.block_alloc[fileHeader.num_keys];
	for (i=0; i<entry.num_blocks; i++) {
		entry.block_startkey[i] = PHYSFS_swapULE32(entry.block_startkey[i]);
		entry.block_length[i] = PHYSFS_swapULE32(entry.block_length[i]);
	}

	/*--- Decompression info ---*/
	entry.compression = (strcmp("Hi_Comp", fileHeader.ident)==0);

	/* Fix offset to start of encrypted/packed data */
	entry.startPos += fileHeader.offset;

	if (!io->seek(io, location))
		goto failed;

    retval = (ROFSentry *) __PHYSFS_DirTreeAdd(&info->tree, entry.name, 0);
	GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, failed);

    /* Move the data we already read into place in the official object. */
    memcpy(((PHYSFS_uint8 *) retval) + sizeof (__PHYSFS_DirTreeEntry),
           ((PHYSFS_uint8 *) &entry) + sizeof (__PHYSFS_DirTreeEntry),
           sizeof (*retval) - sizeof (__PHYSFS_DirTreeEntry));

	return retval;  /* success. */

failed:
	allocator.Free(entry.block_alloc);
	return NULL;  /* failure. */
} /* rofs_load_entry */


static int rofs_load_header(ROFSinfo *info)
{
    PHYSFS_Io *io = info->io;
	PHYSFS_uint32 dir_location, dir_length;

	/* Till now all tested rofs.dat file archives have 2 levels */
	if (!rofs_read_filename(io, info->dirs[0].name, sizeof(info->dirs[0].name))) {
		return 0;
	}

	BAIL_IF_ERRPASS(!readui32(io, &dir_location), -1);
	dir_location <<= 3;

	BAIL_IF_ERRPASS(!readui32(io, &dir_length), -1);

	if (!rofs_read_filename(io, info->dirs[1].name, sizeof(info->dirs[1].name))) {
		return 0;
	}

	info->dirs[0].startPos = info->dirs[1].startPos = dir_location;
	info->dirs[0].size = info->dirs[1].size = dir_length;

	return(1);
}


static int rofs_load_entries(ROFSinfo *info)
{
    PHYSFS_Io *io = info->io;
	int i;

	BAIL_IF_ERRPASS(!io->seek(io, sizeof(rofs_id)), 0);
	BAIL_IF_ERRPASS(!rofs_load_header(info), 0);

	BAIL_IF_ERRPASS(!io->seek(io, info->dirs[0].startPos), 0);
	BAIL_IF_ERRPASS(!readui32(io, &info->entryCount), -1);

	for (i = 0; i < info->entryCount; i++)
	{
		ROFSentry *entry = rofs_load_entry(info);
        BAIL_IF_ERRPASS(!entry, 0);
	} /* for */

	return 1;
} /* rofs_load_entries */


static void *ROFS_openArchive(PHYSFS_Io *io, const char *name, int forWriting, int *claimed)
{
	ROFSinfo *info = NULL;

	assert(io != NULL);  /* shouldn't ever happen. */

	BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);
	BAIL_IF_ERRPASS(!isRofs(io), NULL);

	*claimed = 1;

	info = (ROFSinfo *) allocator.Malloc(sizeof (ROFSinfo));
	BAIL_IF(!info, PHYSFS_ERR_OUT_OF_MEMORY, NULL);
	memset(info, '\0', sizeof (ROFSinfo));

	info->io = io;

	if (!__PHYSFS_DirTreeInit(&info->tree, sizeof (ROFSentry), 1, 1))
        goto ROFS_openarchive_failed;

	if (!rofs_load_entries(info))
		goto ROFS_openarchive_failed;

	assert(info->tree.root->sibling == NULL);

	return info;

ROFS_openarchive_failed:
	if (info != NULL)
		allocator.Free(info);

	return NULL;
} /* ROFS_openArchive */


static inline ROFSentry *rofs_find_entry(ROFSinfo *info, const char *path)
{
    return (ROFSentry *) __PHYSFS_DirTreeFind(&info->tree, path);
} /* rofs_find_entry */


static PHYSFS_Io *ROFS_openRead(void *opaque, const char *filename)
{
	PHYSFS_Io *retval = NULL;
	ROFSinfo *info = (ROFSinfo *) opaque;
	ROFSentry *entry = rofs_find_entry(info, filename);
	ROFSfileinfo *finfo = NULL;

	BAIL_IF_ERRPASS(!entry, NULL);

	retval = (PHYSFS_Io *) allocator.Malloc(sizeof (PHYSFS_Io));
	GOTO_IF(!retval, PHYSFS_ERR_OUT_OF_MEMORY, ROFS_openRead_failed);

	finfo = (ROFSfileinfo *) allocator.Malloc(sizeof (ROFSfileinfo));
	GOTO_IF(!finfo, PHYSFS_ERR_OUT_OF_MEMORY, ROFS_openRead_failed);
	memset(finfo, '\0', sizeof (ROFSfileinfo));

	finfo->io = info->io->duplicate(info->io);
	GOTO_IF_ERRPASS(!finfo->io, ROFS_openRead_failed);

	if (!finfo->io->seek(finfo->io, entry->startPos))
		goto ROFS_openRead_failed;

	finfo->entry = entry;
	finfo->curPos = 0;

	/* Init decryption */
	finfo->decrypt.curblock = 0;
	rofs_decrypt_block_init(finfo);

	/* Init decompression */
	if (entry->compression) {
		rofs_depack_block_init(finfo);
	}

	memcpy(retval, &ROFS_Io, sizeof (PHYSFS_Io));
	retval->opaque = finfo;

	return retval;

ROFS_openRead_failed:
	if (finfo != NULL)
	{
		if (finfo->io != NULL)
			finfo->io->destroy(finfo->io);

		allocator.Free(finfo);
	} /* if */

	if (retval != NULL)
		allocator.Free(retval);

	return NULL;
} /* ROFS_openRead */


static void ROFS_closeArchive(void *opaque)
{
	ROFSinfo *info = (ROFSinfo *) opaque;

	if (!info)
		return;

	if (info->io)
		info->io->destroy(info->io);

    __PHYSFS_DirTreeDeinit(&info->tree);

	allocator.Free(info);
} /* ROFS_closeArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_ROFS =
{
	CURRENT_PHYSFS_ARCHIVER_API_VERSION,
	{
		"ROFS",
		"Resident Evil 3 ROFS format",
		"Patrice Mandin <patmandin@gmail.com>",
		"http://pmandin.atari.org/",
		0	/* supportsSymlinks */
	},
	ROFS_openArchive,
	UNPK_enumerate,
	ROFS_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
	UNPK_stat,
	ROFS_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_ROFS */

/* end of physfs_archiver_rofs.c ... */
