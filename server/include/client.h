#ifndef CLIENT_H
#define CLIENT_H

#include "zappy.h"

typedef struct	s_team	t_team;

typedef struct	s_level
{
	int		level;
	int		players_nb;
	int		resources[7];
}				t_level;


typedef struct s_player
{
    int				fd;
	t_team			*team;
	int				level;
	int				direction;
	int				x;
	int				y;
	int				inventory[7];
	struct timeval	*last_request;
	int				requests_nb;
	bool			incantation;
} t_player;

#endif