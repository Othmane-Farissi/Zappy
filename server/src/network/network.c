#include "zappy.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

t_server g_server;

static const char *g_resource_names[RESOURCE_COUNT] = {
	"nourriture", "linemate", "deraumere", "sibur",
	"mendiane", "phiras", "thystame"
};

static const int g_ritual_players[7] = {1, 2, 2, 4, 4, 6, 6};
static const int g_ritual_resources[7][RESOURCE_COUNT] = {
	{0, 1, 0, 0, 0, 0, 0},
	{0, 1, 1, 1, 0, 0, 0},
	{0, 2, 0, 1, 0, 2, 0},
	{0, 1, 1, 2, 0, 1, 0},
	{0, 1, 2, 1, 3, 0, 0},
	{0, 1, 2, 3, 0, 1, 1},
	{0, 2, 2, 2, 2, 2, 1}
};

static t_team *find_team(t_server *server, const char *name)
{
	int i;

	i = 0;
	while (i < server->teamcount)
	{
		if (strcmp(server->teams[i].name, name) == 0)
			return (&server->teams[i]);
		i++;
	}
	return (NULL);
}

static int resource_index(const char *name)
{
	int resource;

	resource = 0;
	while (resource < RESOURCE_COUNT)
	{
		if (strcmp(name, g_resource_names[resource]) == 0)
			return (resource);
		resource++;
	}
	return (-1);
}

static t_square *player_square(t_server *server, t_player *player)
{
	return (&server->map.squares[player->y * server->map.width + player->x]);
}

static long command_delay(const char *command, int timeunit)
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

static int deadline_reached(const struct timeval *now, const struct timeval *deadline)
{
	return (now->tv_sec > deadline->tv_sec ||
		(now->tv_sec == deadline->tv_sec && now->tv_usec >= deadline->tv_usec));
}

static void send_text(int fd, const char *text);
static void check_victory(t_server *server, t_team *team);

static long elapsed_microseconds(const struct timeval *now, const struct timeval *then)
{
	return ((now->tv_sec - then->tv_sec) * 1000000L + now->tv_usec - then->tv_usec);
}

static int update_hunger(t_server *server, t_player *player)
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

static void set_deadline(t_player *player, long delay)
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

static void set_egg_deadline(struct timeval *deadline, long delay)
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

static void send_text(int fd, const char *text)
{
	size_t length;
	ssize_t sent;

	length = strlen(text);
	while (length > 0)
	{
		sent = send(fd, text, length, MSG_NOSIGNAL);
		if (sent <= 0)
			return;
		text += sent;
		length -= (size_t)sent;
	}
}

static void remove_player(t_server *server, t_player *player)
{
	t_player **current;

	current = &server->players;
	while (*current != NULL && *current != player)
		current = &(*current)->next;
	if (*current == player)
		*current = player->next;
	if (player->team != NULL && player->team->connected > 0)
		player->team->connected--;
	FD_CLR(player->fd, &server->read_fds);
	close(player->fd);
	free(player);
}

static int create_egg(t_server *server, t_team *team)
{
	t_egg *egg;

	egg = calloc(1, sizeof(*egg));
	if (egg == NULL)
		return (1);
	egg->team = team;
	set_egg_deadline(&egg->hatch_at, 600000000L / server->timeunit);
	egg->next = server->eggs;
	server->eggs = egg;
	return (0);
}

static void update_eggs(t_server *server)
{
	t_egg **current;
	t_egg *egg;
	struct timeval now;

	gettimeofday(&now, NULL);
	current = &server->eggs;
	while (*current != NULL)
	{
		egg = *current;
		if (deadline_reached(&now, &egg->hatch_at))
		{
			egg->team->capacity++;
			*current = egg->next;
			free(egg);
		}
		else
			current = &egg->next;
	}
}

static void append_square_contents(t_server *server, t_player *viewer,
	int x, int y, char *response, size_t response_size)
{
	t_square *square;
	t_player *player;
	int resource;
	int count;

	x = (x + server->map.width) % server->map.width;
	y = (y + server->map.height) % server->map.height;
	square = &server->map.squares[y * server->map.width + x];
	resource = 0;
	while (resource < RESOURCE_COUNT)
	{
		count = 0;
		while (count < square->resources[resource])
		{
			strncat(response, g_resource_names[resource], response_size - strlen(response) - 1);
			strncat(response, " ", response_size - strlen(response) - 1);
			count++;
		}
		resource++;
	}
	player = server->players;
	while (player != NULL)
	{
		if (player != viewer && player->team != NULL && player->x == x && player->y == y)
			strncat(response, "player ", response_size - strlen(response) - 1);
		player = player->next;
	}
}

