#ifndef ZAPPY_H
#define ZAPPY_H

#include "server.h"
#include "map.h"
#include "client.h"
#include "queue.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

char	*g_ressources[8];

typedef struct	s_opt
{
	char		opt;
	int			(*fct)(char **av, int *i);
}				t_opt;

#endif