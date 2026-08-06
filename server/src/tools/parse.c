#include "zappy.h"

int validate_args(int argc, char **argv)
{
    int i;

    i = 0;
    if (!g_server.map->width || !g_server.map->height || !g_server.port ||
         !g_server.timeunit == -1 || !g_server.teams || !g_server.max_team_players)
        return (1);
    return 0;
}