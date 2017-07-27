CC = gcc
CFLAGS =-Wall -Wextra -o2
CFILES=$(shell ls *.c)
PROGS=$(CFILES:%.c=%)

all: $(PROGS) 
	
server:Server.c basic.c file_lock.c  io.c io.h parser.c receiver.h basic.h file_lock.h  lock_fcntl.c parser.h sender2.c receiver.c sender2.h
	$(CC) $(CFLAGS) -pthread -o  $@ $^ -lrt

client:Client.c basic.c file_lock.c  io.c io.h parser.c receiver.h basic.h file_lock.h  lock_fcntl.c parser.h sender2.c receiver.c sender2.h
	$(CC) $(CFLAGS) -pthread  -o  $@ $^ -lrt
