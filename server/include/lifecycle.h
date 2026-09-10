#ifndef LIFECYCLE_H
#define LIFECYCLE_H

#include "zappy.h"

void remove_player(t_server *server, t_player *player);
int create_egg(t_server *server, t_team *team);
void update_eggs(t_server *server);

#endif
