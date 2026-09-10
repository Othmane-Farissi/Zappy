# Zappy

Zappy 42 project: a single-threaded multiplayer game server, autonomous AI
client, and graphical client.

## Build

From the repository root:

```sh
make
```

This builds `server/server`, `client/client`, and `gfx/gfx`.

Generated `.o` files are intermediate compilation artifacts. They are kept by
`make` so unchanged source files do not need to be compiled again. Remove them
and the binaries with:

```sh
make clean
make fclean
```

No `.env` file is required. The subject defines server configuration through
command-line arguments:

```sh
./server/server -p 4275 -x 12 -y 8 -n team -c 4 -t 100
```

## Run

In separate terminals:

```sh
./server/server -p 4275 -x 12 -y 8 -n team -c 4 -t 100
./gfx/gfx -p 4275
./client/client -n team -p 4275
```

The source is organized by executable. Each executable has its own `src/`,
`include/` where shared interfaces are needed, and `Makefile`. The server
keeps startup/configuration code under `src/tools/` and socket/game-loop code
under `src/network/`; gameplay responsibilities are being split further as
the implementation grows.
