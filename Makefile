W = -Wall -Wextra -Werror

all: main

main: main.c
	gcc -o main main.c $(W) -ggdb3

clean:
	rm liblambda*.so
