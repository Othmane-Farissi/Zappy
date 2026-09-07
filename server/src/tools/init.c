#include "zappy.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void init_resources(t_square *square)
{
    int resource;

    resource = 0;
    while (resource < RESOURCE_COUNT)
    {
        square->resources[resource] = rand() % 4;
        resource++;
    }
}

int init_server(t_server *server)
{
    size_t count;
    size_t i;

    srand((unsigned int)time(NULL));
    count = (size_t)server->map.width * (size_t)server->map.height;
    server->map.squares = calloc(count, sizeof(*server->map.squares));
    if (server->map.squares == NULL)
        return (1);
    i = 0;
    while (i < count)
    {
        init_resources(&server->map.squares[i]);
        i++;
    }
    i = 0;
    while (i < (size_t)server->teamcount)
    {
        server->teams[i].capacity = server->max_team_players;
        i++;
    }
    return (0);
}

void destroy_server(t_server *server)
{
    t_player *player;
    t_player *next;
    t_egg *egg;
    t_egg *next_egg;
    int i;

    player = server->players;
    while (player != NULL)
    {
        next = player->next;
        free(player);
        player = next;
    }
    egg = server->eggs;
    while (egg != NULL)
    {
        next_egg = egg->next;
        free(egg);
        egg = next_egg;
    }
    i = 0;
    while (i < server->teamcount)
    {
        free(server->teams[i].name);
        i++;
    }
    free(server->map.squares);
}