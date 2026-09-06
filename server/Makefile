CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -Iinclude
NAME = server
SRC = src/main.c src/tools/init.c src/tools/parse.c src/network/network.c
OBJ = $(SRC:.c=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(NAME)

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
