CC=gcc
CFLAGS=-g -lpthread
all:
	$(CC) -o client client.c common.c	$(CFLAGS)
	$(CC) -o server server.c common.c	$(CFLAGS)
server:
	$(CC) -o server server.c common.c	$(CFLAGS)
client:
	$(CC) -o client client.c common.c	$(CFLAGS)
