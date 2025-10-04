CC=gcc
CFLAGS=-Wall -pthread

all: main

main: main.c tslog.c
	$(CC) $(CFLAGS) main.c tslog.c -o main

clean:
	rm -f main *.o saida.log
