#include <stdio.h>
#include "physfs.h"

#define TEST_VERSION_MAJOR  0
#define TEST_VERSION_MINOR  1
#define TEST_VERSION_PATCH  0

void output_versions(void)
{
    PHYSFS_Version compiled;
    PHYSFS_Version linked;

    PHYSFS_VERSION(&compiled);
    PHYSFS_getLinkedVersion(&linked);

    printf("test_physfs version %d.%d.%d.\n"
           " Compiled against PhysicsFS version %d.%d.%d,\n"
           " and linked against %d.%d.%d.\n\n",
            TEST_VERSION_MAJOR, TEST_VERSION_MINOR, TEST_VERSION_PATCH,
            compiled.major, compiled.minor, compiled.patch,
            linked.major, linked.minor, linked.patch);
} /* output_versions */


void output_archivers(void)
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
        } /* for */
    } /* else */
} /* output_archivers */


int main(int argc, char **argv)
{
    if (!PHYSFS_init(argv[0]))
    {
        printf("PHYSFS_init() failed!\n  reason: %s\n", PHYSFS_getLastError());
        return(1);
    } /* if */

    output_versions();
    output_archivers();

    return(0);
} /* main */

/* end of test_physfs.c ... */

