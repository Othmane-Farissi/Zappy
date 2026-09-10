#ifndef TIMING_H
#define TIMING_H

#include "zappy.h"

long command_delay(const char *command, int timeunit);
int deadline_reached(const struct timeval *now, const struct timeval *deadline);
int update_hunger(t_server *server, t_player *player);
void set_deadline(t_player *player, long delay);
void set_egg_deadline(struct timeval *deadline, long delay);

#endif