static void view_offset(t_direction direction, int depth, int side, int *x, int *y)
{
	if (direction == NORTH)
	{
		*x = side;
		*y = -depth;
	}
	else if (direction == EAST)
	{
		*x = depth;
		*y = side;
	}
	else if (direction == SOUTH)
	{
		*x = -side;
		*y = depth;
	}
	else
	{
		*x = -depth;
		*y = -side;
	}
}

static int sound_direction(t_server *server, t_player *receiver, t_player *sender)
{
	int dx;
	int dy;
	int global;
	int facing;
	int delta;

	dx = sender->x - receiver->x;
	dy = sender->y - receiver->y;
	if (dx > server->map.width / 2)
		dx -= server->map.width;
	else if (dx < -(server->map.width / 2))
		dx += server->map.width;
	if (dy > server->map.height / 2)
		dy -= server->map.height;
	else if (dy < -(server->map.height / 2))
		dy += server->map.height;
	if (dx == 0 && dy == 0)
		return (0);
	if (dy < 0)
		global = dx > 0 ? 1 : (dx < 0 ? 7 : 0);
	else if (dy > 0)
		global = dx > 0 ? 3 : (dx < 0 ? 5 : 4);
	else
		global = dx > 0 ? 2 : 6;
	facing = receiver->direction * 2;
	delta = (global - facing + 8) % 8;
	return ((8 - delta) % 8 + 1);
}

static int complete_incantation(t_server *server, t_player *initiator)
{
	t_player *player;
	t_square *square;
	int level;
	int players;
	int resource;
	char response[64];

	level = initiator->level;
	if (level < 1 || level > 7)
		return (0);
	square = player_square(server, initiator);
	players = 0;
	player = server->players;
	while (player != NULL)
	{
		if (player->team != NULL && player->x == initiator->x &&
			player->y == initiator->y && player->level == level)
			players++;
		player = player->next;
	}
	if (players < g_ritual_players[level - 1])
		return (0);
	resource = 0;
	while (resource < RESOURCE_COUNT)
	{
		if (square->resources[resource] < g_ritual_resources[level - 1][resource])
			return (0);
		resource++;
	}
	resource = 0;
	while (resource < RESOURCE_COUNT)
	{
		square->resources[resource] -= g_ritual_resources[level - 1][resource];
		resource++;
	}
	player = server->players;
	while (player != NULL)
	{
		if (player->team != NULL && player->x == initiator->x &&
			player->y == initiator->y && player->level == level)
		{
			player->level++;
			snprintf(response, sizeof(response), "niveau actuel : %d\n", player->level);
			send_text(player->fd, "elevation en cours\n");
			send_text(player->fd, response);
		}
		player = player->next;
	}
	check_victory(server, initiator->team);
	return (1);
}

static void check_victory(t_server *server, t_team *team)
{
	t_player *player;
	int elevated;
	char response[128];

	if (server->winner_announced)
		return;
	elevated = 0;
	player = server->players;
	while (player != NULL)
	{
		if (player->team == team && player->level >= 8)
			elevated++;
		player = player->next;
	}
	if (elevated < 6)
		return;
	server->winner_announced = true;
	snprintf(response, sizeof(response), "equipe gagnante : %s\n", team->name);
	player = server->players;
	while (player != NULL)
	{
		send_text(player->fd, response);
		player = player->next;
	}
}

static int read_line(t_player *player)
{
	char buffer[512];
	ssize_t received;
	size_t available;

	received = recv(player->fd, buffer, sizeof(buffer), 0);
	if (received <= 0)
		return (received == 0 ? 0 : -1);
	available = sizeof(player->input) - player->input_length - 1;
	if ((size_t)received > available)
		return (-1);
	memcpy(player->input + player->input_length, buffer, (size_t)received);
	player->input_length += (size_t)received;
	player->input[player->input_length] = '\0';
	return (1);
}

