#include "zappy.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_number(const char *value, int *result)
{
    char *end;
    long number;

    errno = 0;
    number = strtol(value, &end, 10);
    if (errno != 0 || *value == '\0' || *end != '\0' || number < 1 || number > INT_MAX)
        return (1);
    *result = (int)number;
    return (0);
}

static char *copy_string(const char *value)
{
    size_t length;
    char *copy;

    length = strlen(value) + 1;
    copy = malloc(length);
    if (copy != NULL)
        memcpy(copy, value, length);
    return (copy);
}

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s -p <port> -x <width> -y <height> -n <team> [team...] -c <nb> -t <t>\n", program);
}

int parse_params(int argc, char **argv, t_server *server)
{
    int i;
    int value;

    memset(server, 0, sizeof(*server));
    i = 1;
    while (i < argc)
    {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "-x") == 0 ||
            strcmp(argv[i], "-y") == 0 || strcmp(argv[i], "-c") == 0 ||
            strcmp(argv[i], "-t") == 0)
        {
            if (i + 1 >= argc || parse_number(argv[i + 1], &value) != 0)
                return (print_usage(argv[0]), 1);
            if (argv[i][1] == 'p') server->port = value;
            if (argv[i][1] == 'x') server->map.width = value;
            if (argv[i][1] == 'y') server->map.height = value;
            if (argv[i][1] == 'c') server->max_team_players = value;
            if (argv[i][1] == 't') server->timeunit = value;
            i += 2;
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            i++;
            while (i < argc && argv[i][0] != '-')
            {
                if (server->teamcount == MAX_TEAMS)
                    return (print_usage(argv[0]), 1);
                server->teams[server->teamcount].name = copy_string(argv[i]);
                if (server->teams[server->teamcount].name == NULL)
                    return (1);
                server->teamcount++;
                i++;
            }
        }
        else
            return (print_usage(argv[0]), 1);
    }
    if (server->port > 65535 || server->map.width <= 0 || server->map.height <= 0 ||
        server->teamcount == 0 || server->max_team_players <= 0 || server->timeunit <= 0)
        return (print_usage(argv[0]), 1);
    return (0);
}