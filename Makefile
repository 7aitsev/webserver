CC = gcc
CFLAGS = -Wall -c

.PHONY: all, clean

all: main config handler server
	$(CC) main.o config.o handler.o server.o -Wall -o webserver

main: main.c
	$(CC) main.c $(CFLAGS) -o main.o

config: config.h config.c
	$(CC) config.c $(CFLAGS) -o config.o

server: server.h server.c config
	$(CC) server.c $(CFLAGS) -o server.o

handler: handler.h handler.c
	$(CC) handler.c $(CFLAGS) -o handler.o

clean:
	-rm handler.o server.o config.o main.o webserver 2>/dev/null
