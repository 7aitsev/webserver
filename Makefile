.PHONY: all, clean

all: main.c handler
	gcc main.c handler.o -Wall -g -o webserver

handler: handler.h handler.c
	gcc handler.c -c -Wall -g -o handler.o

clean:
	-rm handler.o webserver 2>/dev/null
