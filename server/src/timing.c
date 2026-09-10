#include "timing.h"
#include "io.h"
#include <string.h>
#include <sys/time.h>

long command_delay(const char *command, int timeunit)
{
    int units;

    units = 7;
    if (strncmp(command, "inventaire", 10) == 0)
        units = 1;
    if (strcmp(command, "connect_nbr") == 0)
        units = 0;
    if (strcmp(command, "incantation") == 0)
        units = 300;
    if (strcmp(command, "fork") == 0)
        units = 42;
    return ((long)units * 1000000L) / timeunit;
}

int deadline_reached(const struct timeval *now, const struct timeval *deadline)
{
    return (now->tv_sec > deadline->tv_sec ||
        (now->tv_sec == deadline->tv_sec && now->tv_usec >= deadline->tv_usec));
}

static long elapsed_microseconds(const struct timeval *now, const struct timeval *then)
{
    return ((now->tv_sec - then->tv_sec) * 1000000L + now->tv_usec - then->tv_usec);
}

int update_hunger(t_server *server, t_player *player)
{
    struct timeval now;
    long interval;
    long units;

    gettimeofday(&now, NULL);
    interval = 126000000L / server->timeunit;
    if (interval < 1)
        interval = 1;
    units = elapsed_microseconds(&now, &player->last_food) / interval;
    if (units <= 0)
        return (0);
    if (units >= player->inventory[FOOD])
        player->inventory[FOOD] = 0;
    else
        player->inventory[FOOD] -= (int)units;
    player->last_food.tv_sec += (units * interval) / 1000000L;
    player->last_food.tv_usec += (units * interval) % 1000000L;
    if (player->last_food.tv_usec >= 1000000L)
    {
        player->last_food.tv_sec++;
        player->last_food.tv_usec -= 1000000L;
    }
    if (player->inventory[FOOD] == 0)
    {
        send_text(player->fd, "mort\n");
        return (1);
    }
    return (0);
}

void set_deadline(t_player *player, long delay)
{
    gettimeofday(&player->action_ready, NULL);
    player->action_ready.tv_sec += delay / 1000000L;
    player->action_ready.tv_usec += delay % 1000000L;
    if (player->action_ready.tv_usec >= 1000000L)
    {
        player->action_ready.tv_sec++;
        player->action_ready.tv_usec -= 1000000L;
    }
    player->action_active = true;
}

void set_egg_deadline(struct timeval *deadline, long delay)
{
    gettimeofday(deadline, NULL);
    deadline->tv_sec += delay / 1000000L;
    deadline->tv_usec += delay % 1000000L;
    if (deadline->tv_usec >= 1000000L)
    {
        deadline->tv_sec++;
        deadline->tv_usec -= 1000000L;
    }
}
