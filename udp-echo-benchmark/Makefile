NAME			=	udp-echo-bench-server

# flags

# command
CC				=	gcc
RM				=	rm -rf

# directory
SRC_DIR			=	srcs/
INC_DIR			=	./includes

# srcs
SRCS			=	$(SRC_DIR)main.c
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