static void handle_command(t_server *server, t_player *player, char *command)
{
	t_square *square;
	t_player *recipient;
	int resource;
	int x;
	int y;
	char response[256];

	if (strcmp(command, "avance") == 0)
	{
		x = player->x;
		y = player->y;
		if (player->direction == NORTH) y = (y + server->map.height - 1) % server->map.height;
		if (player->direction == EAST) x = (x + 1) % server->map.width;
		if (player->direction == SOUTH) y = (y + 1) % server->map.height;
		if (player->direction == WEST) x = (x + server->map.width - 1) % server->map.width;
		player->x = x;
		player->y = y;
		send_text(player->fd, "ok\n");
	}
	else if (strcmp(command, "droite") == 0)
	{
		player->direction = (player->direction + 1) % 4;
		send_text(player->fd, "ok\n");
	}
	else if (strcmp(command, "gauche") == 0)
	{
		player->direction = (player->direction + 3) % 4;
		send_text(player->fd, "ok\n");
	}
	else if (strcmp(command, "inventaire") == 0)
	{
		snprintf(response, sizeof(response), "{nourriture %d, linemate %d, deraumere %d, sibur %d, mendiane %d, phiras %d, thystame %d}\n", player->inventory[0], player->inventory[1], player->inventory[2], player->inventory[3], player->inventory[4], player->inventory[5], player->inventory[6]);
		send_text(player->fd, response);
	}
	else if (strcmp(command, "connect_nbr") == 0)
	{
		snprintf(response, sizeof(response), "%d\n", player->team->capacity - player->team->connected);
		send_text(player->fd, response);
	}
	else if (strcmp(command, "voir") == 0)
	{
		int depth;
		int side;
		int offset_x;
		int offset_y;

		response[0] = '{';
		response[1] = '\0';
		depth = 0;
		while (depth <= player->level)
		{
			side = -depth;
			while (side <= depth)
			{
				if (depth != 0 || side != -depth)
					strncat(response, ", ", sizeof(response) - strlen(response) - 1);
				view_offset(player->direction, depth, side, &offset_x, &offset_y);
				response[sizeof(response) - 1] = '\0';
				append_square_contents(server, player, player->x + offset_x,
					player->y + offset_y, response, sizeof(response));
				side++;
			}
			depth++;
		}
		strncat(response, "}\n", sizeof(response) - strlen(response) - 1);
		send_text(player->fd, response);
	}
	else if (strncmp(command, "prend ", 6) == 0)
	{
		resource = resource_index(command + 6);
		square = player_square(server, player);
		if (resource >= 0 && square->resources[resource] > 0)
		{
			square->resources[resource]--;
			player->inventory[resource]++;
			send_text(player->fd, "ok\n");
		}
		else
			send_text(player->fd, "ko\n");
	}
	else if (strncmp(command, "pose ", 5) == 0)
	{
		resource = resource_index(command + 5);
		square = player_square(server, player);
		if (resource >= 0 && player->inventory[resource] > 0)
		{
			player->inventory[resource]--;
			square->resources[resource]++;
			send_text(player->fd, "ok\n");
		}
		else
			send_text(player->fd, "ko\n");
	}
	else if (strncmp(command, "broadcast ", 10) == 0)
	{
		recipient = server->players;
		while (recipient != NULL)
		{
			if (recipient != player && recipient->team != NULL)
			{
				snprintf(response, sizeof(response), "message %d,%s\n",
					sound_direction(server, recipient, player), command + 10);
				send_text(recipient->fd, response);
			}
			recipient = recipient->next;
		}
		send_text(player->fd, "ok\n");
	}
	else if (strcmp(command, "expulse") == 0)
	{
		recipient = server->players;
		while (recipient != NULL)
		{
			if (recipient != player && recipient->team != NULL &&
				recipient->x == player->x && recipient->y == player->y)
			{
				x = recipient->x;
				y = recipient->y;
				if (player->direction == NORTH) y = (y + server->map.height - 1) % server->map.height;
				if (player->direction == EAST) x = (x + 1) % server->map.width;
				if (player->direction == SOUTH) y = (y + 1) % server->map.height;
				if (player->direction == WEST) x = (x + server->map.width - 1) % server->map.width;
				recipient->x = x;
				recipient->y = y;
				send_text(recipient->fd, "deplacement 0\n");
			}
			recipient = recipient->next;
		}
		send_text(player->fd, "ok\n");
	}
	else if (strcmp(command, "fork") == 0)
	{
		if (create_egg(server, player->team) == 0)
			send_text(player->fd, "ok\n");
		else
			send_text(player->fd, "ko\n");
	}
	else if (strcmp(command, "incantation") == 0)
	{
		if (complete_incantation(server, player) == 0)
			send_text(player->fd, "ko\n");
	}
	else
		send_text(player->fd, "ko\n");
}

