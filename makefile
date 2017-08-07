CC = gcc
CFLAGS =-Wall -Wextra -Wpedantic -o2
CFILES=$(shell ls *.c)
PROGS=$(CFILES:%.c=%)

all: $(PROGS) 
	
server:Server.c basic.c  io.c io.h parser.c basic.h parser.h sender2.c timer.c timer.h get_server.c get_server.h
	$(CC) $(CFLAGS) -pthread -o  $@ $^ -lrt

client:Client.c basic.c  io.c io.h parser.c basic.h parser.h timer.c timer.h get_client.c get_client.h
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt

prova:basic.c basic.h
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt
