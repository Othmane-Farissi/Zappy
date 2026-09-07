#define _POSIX_C_SOURCE 200112L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAP_WIDTH 20
#define MAP_HEIGHT 12
#define TILE_SIZE 52
#define PANEL_HEIGHT 32
#define WINDOW_WIDTH (MAP_WIDTH * TILE_SIZE)
#define WINDOW_HEIGHT (MAP_HEIGHT * TILE_SIZE + PANEL_HEIGHT)
#define RESOURCE_COUNT 7
#define MAX_PLAYERS 256

typedef struct s_gfx
{
    int fd;
    int map_width;
    int map_height;
    Display *display;
    int screen;
    Window window;
    GC context;
    Atom delete_message;
    unsigned int resources[MAP_HEIGHT][MAP_WIDTH][RESOURCE_COUNT];
    unsigned int players[MAP_HEIGHT][MAP_WIDTH];
    int player_x[MAX_PLAYERS];
    int player_y[MAX_PLAYERS];
    int player_direction[MAX_PLAYERS];
    int player_team[MAX_PLAYERS];
    int player_active[MAX_PLAYERS];
    int selected_x;
    int selected_y;
} t_gfx;

static const unsigned long resource_colors[RESOURCE_COUNT] = {
    0xe0b84f, 0x74b9d8, 0xb07c4f, 0x9b9b9b, 0xd58bc3, 0x8a72c9, 0x65b97a
};

static const unsigned long team_colors[4] = {0xe9f1dc, 0xf08a72, 0x72c7d6, 0xd6c36a};

static unsigned long color(Display *display, unsigned long rgb)
{
    XColor value;
    Colormap palette;

    palette = DefaultColormap(display, DefaultScreen(display));
    value.red = (unsigned short)(((rgb >> 16) & 0xff) * 257);
    value.green = (unsigned short)(((rgb >> 8) & 0xff) * 257);
    value.blue = (unsigned short)((rgb & 0xff) * 257);
    value.flags = DoRed | DoGreen | DoBlue;
    if (XAllocColor(display, palette, &value) == 0)
        return BlackPixel(display, DefaultScreen(display));
    return value.pixel;
}

static int read_line(t_gfx *gfx, char *line, size_t line_size)
{
    size_t length;
    char character;

    length = 0;
    while (length + 1 < line_size)
    {
        if (recv(gfx->fd, &character, 1, 0) <= 0)
            return (1);
        if (character == '\n')
        {
            line[length] = '\0';
            return (0);
        }
        line[length++] = character;
    }
    return (1);
}

static int send_text(int fd, const char *text)
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

