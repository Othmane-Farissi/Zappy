#include "world.h"
#include <string.h>

static const char *resource_names[RESOURCE_COUNT] = {
    "nourriture", "linemate", "deraumere", "sibur",
    "mendiane", "phiras", "thystame"
};

int resource_index(const char *name)
{
    int resource;

    resource = 0;
    while (resource < RESOURCE_COUNT)
    {
        if (strcmp(name, resource_names[resource]) == 0)
            return (resource);
        resource++;
    }
    return (-1);
}

t_square *player_square(t_server *server, t_player *player)
{
    return (&server->map.squares[player->y * server->map.width + player->x]);
}

void append_square_contents(t_server *server, t_player *viewer,
    int x, int y, char *response, size_t response_size)
{
    t_square *square;
    t_player *player;
    int resource;
    int count;

    x = (x + server->map.width) % server->map.width;
    y = (y + server->map.height) % server->map.height;
    square = &server->map.squares[y * server->map.width + x];
    resource = 0;
    while (resource < RESOURCE_COUNT)
    {
        count = 0;
        while (count < square->resources[resource])
        {
            strncat(response, resource_names[resource],
                response_size - strlen(response) - 1);
            strncat(response, " ", response_size - strlen(response) - 1);
            count++;
        }
        resource++;
    }
    player = server->players;
    while (player != NULL)
    {
        if (player != viewer && player->team != NULL &&
            player->x == x && player->y == y)
            strncat(response, "player ", response_size - strlen(response) - 1);
        player = player->next;
    }
}

void view_offset(t_direction direction, int depth, int side, int *x, int *y)
{
    if (direction == NORTH)
    {
        *x = side;
        *y = -depth;
    }
    else if (direction == EAST)
    {
        *x = depth;
        *y = side;
    }
    else if (direction == SOUTH)
    {
        *x = -side;
        *y = depth;
    }
    else
    {
        *x = -depth;
        *y = -side;
    }
}

int sound_direction(t_server *server, t_player *receiver, t_player *sender)
{
    int dx;
    int dy;
    int global;
    int facing;
    int delta;

    dx = sender->x - receiver->x;
    dy = sender->y - receiver->y;
    if (dx > server->map.width / 2)
        dx -= server->map.width;
    else if (dx < -(server->map.width / 2))
        dx += server->map.width;
    if (dy > server->map.height / 2)
        dy -= server->map.height;
    else if (dy < -(server->map.height / 2))
        dy += server->map.height;
    if (dx == 0 && dy == 0)
        return (0);
    if (dy < 0)
        global = dx > 0 ? 1 : (dx < 0 ? 7 : 0);
    else if (dy > 0)
        global = dx > 0 ? 3 : (dx < 0 ? 5 : 4);
    else
        global = dx > 0 ? 2 : 6;
    facing = receiver->direction * 2;
    delta = (global - facing + 8) % 8;
    return ((8 - delta) % 8 + 1);
}
