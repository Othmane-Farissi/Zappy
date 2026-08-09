#include "zappy.h"

t_server m_server;

int main(int argc, char **argv)
{
    if (init_server() == 1 || parse_param(argc, argv) == 1 || init_map() == 1)
        return (1);

    return 0;
}