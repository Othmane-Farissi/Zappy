#include "zappy.h"

t_server m_server;

int main(int argc, char **argv)
{
    if (init_server() == 1 || parse_args(argc, argv) == 1)
        return (1);

    return 0;
}