MAKEFLAGS += --no-print-directory

all: server client gfx

server:
	$(MAKE) -C server

client:
	$(MAKE) -C client

gfx:
	$(MAKE) -C gfx

clean:
	$(MAKE) -C server clean
	$(MAKE) -C client clean
	$(MAKE) -C gfx clean

fclean:
	$(MAKE) -C server fclean
	$(MAKE) -C client fclean
	$(MAKE) -C gfx fclean

re: fclean all

.PHONY: all server client gfx clean fclean re
