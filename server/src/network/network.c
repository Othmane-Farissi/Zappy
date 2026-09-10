#include "zappy.h"
#include "gameplay.h"
#include "io.h"
#include "lifecycle.h"
#include "timing.h"
#include "world.h"
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
	char response[MAX_LINE];

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
		snprintf(response, sizeof(response), "ppo %d %d %d %d\n", player->id,
			player->x, player->y, player->direction);
		broadcast_graphics(server, response);
		send_text(player->fd, "ok\n");
	}
	else if (strcmp(command, "droite") == 0)
	{
		player->direction = (player->direction + 1) % 4;
		snprintf(response, sizeof(response), "ppo %d %d %d %d\n", player->id,
			player->x, player->y, player->direction);
		broadcast_graphics(server, response);
		send_text(player->fd, "ok\n");
	}
	else if (strcmp(command, "gauche") == 0)
	{
		player->direction = (player->direction + 3) % 4;
		snprintf(response, sizeof(response), "ppo %d %d %d %d\n", player->id,
			player->x, player->y, player->direction);
		broadcast_graphics(server, response);
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
			snprintf(response, sizeof(response), "bct %d %d %d %d %d %d %d %d %d\n",
				player->x, player->y, square->resources[FOOD], square->resources[LINEMATE],
				square->resources[DERAUMERE], square->resources[SIBUR],
				square->resources[MENDIANE], square->resources[PHIRAS],
				square->resources[THYSTAME]);
			broadcast_graphics(server, response);
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
			snprintf(response, sizeof(response), "bct %d %d %d %d %d %d %d %d %d\n",
				player->x, player->y, square->resources[FOOD], square->resources[LINEMATE],
				square->resources[DERAUMERE], square->resources[SIBUR],
				square->resources[MENDIANE], square->resources[PHIRAS],
				square->resources[THYSTAME]);
			broadcast_graphics(server, response);
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
		snprintf(response, sizeof(response), "pbc %d %s\n", player->id, command + 10);
		broadcast_graphics(server, response);
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
	player->id = ++server->next_player_id;
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
	int x;
	int y;
	t_square *square;
	t_player *existing;

	if (strcmp(team_name, "GRAPHIC") == 0)
	{
		player->graphic = true;
		snprintf(response, sizeof(response), "msz %d %d\n", server->map.width, server->map.height);
		send_text(player->fd, response);
		y = 0;
		while (y < server->map.height)
		{
			x = 0;
			while (x < server->map.width)
			{
				square = &server->map.squares[y * server->map.width + x];
				snprintf(response, sizeof(response), "bct %d %d %d %d %d %d %d %d %d\n",
					x, y, square->resources[FOOD], square->resources[LINEMATE],
					square->resources[DERAUMERE], square->resources[SIBUR],
					square->resources[MENDIANE], square->resources[PHIRAS],
					square->resources[THYSTAME]);
				send_text(player->fd, response);
				x++;
			}
			y++;
		}
		x = 0;
		while (x < server->teamcount)
		{
			snprintf(response, sizeof(response), "tna %s\n", server->teams[x].name);
			send_text(player->fd, response);
			x++;
		}
		existing = server->players;
		while (existing != NULL)
		{
			if (existing->team != NULL && !existing->graphic)
			{
				snprintf(response, sizeof(response), "pnw %d %d %d %d %s\n",
					existing->id, existing->x, existing->y, existing->direction,
					existing->team->name);
				send_text(player->fd, response);
			}
			existing = existing->next;
		}
		return (0);
	}

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
	snprintf(response, sizeof(response), "pnw %d %d %d %d %s\n", player->id,
		player->x, player->y, player->direction, team->name);
	broadcast_graphics(server, response);
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
			else if (player->team == NULL && !player->graphic)
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
			if (player->team != NULL && !player->graphic)
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
