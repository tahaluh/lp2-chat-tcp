CC=gcc
CFLAGS=-Wall -pthread

all: server client

server: server.c tslog.c
	$(CC) $(CFLAGS) server.c tslog.c -o server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f server client *.o server.log
