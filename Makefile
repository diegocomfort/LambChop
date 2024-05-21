W = -Wall -Wextra -Werror #-fsanitize=leak -ggdb

all: main

main: main.c lambchop.h
	gcc -o main main.c $(W)

clean:
	rm main

reset: clean main
