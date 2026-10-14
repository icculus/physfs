/*
 * cue+bin support routines for PhysicsFS.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 * This file was written by Tomas Jakobsson, based on the ISO9660 archiver by Ryan C. Gordon.
 */

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"

#if PHYSFS_SUPPORTS_CUEBIN

typedef struct
{
    __PHYSFS_DirTree tree;
    PHYSFS_Io *io;
} UNPKinfo;

typedef struct
{
    __PHYSFS_DirTreeEntry tree;
    PHYSFS_uint64 startPos;
    PHYSFS_uint64 size;
    PHYSFS_sint64 ctime;
    PHYSFS_sint64 mtime;
} UNPKentry;

typedef struct CUEBINTrack
{
    PHYSFS_sint64 index0;
    PHYSFS_sint64 index1;
    PHYSFS_uint64 sector_count;
    PHYSFS_uint32 sector_size;
    PHYSFS_uint32 logical_sector_size;
    PHYSFS_uint32 data_offset;
} CUEBINTrack;

typedef struct CUEBINFile
{
    char *filename;
    PHYSFS_Io *io;
    CUEBINTrack *tracks;
    int track_count;
    PHYSFS_uint64 sector_count;
} CUEBINFile;

typedef struct CUEBINInfo
{
    CUEBINFile *files;
    int file_count;
    PHYSFS_uint64 position;
} CUEBINInfo;


static void cueBinFreeInfo(CUEBINInfo *info);
static const PHYSFS_Io CUEBIN_Io;


static PHYSFS_sint64 CUEBIN_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 len)
{
    CUEBINInfo *info = (CUEBINInfo *) io->opaque;
    PHYSFS_uint8 *dst = (PHYSFS_uint8 *) buf;
    PHYSFS_uint64 remaining = len;

    while (remaining)
    {
        PHYSFS_uint64 sector;
        PHYSFS_uint64 sector_offset;
        PHYSFS_uint64 physical_offset;
        PHYSFS_uint64 available;
        PHYSFS_uint64 position = info->position;
        CUEBINFile *file = NULL;
        CUEBINTrack *track = NULL;
        int i;
        int j;

        for (i = 0; i < info->file_count; i++)
        {
            PHYSFS_uint64 file_byte_length = 0;

            for (j = 0; j < info->files[i].track_count; j++)
            {
                CUEBINTrack *current = &info->files[i].tracks[j];
                file_byte_length += current->sector_count * current->logical_sector_size;
            }

            if (position < file_byte_length)
            {
                file = &info->files[i];
                break;
            }

            position -= file_byte_length;
        }

        BAIL_IF(!file, PHYSFS_ERR_PAST_EOF, -1);

        for (i = 0; i < file->track_count; i++)
        {
            CUEBINTrack *current = &file->tracks[i];
            PHYSFS_uint64 track_byte_length = current->sector_count * current->logical_sector_size;

            if (position < track_byte_length)
            {
                track = current;
                break;
            }

            position -= track_byte_length;
        }

        BAIL_IF(!track, PHYSFS_ERR_PAST_EOF, -1);

        sector = position / track->logical_sector_size;
        sector_offset = position % track->logical_sector_size;
        physical_offset = (track->index0 + sector) * track->sector_size + track->data_offset + sector_offset;
        available = track->logical_sector_size - sector_offset;

        if (available > remaining)
            available = remaining;

        BAIL_IF(!file->io->seek(file->io, physical_offset), PHYSFS_ERR_IO, -1);
        BAIL_IF(file->io->read(file->io, dst, available) != (PHYSFS_sint64) available, PHYSFS_ERR_IO, -1);

        info->position += available;
        dst += available;
        remaining -= available;
    }

    return (PHYSFS_sint64) len;
} /* CUEBIN_read */


static PHYSFS_sint64 CUEBIN_write(PHYSFS_Io *io, const void *b, PHYSFS_uint64 len)
{
    BAIL(PHYSFS_ERR_READ_ONLY, -1);
} /* CUEBIN_write */


static PHYSFS_sint64 CUEBIN_tell(PHYSFS_Io *io)
{
    CUEBINInfo *info = (CUEBINInfo *) io->opaque;

    return (PHYSFS_sint64) info->position;
} /* CUEBIN_tell */


