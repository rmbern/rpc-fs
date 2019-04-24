CC=gcc
CFLAGS=-g -lpthread
ARFLAGS=rs
remotefile:
	$(CC) -c client.c common.c	$(CFLAGS)
	$(CC) -o server server.c common.c	$(CFLAGS)
	ar rs libremotefile.a client.o client.h
	$(CC) -o test test.c common.c -L . -lremotefile $(CFLAGS)
