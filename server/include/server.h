#ifndef SERVER_H
#define SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/time.h>

#define RESOURCE_COUNT 7
#define MAX_LINE 4096
#define MAX_TEAMS 64

typedef enum e_resource
{
	FOOD,
	LINEMATE,
	DERAUMERE,
	SIBUR,
	MENDIANE,
	PHIRAS,
	THYSTAME
} t_resource;

typedef enum e_direction
{
	NORTH,
	EAST,
	SOUTH,
	WEST
} t_direction;

typedef struct s_team t_team;
typedef struct s_player t_player;
typedef struct s_egg t_egg;

typedef struct s_square
{
	int resources[RESOURCE_COUNT];
	t_player *players;
} t_square;

typedef struct s_map
{
	int width;
	int height;
	t_square *squares;
} t_map;

struct s_player
{
	int fd;
	int x;
	int y;
	int level;
	int inventory[RESOURCE_COUNT];
	t_direction direction;
	t_team *team;
	t_player *next;
	char input[MAX_LINE];
	size_t input_length;
	char commands[10][MAX_LINE];
	int command_count;
	bool action_active;
	struct timeval action_ready;
	struct timeval last_food;
};

struct s_egg
{
	t_team *team;
	struct timeval hatch_at;
	t_egg *next;
};

struct s_team
{
	char *name;
	int capacity;
	int connected;
};

typedef struct s_server
{
	int port;
	int timeunit;
	int max_team_players;
	int teamcount;
	int serverfd;
	t_map map;
	t_team teams[MAX_TEAMS];
	t_player *players;
	t_egg *eggs;
	bool winner_announced;
	fd_set read_fds;
	int max_fd;
} t_server;

extern t_server g_server;

int parse_params(int argc, char **argv, t_server *server);
int init_server(t_server *server);
void destroy_server(t_server *server);
int run_server(t_server *server);

#endif