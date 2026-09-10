#include "gameplay.h"
#include "io.h"
#include "world.h"
#include <stdio.h>

static const int ritual_players[7] = {1, 2, 2, 4, 4, 6, 6};
static const int ritual_resources[7][RESOURCE_COUNT] = {
    {0, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 0, 0, 0},
    {0, 2, 0, 1, 0, 2, 0},
    {0, 1, 1, 2, 0, 1, 0},
    {0, 1, 2, 1, 3, 0, 0},
    {0, 1, 2, 3, 0, 1, 1},
    {0, 2, 2, 2, 2, 2, 1}
};

static void check_victory(t_server *server, t_team *team)
{
    t_player *player;
    int elevated;
    char response[128];

    if (server->winner_announced)
        return;
    elevated = 0;
    player = server->players;
    while (player != NULL)
    {
        if (player->team == team && player->level >= 8)
            elevated++;
        player = player->next;
    }
    if (elevated < 6)
        return;
    server->winner_announced = true;
    snprintf(response, sizeof(response), "equipe gagnante : %s\n", team->name);
    player = server->players;
    while (player != NULL)
    {
        send_text(player->fd, response);
        player = player->next;
    }
}

int complete_incantation(t_server *server, t_player *initiator)
{
    t_player *player;
    t_square *square;
    int level;
    int players;
    int resource;
    char response[64];

    level = initiator->level;
    if (level < 1 || level > 7)
        return (0);
    square = player_square(server, initiator);
    players = 0;
    player = server->players;
    while (player != NULL)
    {
        if (player->team != NULL && player->x == initiator->x &&
            player->y == initiator->y && player->level == level)
            players++;
        player = player->next;
    }
    if (players < ritual_players[level - 1])
        return (0);
    resource = 0;
    while (resource < RESOURCE_COUNT)
    {
        if (square->resources[resource] < ritual_resources[level - 1][resource])
            return (0);
        resource++;
    }
    resource = 0;
    while (resource < RESOURCE_COUNT)
    {
        square->resources[resource] -= ritual_resources[level - 1][resource];
        resource++;
    }
    player = server->players;
    while (player != NULL)
    {
        if (player->team != NULL && player->x == initiator->x &&
            player->y == initiator->y && player->level == level)
        {
            player->level++;
            snprintf(response, sizeof(response), "niveau actuel : %d\n", player->level);
            send_text(player->fd, "elevation en cours\n");
            send_text(player->fd, response);
        }
        player = player->next;
    }
    check_victory(server, initiator->team);
    return (1);
}
