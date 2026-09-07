#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_LINE 4096

typedef struct s_client
{
    int fd;
    char input[MAX_LINE];
    size_t input_length;
} t_client;

static void print_usage(const char *program)
{
    fprintf(stderr, "Usage: %s -n <team> -p <port> [-h <hostname>]\n", program);
}

static int parse_args(int argc, char **argv, char **team, char **port, char **host)
{
    int i;
    int seen_team;
    int seen_port;
    int seen_host;

    *host = "localhost";
    seen_team = 0;
    seen_port = 0;
    seen_host = 0;
    i = 1;
    while (i < argc)
    {
        if ((strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-p") == 0 ||
             strcmp(argv[i], "-h") == 0) && i + 1 < argc && argv[i + 1][0] != '\0')
        {
            if (strcmp(argv[i], "-n") == 0)
            {
                if (seen_team)
                    return (1);
                *team = argv[i + 1];
                seen_team = 1;
            }
            else if (strcmp(argv[i], "-p") == 0)
            {
                if (seen_port)
                    return (1);
                *port = argv[i + 1];
                seen_port = 1;
            }
            else
            {
                if (seen_host)
                    return (1);
                *host = argv[i + 1];
                seen_host = 1;
            }
            i += 2;
        }
        else
            return (1);
    }
    return (!seen_team || !seen_port);
}

static int send_all(int fd, const char *text)
{
    size_t length;
    ssize_t sent;

    length = strlen(text);
    while (length > 0)
    {
        sent = send(fd, text, length, 0);
        if (sent <= 0)
            return (1);
        text += sent;
        length -= (size_t)sent;
    }
    return (0);
}

static int read_line(t_client *client, char *line, size_t line_size)
{
    char character;
    ssize_t received;

    while (client->input_length + 1 < sizeof(client->input))
    {
        received = recv(client->fd, &character, 1, 0);
        if (received <= 0)
            return (1);
        if (character == '\n')
        {
            client->input[client->input_length] = '\0';
            if (client->input_length > 0 && client->input[client->input_length - 1] == '\r')
                client->input[--client->input_length] = '\0';
            if (client->input_length + 1 > line_size)
                return (1);
            memcpy(line, client->input, client->input_length + 1);
            client->input_length = 0;
            return (0);
        }
        client->input[client->input_length++] = character;
    }
    return (1);
}

static int connect_server(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *current;
    int fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, port, &hints, &results) != 0)
        return (-1);
    fd = -1;
    current = results;
    while (current != NULL)
    {
        fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (fd >= 0 && connect(fd, current->ai_addr, current->ai_addrlen) == 0)
            break;
        if (fd >= 0)
            close(fd);
        fd = -1;
        current = current->ai_next;
    }
    freeaddrinfo(results);
    return (fd);
}

static int handshake(t_client *client, const char *team)
{
    char line[MAX_LINE];

    if (read_line(client, line, sizeof(line)) != 0 || strcmp(line, "BIENVENUE") != 0)
        return (1);
    if (send_all(client->fd, team) != 0 || send_all(client->fd, "\n") != 0)
        return (1);
    if (read_line(client, line, sizeof(line)) != 0 || strcmp(line, "ko") == 0)
        return (1);
    if (read_line(client, line, sizeof(line)) != 0)
        return (1);
    return (0);
}

static int run_client(t_client *client)
{
    static const char *commands[] = {
        "voir\n", "inventaire\n", "prend nourriture\n", "pose nourriture\n",
        "avance\n", "droite\n", "gauche\n", "broadcast ready\n",
        "expulse\n", "fork\n", "incantation\n", "connect_nbr\n"
    };
    char response[MAX_LINE];
    size_t command;

    command = 0;
    while (1)
    {
        if (send_all(client->fd, commands[command]) != 0 ||
            read_line(client, response, sizeof(response)) != 0)
            return (1);
        if (strcmp(response, "mort") == 0)
            return (0);
        command = (command + 1) % (sizeof(commands) / sizeof(commands[0]));
    }
}

int main(int argc, char **argv)
{
    t_client client;
    char *team;
    char *port;
    char *host;
    int status;

    if (parse_args(argc, argv, &team, &port, &host) != 0)
    {
        print_usage(argv[0]);
        return (1);
    }
    client.fd = connect_server(host, port);
    if (client.fd < 0)
        return (fprintf(stderr, "Could not connect to %s:%s\n", host, port), 1);
    client.input_length = 0;
    status = handshake(&client, team);
    if (status == 0)
        status = run_client(&client);
    close(client.fd);
    return (status);
}
