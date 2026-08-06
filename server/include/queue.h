#ifndef QUEUE_H
#define QUEUE_H

#include "zappy.h"

typedef struct	s_node
{
	void			*data;
	size_t			d_size;
	struct s_node	*next;

}				t_node;

typedef struct	s_queue
{
	t_node		*first;
	t_node		*last;

}				t_queue;

t_queue		*ft_init_queue(void);

#endif