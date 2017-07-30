.PHONY: all, clean

all: main.c handler server
	gcc main.c handler.o server.o -Wall -g -o webserver

server: server.h server.c
	gcc server.c -c -Wall -g -o server.o
handler: handler.h handler.c
	gcc handler.c -c -Wall -g -o handler.o

clean:
	-rm handler.o server.o webserver 2>/dev/null
