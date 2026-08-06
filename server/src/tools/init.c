#include "zappy.h"

t_queue		*ft_init_queue(void)
{
	t_queue *queue;

	if (!(queue = ft_memalloc(sizeof(t_queue))))
		return (NULL);
	queue->first = NULL;
	queue->last = NULL;
	return (queue);
}

int init_server(void)
{
    if (!(g_server.map = ft_memalloc(sizeof(t_map))))
		return (1);
	if (!(g_server.buff = ft_memalloc(4096 * sizeof(char))))
		return (1);
	g_server.events = ft_init_queue();
	g_server.timeunit = -1;
	return (0);
}