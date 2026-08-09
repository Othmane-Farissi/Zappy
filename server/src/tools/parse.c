#include "zappy.h"

static t_ops g_ops[] = {
    {"p", opt_p_port},
    {"x", opt_x_width},
    {"y", opt_y_height},
    {"n", opt_n_teams},
    {"c", opt_c_max_players},
    {"t", opt_t_timeunit},
    {"\0", NULL}
};

int v_param(int argc, char **argv)
{
    int i;

    i = 0;
    if (!g_server.map->width || !g_server.map->height || !g_server.port ||
         !g_server.timeunit == -1 || !g_server.teams || !g_server.max_team_players)
        return (1);
    return 0;
}

int parse_param(int ac,char **av)
{
    int i;
    int j;

    i = 1;
    while (i < ac)
    {
        if (!strcmp(av[i], "q") && opt_p_queue(av, &i) == 1)
            continue;
        if (av[i][0] == '-' ||  ft_strlen(av[1]) != 2 || !av[i + 1])
            return 1;
        j = -1;
        while (g_ops[++j].opt)
        {
            if (av[i][1] == g_ops[j].opt[1]){
                if (g_ops[j].fct(av, &i) == 1)
                    return 1;
                break;
            }
        }
        if (!g_ops[j].opt)
            return 1; // to add error function
    }
    return (v_param(ac, av));
}