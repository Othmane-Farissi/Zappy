#ifndef WORLD_H
#define WORLD_H

#include "zappy.h"

int resource_index(const char *name);
t_square *player_square(t_server *server, t_player *player);
void append_square_contents(t_server *server, t_player *viewer,
    int x, int y, char *response, size_t response_size);
void view_offset(t_direction direction, int depth, int side, int *x, int *y);
int sound_direction(t_server *server, t_player *receiver, t_player *sender);

#endif
