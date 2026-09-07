CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror $(shell pkg-config --cflags x11)
LDFLAGS = $(shell pkg-config --libs x11)
NAME = gfx
SRC = src/main.c
OBJ = $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME) $(LDFLAGS)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
