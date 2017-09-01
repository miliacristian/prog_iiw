#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>
#include <netinet/in.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <zconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wait.h>
#include <zconf.h>
#include <wchar.h>
#include <signal.h>
#include <sys/time.h>
#include "basic.h"
#ifndef PROG_IIW_LIST_H
#define PROG_IIW_LIST_H


#endif

//Creates a new Node and returns pointer to it.
struct node* get_new_node(int seq,struct timespec timespec,int timer_ms);
int delete_head(struct node** head, struct node* old_head);
void insert_at_head(struct node* new_node, struct node** head,struct node** tail);
void insert_ordered(int seq,struct timespec timespec,int timer_ms, struct node** head, struct node** tail);
void print(struct node* head);
void reverse_print(struct node* head);
void initialize_timeval(struct timespec *tv,int timer_ms);