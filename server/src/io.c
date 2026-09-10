#include "io.h"
#include <string.h>
#include <sys/socket.h>

void send_text(int fd, const char *text)
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

void broadcast_graphics(t_server *server, const char *text)
{
    t_player *player;

    player = server->players;
    while (player != NULL)
    {
        if (player->graphic)
            send_text(player->fd, text);
        player = player->next;
    }
}
