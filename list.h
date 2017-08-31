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
struct Node* GetNewNode(int seq,int timer_ms);
int deleteHead(struct Node** head, struct Node* oldHead);
void InsertAtHead(struct Node* newNode, struct Node** head,struct Node** tail);
void InsertOrdered(int seq,int timer_ms, struct Node** head, struct Node** tail);
void Print(struct Node* head);
void ReversePrint(struct Node* head);
void initialize_timeval(struct timeval *tv,int timer_ms);