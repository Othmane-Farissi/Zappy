#ifndef IO_H
#define IO_H

#include "zappy.h"

void send_text(int fd, const char *text);
void broadcast_graphics(t_server *server, const char *text);

#endif
