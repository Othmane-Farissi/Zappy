#include <X11/Xlib.h>
#include <X11/Xutil.h>
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

typedef struct s_gfx
{
    Display *display;
    int screen;
    Window window;
    GC context;
    Atom delete_message;
    unsigned int resources[MAP_HEIGHT][MAP_WIDTH][RESOURCE_COUNT];
    unsigned int players[MAP_HEIGHT][MAP_WIDTH];
    int selected_x;
    int selected_y;
} t_gfx;

static const unsigned long resource_colors[RESOURCE_COUNT] = {
    0xe0b84f, 0x74b9d8, 0xb07c4f, 0x9b9b9b, 0xd58bc3, 0x8a72c9, 0x65b97a
};

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

static void initialize_map(t_gfx *gfx)
{
    int x;
    int y;
    int resource;

    y = 0;
    while (y < MAP_HEIGHT)
    {
        x = 0;
        while (x < MAP_WIDTH)
        {
            resource = 0;
            while (resource < RESOURCE_COUNT)
            {
                gfx->resources[y][x][resource] = (unsigned int)((x * 3 + y * 5 + resource) % 3);
                resource++;
            }
            gfx->players[y][x] = 0;
            x++;
        }
        y++;
    }
    gfx->players[MAP_HEIGHT / 2][MAP_WIDTH / 2] = 1;
    gfx->selected_x = -1;
    gfx->selected_y = -1;
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
    if (gfx->players[y][x] > 0)
    {
        XSetForeground(gfx->display, gfx->context, color(gfx->display, 0xe9f1dc));
        XFillArc(gfx->display, gfx->window, gfx->context,
            x * TILE_SIZE + 16, y * TILE_SIZE + 17, 20, 20, 0, 360 * 64);
        XSetForeground(gfx->display, gfx->context, color(gfx->display, 0x163128));
        XDrawString(gfx->display, gfx->window, gfx->context,
            x * TILE_SIZE + 24, y * TILE_SIZE + 31, "1", 1);
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
    while (y < MAP_HEIGHT)
    {
        x = 0;
        while (x < MAP_WIDTH)
        {
            draw_tile(gfx, x, y);
            x++;
        }
        y++;
    }
    XSetForeground(gfx->display, gfx->context, color(gfx->display, 0x17221e));
    XFillRectangle(gfx->display, gfx->window, gfx->context,
        0, MAP_HEIGHT * TILE_SIZE, WINDOW_WIDTH, PANEL_HEIGHT);
    snprintf(status, sizeof(status), "Zappy map  |  click a square to inspect it");
    XSetForeground(gfx->display, gfx->context, color(gfx->display, 0xe9f1dc));
    XDrawString(gfx->display, gfx->window, gfx->context,
        10, MAP_HEIGHT * TILE_SIZE + 21, status, (int)strlen(status));
    XFlush(gfx->display);
}

static void select_tile(t_gfx *gfx, int pixel_x, int pixel_y)
{
    char title[256];
    int resource;
    int x;
    int y;
    int total;

    if (pixel_y >= MAP_HEIGHT * TILE_SIZE)
        return;
    x = pixel_x / TILE_SIZE;
    y = pixel_y / TILE_SIZE;
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT)
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

int main(void)
{
    t_gfx gfx;
    XEvent event;
    XClassHint class_hint;

    memset(&gfx, 0, sizeof(gfx));
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
    initialize_map(&gfx);
    set_title(&gfx, "Zappy graphical client");
    XMapWindow(gfx.display, gfx.window);
    while (1)
    {
        XNextEvent(gfx.display, &event);
        if (event.type == ClientMessage && (Atom)event.xclient.data.l[0] == gfx.delete_message)
            break;
        if (event.type == Expose)
            draw_map(&gfx);
        else if (event.type == ButtonPress)
            select_tile(&gfx, event.xbutton.x, event.xbutton.y);
    }
    XFreeGC(gfx.display, gfx.context);
    XDestroyWindow(gfx.display, gfx.window);
    XCloseDisplay(gfx.display);
    return (0);
}
