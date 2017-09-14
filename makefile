CC = gcc
CFLAGS =-Wall -Wextra -Wpedantic -o2
CFILES=$(shell ls *.c)
PROGS=$(CFILES:%.c=%)

install:server client
	
server:Server.c Server.h basic.c file_lock.c file_lock.h basic.h io.c io.h dynamic_list.c dynamic_list.h parser.c parser.h timer.c timer.h get_server.c get_server.h communication.h communication.c list_server.c list_server.h put_server.h put_server.c
	$(CC) $(CFLAGS) -pthread -o  $@ $^ -lrt

client:Client.c Client.h basic.c basic.h file_lock.c file_lock.h io.c io.h dynamic_list.c dynamic_list.h parser.c  parser.h timer.c timer.h get_client.c get_client.h communication.h communication.c list_client.c list_client.h put_client.h put_client.c
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt
