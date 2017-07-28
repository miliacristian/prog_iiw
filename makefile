CC = gcc
CFLAGS =-Wall -Wextra -Wpedantic -o2
CFILES=$(shell ls *.c)
PROGS=$(CFILES:%.c=%)

all: $(PROGS) 
	
server:Server.c basic.c  io.c io.h parser.c receiver.h basic.h parser.h sender2.c receiver.c sender2.h
	$(CC) $(CFLAGS) -pthread -o  $@ $^ -lrt

client:Client.c basic.c  io.c io.h parser.c receiver.h basic.h parser.h sender2.c receiver.c sender2.h
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt

prova:basic.c basic.h
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt
