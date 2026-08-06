#ifndef SERVER_H
#define SERVER_H

typedef struct	s_egg
{
	t_team		*team;
	int			x;
	int			y;
	int			father_fd;
	int			hatched;
	int			number;
}				t_egg;

typedef struct	s_team
{
	char		*name;
	int			max_players;
	int			connected;
	int			allowed_eggs;
	t_queue		*egg_queue;

}				t_team;

typedef struct s_server
{
    t_map			*map;				/* game map */
	t_team			**teams;			/* set of all teams */
	t_queue			*events;			/* priority queue of events */
	t_list			*graphics;			/* a list of graphical clients */
	fd_set			client_fds;			/* set of all clients fds */
	char			*buff;				/* buff used to send messages to clients in order to reduce malloc usage */
	struct timeval	time;				/* "actual" game time */
	int				timeunit;			/* time unit; "-t" option */
	int				port;				/* port; "-p" option */
	int				print_queue;		/* flag showing the event's queue; "-q" option */
	int				max_team_players;	/* a max num of players at the beginning of the game; "-c" option */
	int				teamcount;			/* number of teams */
	int				serverfd;			/* server socket fd */
	int				number_egg;	
    
} t_server;

#endif