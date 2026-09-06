#include "zappy.h"

int main(int argc, char **argv)
{
    if (parse_params(argc, argv, &g_server) != 0)
        return (1);
    if (init_server(&g_server) != 0)
        return (1);
    if (run_server(&g_server) != 0)
    {
        destroy_server(&g_server);
        return (1);
    }
    destroy_server(&g_server);
    return (0);
}