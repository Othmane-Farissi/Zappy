#include "zappy.h"


// +++++++++++++++++++++ SERVER 

t_queue		*ft_init_queue(void)
{
	t_queue *queue;

	if (!(queue = calloc(sizeof(t_queue))))
		return (NULL);
	queue->first = NULL;
	queue->last = NULL;
	return (queue);
}

int init_server(void)
{
    if (!(g_server.map = calloc(sizeof(t_map))))
		return (1);
	if (!(g_server.buff = calloc(4096 * sizeof(char))))
		return (1);
	g_server.events = ft_init_queue();
	g_server.timeunit = -1;
	return (0);
}

// +++++++++++++++++++++ MAP

int init_square(t_square **square)
{
	int i;

	i = 0;
	if (!(*square = calloc(sizeof(t_square))) || !((*square)->players = calloc(sizeof(t_player)*FD_SETSIZE)))
		return (1); // to add free function
	while (i < FD_SETSIZE)
	{
		(*square)->players[i++] = NULL;
	}
	return (0);
}

int init_map_ressources(void)
{
	int i;
	int k;
	int r;
	int n_team;
	int size;

	i = 0;
	k = 0;
	r = 0;
	n_team = sizeof(g_server.teams) / sizeof(t_team);
	size = g_server.map->width;
	while (k < 10)
	{
		i = 0;
		while (i < map_size)
		{
			ressource = 0;
			while (ressource < 7)
			{
				generate_ressource(ressource);
				ressource++;
			}
			i++;
		}
		k++;
	}
}	

int init_map(void)
{
	int i;
	int j;

	i = 0;
	if (!(g_server.map->squares = calloc(g_server.map->width * sizeof(t_square **))))
		return (1);
	
	while (i < g_server.map->width)
	{
		if (!(g_server.map->squares[i] = calloc(g_server.map->height * sizeof(t_square *))))
			return (1);
		j = 0;
		while (j < g_server.map->height)
		{
			if (init_square(&(g_server.map->squares[i][j])) == 1)
			{
				return (1); // to add free function
			}
		}
		i++;
		init_map_ressources();
	}
	
	return (0);
}