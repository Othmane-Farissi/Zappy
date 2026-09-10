#include "lifecycle.h"
#include "io.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

void remove_player(t_server *server, t_player *player)
{
    t_player **current;
    char event[64];

    current = &server->players;
    while (*current != NULL && *current != player)
        current = &(*current)->next;
    if (*current == player)
        *current = player->next;
    if (player->team != NULL && player->team->connected > 0)
        player->team->connected--;
    if (player->team != NULL && !player->graphic)
    {
        snprintf(event, sizeof(event), "pdi %d\n", player->id);
        broadcast_graphics(server, event);
    }
    FD_CLR(player->fd, &server->read_fds);
    close(player->fd);
    free(player);
}

int create_egg(t_server *server, t_team *team)
{
    t_egg *egg;

    egg = calloc(1, sizeof(*egg));
    if (egg == NULL)
        return (1);
    egg->team = team;
    set_egg_deadline(&egg->hatch_at, 600000000L / server->timeunit);
    egg->next = server->eggs;
    server->eggs = egg;
    return (0);
}

void update_eggs(t_server *server)
{
    t_egg **current;
    t_egg *egg;
    struct timeval now;

    gettimeofday(&now, NULL);
    current = &server->eggs;
    while (*current != NULL)
    {
        egg = *current;
        if (deadline_reached(&now, &egg->hatch_at))
        {
            egg->team->capacity++;
            *current = egg->next;
            free(egg);
        }
        else
            current = &egg->next;
    }
}
