#include <stdio.h>
#include <stdlib.h>
#include <readline.h>
#include <history.h>
#include "physfs.h"

#define TEST_VERSION_MAJOR  0
#define TEST_VERSION_MINOR  1
#define TEST_VERSION_PATCH  0

static void output_versions(void)
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


static void output_archivers(void)
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

    printf("\n");
} /* output_archivers */


static int cmd_help(char *cmdstr)
{
    printf("Commands:\n"
           "  quit - exit this program.\n"
           "  help - this information.\n");
    return(1);
} /* output_cmd_help */


static int cmd_quit(char *cmdstr)
{
    return(0);
} /* cmd_quit */


typedef struct
{
    const char *cmd;
    int (*func)(char *cmdstr);
} command_info;

static command_info commands[] =
{
    {"quit", cmd_quit},
    {"q",    cmd_quit},
    {"help", cmd_help},
    {NULL, NULL}
};


static int process_command(char *complete_cmd)
{
    command_info *i;
    char *ptr = strchr(complete_cmd, ' ');
    char *cmd = NULL;
    int rc = 1;

    if (ptr == NULL)
    {
        cmd = malloc(strlen(complete_cmd) + 1);
        strcpy(cmd, complete_cmd);
    } /* if */
    else
    {
        *ptr = '\0';
        cmd = malloc(strlen(complete_cmd) + 1);
        strcpy(cmd, complete_cmd);
        *ptr = ' ';
    } /* else */

    for (i = commands; i->cmd != NULL; i++)
    {
        if (strcmp(i->cmd, cmd) == 0)
        {
            rc = i->func(complete_cmd);
            break;
        } /* if */
    } /* for */

    if (i->cmd == NULL)
        printf("Unknown command. Enter \"help\" for instructions.\n");

    free(cmd);
    return(rc);
} /* process_command */


int main(int argc, char **argv)
{
    char *buf = NULL;
    int rc = 0;

    printf("\n");

    if (!PHYSFS_init(argv[0]))
    {
        printf("PHYSFS_init() failed!\n  reason: %s\n", PHYSFS_getLastError());
        return(1);
    } /* if */

    output_versions();
    output_archivers();

    printf("Enter commands. Enter \"help\" for instructions.\n");

    do
    {
        buf = readline("> ");
        rc = process_command(buf);
        free(buf);
    } while (rc);

    return(0);
} /* main */

/* end of test_physfs.c ... */

