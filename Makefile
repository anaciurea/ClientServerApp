CC = gcc
CFLAGS = -Wall -g

all: server subscriber

server: server.c protocol.h
	$(CC) server.c -o server $(CFLAGS)

subscriber: subscriber.c protocol.h
	$(CC) subscriber.c -o subscriber $(CFLAGS)

clean:
	rm -f server subscriber