NAME			=	load-generator

# flags
FLAGS			=	-g -O2 -Wall -Wextra

# command
CC				=	gcc
RM				=	rm -rf

# directory
SRC_DIR			=	srcs/
INC_DIR			=	./includes

# srcs
SRCS			=	$(SRC_DIR)main.c $(SRC_DIR)file_io.c $(SRC_DIR)report.c $(SRC_DIR)stats.c $(SRC_DIR)time_utils.c $(SRC_DIR)worker.c
OBJS			=	$(SRCS:.cpp=.o)

all:	$(NAME)

.cpp.o: 
	$(CC) -c $< -o $@ -I $(INC_DIR)

$(NAME): $(OBJS)
	$(CC) -o $(NAME) $(OBJS) -I $(INC_DIR)

clean:
	rm -f $(OBJS) ./srcs/$(NAME)

fclean: clean
	$(RM) $(NAME)

re:	fclean all

.PHONY: all clean fclean re