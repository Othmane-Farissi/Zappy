#ifndef MAP_H
#define MAP_H

#include "zappy.h"

typedef struct s_player t_player;
    
typedef struct s_square
{
    resource_t resources[7];
    t_player *players;
} t_square;

typedef struct s_map
{
    int			width;
	int			height;
	t_square	***squares;
	uint8_t		max_ressources[7];
	uint8_t		current_ressources[7];

} t_map;

#endif