static void process_input(t_server *server, t_player *player)
{
	char *newline;
	size_t remaining;
	struct timeval now;

	while ((newline = strchr(player->input, '\n')) != NULL)
	{
		*newline = '\0';
		if (newline > player->input && newline[-1] == '\r')
			newline[-1] = '\0';
		if (player->command_count < 10)
		{
			strncpy(player->commands[player->command_count], player->input, MAX_LINE - 1);
			player->commands[player->command_count][MAX_LINE - 1] = '\0';
			player->command_count++;
		}
		remaining = player->input_length - (size_t)(newline - player->input + 1);
		memmove(player->input, newline + 1, remaining);
		player->input_length = remaining;
		player->input[remaining] = '\0';
	}
	gettimeofday(&now, NULL);
	if (!player->action_active && player->command_count > 0)
		set_deadline(player, command_delay(player->commands[0], server->timeunit));
	if (player->action_active && deadline_reached(&now, &player->action_ready))
	{
		handle_command(server, player, player->commands[0]);
		memmove(player->commands, player->commands[1], (size_t)(player->command_count - 1) * MAX_LINE);
		player->command_count--;
		player->action_active = false;
	}
}

static int accept_player(t_server *server)
{
	int fd;
	struct sockaddr_in address;
	socklen_t length;
	t_player *player;

	length = sizeof(address);
	fd = accept(server->serverfd, (struct sockaddr *)&address, &length);
	if (fd < 0)
		return (0);
	player = calloc(1, sizeof(*player));
	if (player == NULL)
		return (close(fd), 1);
	player->fd = fd;
	player->team = NULL;
	player->next = server->players;
	server->players = player;
	FD_SET(fd, &server->read_fds);
	if (fd > server->max_fd)
		server->max_fd = fd;
	send_text(fd, "BIENVENUE\n");
	return (0);
}

static int complete_handshake(t_server *server, t_player *player, char *team_name)
{
	t_team *team;
	char response[128];

	team = find_team(server, team_name);
	if (team == NULL || team->connected >= team->capacity)
		return (send_text(player->fd, "ko\n"), 1);
	player->team = team;
	team->connected++;
	player->x = 0;
	player->y = 0;
	player->level = 1;
	player->direction = NORTH;
	player->inventory[FOOD] = 10;
	gettimeofday(&player->last_food, NULL);
	snprintf(response, sizeof(response), "%d\n%d %d\n", team->capacity - team->connected, server->map.width, server->map.height);
	send_text(player->fd, response);
	return (0);
}

int run_server(t_server *server)
{
	struct sockaddr_in address;
	fd_set ready_fds;
	struct timeval timeout;
	t_player *player;
	t_player *next;
	int option;
	int ready;

	server->serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if (server->serverfd < 0)
		return (1);
	option = 1;
	setsockopt(server->serverfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons((uint16_t)server->port);
	if (bind(server->serverfd, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(server->serverfd, 128) < 0)
		return (close(server->serverfd), 1);
	FD_ZERO(&server->read_fds);
	FD_SET(server->serverfd, &server->read_fds);
	server->max_fd = server->serverfd;
	while (1)
	{
		ready_fds = server->read_fds;
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000;
		ready = select(server->max_fd + 1, &ready_fds, NULL, NULL, &timeout);
		if (ready < 0)
		{
			if (errno == EINTR) continue;
			break;
		}
		if (FD_ISSET(server->serverfd, &ready_fds))
			accept_player(server);
		update_eggs(server);
		player = server->players;
		while (player != NULL)
		{
			next = player->next;
			if (FD_ISSET(player->fd, &ready_fds) && read_line(player) <= 0)
				remove_player(server, player);
			else if (player->team == NULL)
			{
				char *newline = strchr(player->input, '\n');
				if (newline != NULL)
				{
					*newline = '\0';
					complete_handshake(server, player, player->input);
					player->input_length = 0;
					player->input[0] = '\0';
				}
			}
			if (player->team != NULL)
			{
				if (update_hunger(server, player) != 0)
					remove_player(server, player);
				else
					process_input(server, player);
			}
			player = next;
		}
	}
	close(server->serverfd);
	return (0);
}