static int load_server_map(t_gfx *gfx)
{
    char line[256];
    int x;
    int y;
    int values[RESOURCE_COUNT];
    int resource;
    int count;

    if (read_line(gfx, line, sizeof(line)) != 0 ||
        sscanf(line, "msz %d %d", &gfx->map_width, &gfx->map_height) != 2 ||
        gfx->map_width < 1 || gfx->map_width > MAP_WIDTH ||
        gfx->map_height < 1 || gfx->map_height > MAP_HEIGHT)
        return (1);
    memset(gfx->resources, 0, sizeof(gfx->resources));
    memset(gfx->players, 0, sizeof(gfx->players));
    y = 0;
    while (y < gfx->map_height)
    {
        x = 0;
        while (x < gfx->map_width)
        {
            if (read_line(gfx, line, sizeof(line)) != 0)
                return (1);
            count = sscanf(line, "bct %d %d %d %d %d %d %d %d %d",
                &x, &y, &values[0], &values[1], &values[2], &values[3],
                &values[4], &values[5], &values[6]);
            if (count != 9 || x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
                return (1);
            resource = 0;
            while (resource < RESOURCE_COUNT)
            {
                gfx->resources[y][x][resource] = (unsigned int)values[resource];
                resource++;
            }
            x++;
        }
        y++;
    }
    return (0);
}

static void set_title(t_gfx *gfx, const char *text);

static void remove_player(t_gfx *gfx, int id)
{
    if (id < 0 || id >= MAX_PLAYERS || !gfx->player_active[id])
        return;
    if (gfx->player_x[id] >= 0 && gfx->player_y[id] >= 0 &&
        gfx->players[gfx->player_y[id]][gfx->player_x[id]] > 0)
        gfx->players[gfx->player_y[id]][gfx->player_x[id]]--;
    gfx->player_active[id] = 0;
}

static void update_player(t_gfx *gfx, int id, int x, int y, int direction, int team)
{
    if (id < 0 || id >= MAX_PLAYERS || x < 0 || x >= gfx->map_width ||
        y < 0 || y >= gfx->map_height)
        return;
    if (gfx->player_active[id])
    {
        if (gfx->players[gfx->player_y[id]][gfx->player_x[id]] > 0)
            gfx->players[gfx->player_y[id]][gfx->player_x[id]]--;
    }
    gfx->player_x[id] = x;
    gfx->player_y[id] = y;
    gfx->player_direction[id] = direction;
    if (team >= 0)
        gfx->player_team[id] = team;
    gfx->player_active[id] = 1;
    gfx->players[y][x]++;
}

static void process_server_line(t_gfx *gfx, char *line)
{
    int id;
    int x;
    int y;
    int direction;
    char team[64];
    int values[RESOURCE_COUNT];
    char message[128];

    if (sscanf(line, "pnw %d %d %d %d %63s", &id, &x, &y, &direction, team) == 5)
        update_player(gfx, id, x, y, direction, (unsigned char)team[0] % 4);
    else if (sscanf(line, "ppo %d %d %d %d", &id, &x, &y, &direction) == 4)
        update_player(gfx, id, x, y, direction, -1);
    else if (sscanf(line, "pdi %d", &id) == 1)
        remove_player(gfx, id);
    else if (sscanf(line, "bct %d %d %d %d %d %d %d %d %d", &x, &y,
        &values[0], &values[1], &values[2], &values[3], &values[4],
        &values[5], &values[6]) == 9 && x >= 0 && x < gfx->map_width &&
        y >= 0 && y < gfx->map_height)
    {
        memcpy(gfx->resources[y][x], values, sizeof(values));
    }
    else if (sscanf(line, "pbc %d %127[^\n]", &id, message) == 2)
    {
        snprintf(message, sizeof(message), "Zappy | player %d broadcast", id);
        set_title(gfx, message);
    }
}

static void set_title(t_gfx *gfx, const char *text)
{
    XStoreName(gfx->display, gfx->window, text);
}

static void draw_tile(t_gfx *gfx, int x, int y)
{
    int resource;
    int offset;
    int count;
    unsigned long tile_color;

    tile_color = color(gfx->display, (x + y) % 2 == 0 ? 0x253b35 : 0x2d493d);
    XSetForeground(gfx->display, gfx->context, tile_color);
    XFillRectangle(gfx->display, gfx->window, gfx->context,
        x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE);
    XSetForeground(gfx->display, gfx->context, color(gfx->display, 0x42634e));
    XDrawRectangle(gfx->display, gfx->window, gfx->context,
        x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE - 1, TILE_SIZE - 1);
    offset = 5;
    resource = 0;
    while (resource < RESOURCE_COUNT)
    {
        count = 0;
        while (count < (int)gfx->resources[y][x][resource])
        {
            XSetForeground(gfx->display, gfx->context, color(gfx->display, resource_colors[resource]));
            XFillArc(gfx->display, gfx->window, gfx->context,
                x * TILE_SIZE + offset, y * TILE_SIZE + 5,
                7, 7, 0, 360 * 64);
            offset += 9;
            count++;
        }
        resource++;
    }
    {
        int id;
        int shown;
        int center_x;
        int center_y;
        int end_x;
        int end_y;

        shown = 0;
        id = 0;
        while (id < MAX_PLAYERS)
        {
            if (gfx->player_active[id] && gfx->player_x[id] == x && gfx->player_y[id] == y)
            {
                center_x = x * TILE_SIZE + 12 + (shown % 3) * 14;
                center_y = y * TILE_SIZE + 17 + (shown / 3) * 14;
                XSetForeground(gfx->display, gfx->context,
                    color(gfx->display, team_colors[gfx->player_team[id] % 4]));
                XFillArc(gfx->display, gfx->window, gfx->context,
                    center_x, center_y, 16, 16, 0, 360 * 64);
                end_x = center_x + 8;
                end_y = center_y + 8;
                if (gfx->player_direction[id] == 0)
                    end_y -= 7;
                else if (gfx->player_direction[id] == 1)
                    end_x += 7;
                else if (gfx->player_direction[id] == 2)
                    end_y += 7;
                else
                    end_x -= 7;
                XSetForeground(gfx->display, gfx->context, color(gfx->display, 0x163128));
                XDrawLine(gfx->display, gfx->window, gfx->context,
                    center_x + 8, center_y + 8, end_x, end_y);
                shown++;
            }
            id++;
        }
    }
    if (x == gfx->selected_x && y == gfx->selected_y)
    {
        XSetForeground(gfx->display, gfx->context, color(gfx->display, 0xf0d36a));
        XDrawRectangle(gfx->display, gfx->window, gfx->context,
            x * TILE_SIZE + 2, y * TILE_SIZE + 2, TILE_SIZE - 5, TILE_SIZE - 5);
    }
}

static void draw_map(t_gfx *gfx)
{
    int x;
    int y;
    char status[128];

    y = 0;
    while (y < gfx->map_height)
    {
        x = 0;
        while (x < gfx->map_width)
        {
            draw_tile(gfx, x, y);
            x++;
        }
        y++;
    }
    XSetForeground(gfx->display, gfx->context, color(gfx->display, 0x17221e));
    XFillRectangle(gfx->display, gfx->window, gfx->context,
        0, gfx->map_height * TILE_SIZE, WINDOW_WIDTH, PANEL_HEIGHT);
    snprintf(status, sizeof(status), "Zappy map  |  click a square to inspect it");
    XSetForeground(gfx->display, gfx->context, color(gfx->display, 0xe9f1dc));
    XDrawString(gfx->display, gfx->window, gfx->context,
        10, gfx->map_height * TILE_SIZE + 21, status, (int)strlen(status));
    XFlush(gfx->display);
}

static void select_tile(t_gfx *gfx, int pixel_x, int pixel_y)
{
    char title[256];
    int resource;
    int x;
    int y;
    int total;

    if (pixel_y >= gfx->map_height * TILE_SIZE)
        return;
    x = pixel_x / TILE_SIZE;
    y = pixel_y / TILE_SIZE;
    if (x < 0 || x >= gfx->map_width || y < 0 || y >= gfx->map_height)
        return;
    gfx->selected_x = x;
    gfx->selected_y = y;
    total = 0;
    resource = 0;
    while (resource < RESOURCE_COUNT)
    {
        total += (int)gfx->resources[y][x][resource];
        resource++;
    }
    snprintf(title, sizeof(title), "Zappy | square (%d, %d) | resources: %d | players: %u",
        x, y, total, gfx->players[y][x]);
    set_title(gfx, title);
    draw_map(gfx);
}

static int parse_args(int argc, char **argv, const char **host, const char **port)
{
    int i;

    *host = "localhost";
    *port = "4242";
    i = 1;
    while (i < argc)
    {
        if (i + 1 >= argc || (strcmp(argv[i], "-p") != 0 && strcmp(argv[i], "-h") != 0))
            return (1);
        if (strcmp(argv[i], "-p") == 0)
            *port = argv[i + 1];
        else
            *host = argv[i + 1];
        i += 2;
    }
    return (0);
}

int main(int argc, char **argv)
{
    t_gfx gfx;
    XEvent event;
    XClassHint class_hint;
    const char *host;
    const char *port;
    char welcome[64];

    memset(&gfx, 0, sizeof(gfx));
    if (parse_args(argc, argv, &host, &port) != 0)
    {
        fprintf(stderr, "Usage: %s [-p port] [-h hostname]\n", argv[0]);
        return (1);
    }
    gfx.fd = connect_server(host, port);
    if (gfx.fd < 0 || read_line(&gfx, welcome, sizeof(welcome)) != 0 ||
        strcmp(welcome, "BIENVENUE") != 0 || send_text(gfx.fd, "GRAPHIC\n") != 0 ||
        load_server_map(&gfx) != 0)
    {
        fprintf(stderr, "gfx: could not load map from %s:%s\n", host, port);
        if (gfx.fd >= 0)
            close(gfx.fd);
        return (1);
    }
    gfx.display = XOpenDisplay(NULL);
    if (gfx.display == NULL)
    {
        fprintf(stderr, "gfx: cannot open X display\n");
        return (1);
    }
    gfx.screen = DefaultScreen(gfx.display);
    gfx.window = XCreateSimpleWindow(gfx.display, RootWindow(gfx.display, gfx.screen),
        80, 80, WINDOW_WIDTH, WINDOW_HEIGHT, 1,
        color(gfx.display, 0x0d1713), color(gfx.display, 0x1d3028));
    class_hint.res_name = (char *)"gfx";
    class_hint.res_class = (char *)"ZappyGfx";
    XSetClassHint(gfx.display, gfx.window, &class_hint);
    XSelectInput(gfx.display, gfx.window, ExposureMask | ButtonPressMask | StructureNotifyMask);
    gfx.delete_message = XInternAtom(gfx.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(gfx.display, gfx.window, &gfx.delete_message, 1);
    gfx.context = XCreateGC(gfx.display, gfx.window, 0, NULL);
    gfx.selected_x = -1;
    gfx.selected_y = -1;
    set_title(&gfx, "Zappy graphical client");
    XMapWindow(gfx.display, gfx.window);
    while (1)
    {
        fd_set read_fds;
        struct timeval timeout;
        int display_fd;
        int ready;
        char line[256];

        display_fd = ConnectionNumber(gfx.display);
        FD_ZERO(&read_fds);
        FD_SET(display_fd, &read_fds);
        FD_SET(gfx.fd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        ready = select((display_fd > gfx.fd ? display_fd : gfx.fd) + 1,
            &read_fds, NULL, NULL, &timeout);
        if (ready < 0)
            break;
        if (FD_ISSET(gfx.fd, &read_fds))
        {
            if (read_line(&gfx, line, sizeof(line)) != 0)
                break;
            process_server_line(&gfx, line);
            draw_map(&gfx);
        }
        if (FD_ISSET(display_fd, &read_fds) && XPending(gfx.display) > 0)
        {
            XNextEvent(gfx.display, &event);
            if (event.type == ClientMessage &&
                (Atom)event.xclient.data.l[0] == gfx.delete_message)
                break;
            if (event.type == Expose)
                draw_map(&gfx);
            else if (event.type == ButtonPress)
                select_tile(&gfx, event.xbutton.x, event.xbutton.y);
        }
    }
    XFreeGC(gfx.display, gfx.context);
    XDestroyWindow(gfx.display, gfx.window);
    XCloseDisplay(gfx.display);
    close(gfx.fd);
    return (0);
}