static int CUEBIN_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    CUEBINInfo *info = (CUEBINInfo *) io->opaque;

    BAIL_IF(offset > (PHYSFS_uint64) io->length(io), PHYSFS_ERR_PAST_EOF, 0);

    info->position = offset;

    return 1;
} /* CUEBIN_seek */


static PHYSFS_sint64 CUEBIN_length(PHYSFS_Io *io)
{
    CUEBINInfo *info = (CUEBINInfo *) io->opaque;
    PHYSFS_uint64 byte_length = 0;
    int i;
    int j;

    for (i = 0; i < info->file_count; i++)
    {
        CUEBINFile *file = &info->files[i];

        for (j = 0; j < file->track_count; j++)
        {
            CUEBINTrack *track = &file->tracks[j];
            byte_length += track->sector_count * track->logical_sector_size;
        }
    }

    return (PHYSFS_sint64) byte_length;
} /* CUEBIN_length */


static PHYSFS_Io *CUEBIN_duplicate(PHYSFS_Io *io)
{
    CUEBINInfo *info = (CUEBINInfo *) io->opaque;
    CUEBINInfo *duplicate_info = NULL;
    PHYSFS_Io *duplicate_io = NULL;
    int i;

    duplicate_info = allocator.Malloc(sizeof(CUEBINInfo));
    BAIL_IF(!duplicate_info, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    duplicate_info->files = NULL;
    duplicate_info->file_count = 0;
    duplicate_info->position = info->position;

    duplicate_info->files = allocator.Malloc(sizeof(CUEBINFile) * info->file_count);
    if (!duplicate_info->files)
    {
        cueBinFreeInfo(duplicate_info);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    }

    duplicate_info->file_count = info->file_count;
    memset(duplicate_info->files, 0, sizeof(CUEBINFile) * duplicate_info->file_count);

    for (i = 0; i < info->file_count; i++)
    {
        CUEBINFile *source = &info->files[i];
        CUEBINFile *destination = &duplicate_info->files[i];

        destination->filename = allocator.Malloc(strlen(source->filename) + 1);
        if (!destination->filename)
        {
            cueBinFreeInfo(duplicate_info);
            BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
        }

        strcpy(destination->filename, source->filename);

        destination->track_count = source->track_count;
        destination->sector_count = source->sector_count;

        destination->tracks = allocator.Malloc(sizeof(CUEBINTrack) * source->track_count);
        if (!destination->tracks)
        {
            cueBinFreeInfo(duplicate_info);
            BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
        }

        memcpy(destination->tracks, source->tracks, sizeof(CUEBINTrack) * source->track_count);

        destination->io = source->io->duplicate(source->io);
        if (!destination->io)
        {
            cueBinFreeInfo(duplicate_info);
            BAIL(PHYSFS_ERR_IO, NULL);
        }
    }

    duplicate_io = (PHYSFS_Io *) allocator.Malloc(sizeof(PHYSFS_Io));
    if (!duplicate_io)
    {
        cueBinFreeInfo(duplicate_info);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    }

    memcpy(duplicate_io, &CUEBIN_Io, sizeof(PHYSFS_Io));
    duplicate_io->opaque = duplicate_info;

    return duplicate_io;
} /* CUEBIN_duplicate */


static int CUEBIN_flush(PHYSFS_Io *io)
{
    BAIL(PHYSFS_ERR_READ_ONLY, -1);
} /* CUEBIN_flush */


static void CUEBIN_destroy(PHYSFS_Io *io)
{
    CUEBINInfo *info = (CUEBINInfo *) io->opaque;

    cueBinFreeInfo(info);
    allocator.Free(io);
} /* CUEBIN_destroy */


static const PHYSFS_Io CUEBIN_Io =
{
    CURRENT_PHYSFS_IO_API_VERSION, NULL,
    CUEBIN_read,
    CUEBIN_write,
    CUEBIN_seek,
    CUEBIN_tell,
    CUEBIN_length,
    CUEBIN_duplicate,
    CUEBIN_flush,
    CUEBIN_destroy
};


static void cueBinFreeInfo(CUEBINInfo *info)
{
    int i;

    if (!info)
        return;

    for (i = 0; i < info->file_count; i++)
    {
        CUEBINFile *file = &info->files[i];

        if (file->io)
            file->io->destroy(file->io);

        if (file->tracks)
            allocator.Free(file->tracks);

        if (file->filename)
            allocator.Free(file->filename);
    }

    if (info->files)
        allocator.Free(info->files);

    allocator.Free(info);
} /* cueBinFreeInfo */


static void *cueBinReadFile(PHYSFS_Io *io, PHYSFS_uint64 *length)
{
    PHYSFS_sint64 len = io->length(io);
    void *buffer = NULL;

    BAIL_IF(len < 0, PHYSFS_ERR_IO, NULL);

    buffer = (char *) allocator.Malloc((size_t) len + 1);
    BAIL_IF(!buffer, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    if (!__PHYSFS_readAll(io, buffer, (PHYSFS_uint64) len))
    {
        allocator.Free(buffer);
        BAIL(PHYSFS_ERR_IO, NULL);
    }

    ((char *) buffer)[len] = '\0';

    if (length)
        *length = len;

    return buffer;
} /* cueBinReadFile */


static int cueBinParse(CUEBINInfo *info, char *cue)
{
    CUEBINFile *file = NULL;
    CUEBINTrack *track = NULL;
    char *line = cue;

    while (*line)
    {
        while (*line == ' ' || *line == '\t')
            line++;

        if (strncmp(line, "FILE ", 5) == 0)
        {
            CUEBINFile *files = NULL;
            char filename[256];

            if (sscanf(line + 5, "\"%255[^\"]\"", filename) != 1)
                BAIL(PHYSFS_ERR_CORRUPT, 0);

            files = allocator.Malloc(sizeof(CUEBINFile) * (info->file_count + 1));
            BAIL_IF(!files, PHYSFS_ERR_OUT_OF_MEMORY, 0);

            if (info->files)
            {
                memcpy(files, info->files, sizeof(CUEBINFile) * info->file_count);
                allocator.Free(info->files);
            }

            info->files = files;
            file = &info->files[info->file_count];

            file->filename = NULL;
            file->io = NULL;
            file->tracks = NULL;
            file->track_count = 0;
            file->sector_count = 0;

            file->filename = allocator.Malloc(strlen(filename) + 1);
            BAIL_IF(!file->filename, PHYSFS_ERR_OUT_OF_MEMORY, 0);

            strcpy(file->filename, filename);
            info->file_count++;
            track = NULL;
        }
        else if (strncmp(line, "TRACK ", 6) == 0)
        {
            CUEBINTrack *tracks = NULL;
            int track_number;
            char mode[32];

            BAIL_IF(!file, PHYSFS_ERR_CORRUPT, 0);

            if (sscanf(line + 6, "%d %31s", &track_number, mode) != 2)
                BAIL(PHYSFS_ERR_CORRUPT, 0);

            tracks = allocator.Malloc(sizeof(CUEBINTrack) * (file->track_count + 1));
            BAIL_IF(!tracks, PHYSFS_ERR_OUT_OF_MEMORY, 0);

            if (file->tracks)
            {
                memcpy(tracks, file->tracks, sizeof(CUEBINTrack) * file->track_count);
                allocator.Free(file->tracks);
            }

            file->tracks = tracks;
            track = &file->tracks[file->track_count];

            track->index0 = -1;
            track->index1 = -1;
            track->sector_count = 0;
            track->sector_size = 0;
            track->logical_sector_size = 0;
            track->data_offset = 0;

            if (strcmp(mode, "MODE1/2352") == 0)
            {
                track->sector_size = 2352;
                track->logical_sector_size = 2048;
                track->data_offset = 16;
            }
            else if (strcmp(mode, "MODE2/2352") == 0)
            {
                track->sector_size = 2352;
                track->logical_sector_size = 2048;
                track->data_offset = 24;
            }
            else if (strcmp(mode, "AUDIO") == 0)
            {
                track->sector_size = 2352;
                track->logical_sector_size = 2352;
                track->data_offset = 0;
            }
            else
            {
                track = NULL;
                BAIL(PHYSFS_ERR_UNSUPPORTED, 0);
            }

            file->track_count++;
        }
        else if (strncmp(line, "INDEX ", 6) == 0)
        {
            PHYSFS_sint64 frame_position;
            int index;
            int minutes;
            int seconds;
            int frames;

            BAIL_IF(!track, PHYSFS_ERR_CORRUPT, 0);

            if (sscanf(line + 6, "%d %d:%d:%d", &index, &minutes, &seconds, &frames) != 4)
                BAIL(PHYSFS_ERR_CORRUPT, 0);

            BAIL_IF(minutes < 0 || seconds < 0 || seconds >= 60 || frames < 0 || frames >= 75, PHYSFS_ERR_CORRUPT, 0);

            frame_position = (((PHYSFS_sint64) minutes * 60) + seconds) * 75 + frames;

            if (index == 0)
            {
                BAIL_IF(track->index0 >= 0, PHYSFS_ERR_CORRUPT, 0);
                track->index0 = frame_position;
            }
            else if (index == 1)
            {
                BAIL_IF(track->index1 >= 0, PHYSFS_ERR_CORRUPT, 0);
                track->index1 = frame_position;
            }
            else
            {
                BAIL(PHYSFS_ERR_CORRUPT, 0);
            }

            if (track->index0 < 0 && track->index1 >= 0)
                track->index0 = track->index1;
        }

        while (*line && *line != '\n' && *line != '\r')
            line++;

        while (*line == '\r' || *line == '\n')
            line++;
    }

    return 1;
} /* cueBinParse */


static void cuebinPatchTree(__PHYSFS_DirTreeEntry *tree_entry, CUEBINInfo *info)
{
    while (tree_entry != NULL)
    {
        if (tree_entry->children != NULL)
            cuebinPatchTree(tree_entry->children, info);
        else
        {
            UNPKentry *unpk_entry = (UNPKentry *) tree_entry;
            PHYSFS_uint64 target_sector = unpk_entry->startPos / 2048;
            PHYSFS_uint64 parsed_bytes = 0;
            PHYSFS_uint64 parsed_sectors = 0;
            int i;
            int j;

            for (i = 0; i < info->file_count; i++)
            {
                CUEBINFile *cuebin_file;

                if (parsed_sectors > target_sector)
                    break;

                cuebin_file = &info->files[i];
                for (j = 0; j < cuebin_file->track_count; j++)
                {
                    CUEBINTrack *track = &cuebin_file->tracks[j];
                    PHYSFS_uint64 local_sector;

                    if (parsed_sectors > target_sector)
                        break;

                    local_sector = target_sector - parsed_sectors;

                    if (track->logical_sector_size == 2352 &&
                        local_sector >= (PHYSFS_uint64)track->index0 &&
                        local_sector < (PHYSFS_uint64)(track->index0 + track->sector_count))
                    {
                        PHYSFS_uint64 sector_count = (unpk_entry->size + 2047) / 2048;
                        unpk_entry->startPos = parsed_bytes + local_sector * 2352;
                        unpk_entry->size = sector_count * 2352;
                    }

                    parsed_bytes += track->sector_count * track->logical_sector_size;
                    parsed_sectors += track->sector_count;
                }
            }
        }
        tree_entry = tree_entry->sibling;
    }
} /* cuebinPatchTree */


static void *CUEBIN_openArchive(PHYSFS_Io *io, const char *filename,
                                int forWriting, int *claimed)
{
    CUEBINInfo *info = NULL;
    PHYSFS_Io *cuebin_io = NULL;
    PHYSFS_uint64 length = 0;
    char *cue = NULL;
    void *archive = NULL;
    int i;
    int j;

    BAIL_IF(forWriting, PHYSFS_ERR_READ_ONLY, NULL);

    info = allocator.Malloc(sizeof(CUEBINInfo));
    BAIL_IF(!info, PHYSFS_ERR_OUT_OF_MEMORY, NULL);

    info->files = NULL;
    info->file_count = 0;
    info->position = 0;

    cue = (char *) cueBinReadFile(io, &length);
    if (!cue)
    {
        cueBinFreeInfo(info);
        return NULL;
    }

    if (!cueBinParse(info, cue))
    {
        allocator.Free(cue);
        cueBinFreeInfo(info);
        return NULL;
    }

    allocator.Free(cue);

    for (i = 0; i < info->file_count; i++)
    {
        CUEBINFile *file = &info->files[i];
        PHYSFS_sint64 bin_length;
        const char *real_dir = PHYSFS_getRealDir(filename);
        char *base_dir = NULL;
        char *path = NULL;

        if (real_dir)
        {
            base_dir = allocator.Malloc(strlen(real_dir) + 1);
            if (!base_dir)
            {
                cueBinFreeInfo(info);
                BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
            }

            strcpy(base_dir, real_dir);
        }
        else
        {
            const char *slash_separator = strrchr(filename, '/');
            const char *backslash_separator = strrchr(filename, '\\');
            const char *separator = slash_separator;
            size_t directory_length;

            if (backslash_separator && (!separator || backslash_separator > separator))
                separator = backslash_separator;

            directory_length = separator ? (size_t)(separator - filename) : 0;

            base_dir = allocator.Malloc(directory_length + 1);
            if (!base_dir)
            {
                cueBinFreeInfo(info);
                BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
            }

            memcpy(base_dir, filename, directory_length);
            base_dir[directory_length] = '\0';
        }

        path = allocator.Malloc(strlen(base_dir) + strlen(PHYSFS_getDirSeparator()) + strlen(file->filename) + 1);
        if (!path)
        {
            allocator.Free(base_dir);
            cueBinFreeInfo(info);
            BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
        }

        strcpy(path, base_dir);
        strcat(path, PHYSFS_getDirSeparator());
        strcat(path, file->filename);

        file->io = __PHYSFS_createNativeIo(path, 'r');
        allocator.Free(path);
        allocator.Free(base_dir);

        if (!file->io)
        {
            cueBinFreeInfo(info);
            BAIL(PHYSFS_ERR_IO, NULL);
        }

        bin_length = file->io->length(file->io);

        if (bin_length < 0 || bin_length % 2352 != 0)
        {
            cueBinFreeInfo(info);
            BAIL(PHYSFS_ERR_CORRUPT, NULL);
        }

        file->sector_count = (PHYSFS_uint64) bin_length / 2352;

        for (j = 0; j < file->track_count; j++)
        {
            CUEBINTrack *track = &file->tracks[j];

            if (track->index0 < 0 || track->index1 < 0 || track->index1 < track->index0 || (PHYSFS_uint64) track->index0 >= file->sector_count || (PHYSFS_uint64) track->index1 >= file->sector_count)
            {
                cueBinFreeInfo(info);
                BAIL(PHYSFS_ERR_CORRUPT, NULL);
            }

            if (j > 0)
            {
                CUEBINTrack *previous = &file->tracks[j - 1];

                if (track->index0 < previous->index0 || track->index1 < previous->index1)
                {
                    cueBinFreeInfo(info);
                    BAIL(PHYSFS_ERR_CORRUPT, NULL);
                }
            }
        }

        for (j = 0; j < file->track_count; j++)
        {
            CUEBINTrack *track = &file->tracks[j];
            PHYSFS_uint64 track_end;

            if (j + 1 < file->track_count)
                track_end = (PHYSFS_uint64) file->tracks[j + 1].index0;
            else
                track_end = file->sector_count;

            if (track_end <= (PHYSFS_uint64) track->index1)
            {
                cueBinFreeInfo(info);
                BAIL(PHYSFS_ERR_CORRUPT, NULL);
            }

            track->sector_count = track_end - (PHYSFS_uint64) track->index0;
        }
    }

    cuebin_io = (PHYSFS_Io *) allocator.Malloc(sizeof(PHYSFS_Io));
    if (!cuebin_io)
    {
        cueBinFreeInfo(info);
        BAIL(PHYSFS_ERR_OUT_OF_MEMORY, NULL);
    }

    memcpy(cuebin_io, &CUEBIN_Io, sizeof(PHYSFS_Io));
    cuebin_io->opaque = info;

    archive = __PHYSFS_openISO9660(cuebin_io, filename, 0, claimed);
    if (!archive)
    {
        cuebin_io->destroy(cuebin_io);
        return NULL;
    }

    if (archive)
    {
        UNPKinfo *unpk = (UNPKinfo *) archive;
        __PHYSFS_DirTreeEntry *root = (__PHYSFS_DirTreeEntry *) unpk->tree.root;

        if (root)
            cuebinPatchTree(root->children, info);
    }

    return archive;
} /* CUEBIN_openArchive */


const PHYSFS_Archiver __PHYSFS_Archiver_CUEBIN =
{
    CURRENT_PHYSFS_ARCHIVER_API_VERSION,
    {
        "CUE",
        "CUE + BIN image file",
        "Tomas Jakobsson",
        "https://github.com/tomaz82/",
        0,  /* supportsSymlinks */
    },
    CUEBIN_openArchive,
    UNPK_enumerate,
    UNPK_openRead,
    UNPK_openWrite,
    UNPK_openAppend,
    UNPK_remove,
    UNPK_mkdir,
    UNPK_stat,
    UNPK_closeArchive
};

#endif  /* defined PHYSFS_SUPPORTS_CUEBIN */

/* end of physfs_archiver_cuebin.c ... */

