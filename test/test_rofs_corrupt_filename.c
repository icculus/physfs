/**
 * Regression test for the ROFS archiver: a directory-name entry in a
 * rofs.dat file is just a run of bytes terminated by a NUL byte, with no
 * length prefix anywhere in the format. A corrupt (or deliberately hostile)
 * archive that omits the terminator for long enough will make
 * rofs_read_filename() walk right off the end of its destination buffer,
 * which lives inside a heap allocation. This writes such a file and mounts
 * it, and expects PHYSFS_mount() to fail cleanly instead of corrupting
 * memory.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "physfs.h"

static const char *make_evil_archive(void)
{
    static const char *fname = "test_rofs_corrupt_filename.dat";
    static const unsigned char rofs_id[21] = {
        3, 0, 0, 0,
        1, 0, 0, 0,
        4, 0, 0, 0,
        0, 1, 1, 0,
        0, 4, 0, 0,
        0
    };
    FILE *f = fopen(fname, "wb");
    int i;

    if (!f)
    {
        fprintf(stderr, "failed to create %s for writing\n", fname);
        return NULL;
    }

    fwrite(rofs_id, 1, sizeof (rofs_id), f);

    /* This stands in for the first directory name in the header. It's way
       longer than the 48 bytes ROFSentry.name has room for, and never
       includes a NUL byte, so a naive byte-at-a-time reader will keep
       going well past the end of that field. */
    for (i = 0; i < 4096; i++)
        fputc('A', f);
    fputc('\0', f);

    fclose(f);
    return fname;
} /* make_evil_archive */

int main(int argc, char **argv)
{
    const char *fname;
    int rc;

    if (!PHYSFS_init(argv[0]))
    {
        fprintf(stderr, "PHYSFS_init failed: %s\n",
                PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
        return 1;
    } /* if */

    fname = make_evil_archive();
    if (!fname)
    {
        PHYSFS_deinit();
        return 1;
    } /* if */

    /* This used to overrun a heap buffer while parsing the header; it
       should now just reject the archive as corrupt. */
    rc = PHYSFS_mount(fname, NULL, 1);

    remove(fname);
    PHYSFS_deinit();

    if (rc)
    {
        fprintf(stderr, "PHYSFS_mount unexpectedly succeeded on a "
                        "corrupt ROFS archive!\n");
        return 1;
    } /* if */

    printf("ok: corrupt ROFS filename was rejected instead of "
           "overrunning the buffer.\n");
    return 0;
} /* main */
