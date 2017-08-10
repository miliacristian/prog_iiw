CC = gcc
CFLAGS =-Wall -Wextra -Wpedantic -o2
CFILES=$(shell ls *.c)
PROGS=$(CFILES:%.c=%)

all: $(PROGS) 
	
server:Server.c Server.h basic.c basic.h io.c io.h parser.c parser.h timer.c timer.h get_server.c get_server.h communication.h communication.c list_server.c list_server.h
	$(CC) $(CFLAGS) -pthread -o  $@ $^ -lrt

client:Client.c Client.h basic.c basic.h io.c io.h parser.c  parser.h timer.c timer.h get_client.c get_client.h communication.h communication.c list_client.c list_client.h
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt

prova:basic.c basic.h
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt
