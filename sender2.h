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
#include <wait.h>
#include <zconf.h>
#include "basic.h"

/*#ifndef LINE1_H
#define LINE1_H

struct window_snd_buf{//struttura per memorizzare info sui pacchetti da inviare
// se diventa pesante come memoria è meglio allocata nell'heap?
    int acked;
    char payload[MAXPKTSIZE-8];
    timer_t time_id;
    int time_start;
    int seq_numb;
};


struct addr{
    int sockfd;
    struct sockaddr_in dest_addr;
};

#endif

void make_timers(struct window_snd_buf*win_buf,int W);
void make_timeout_timer(timer_t* timer_id);
void set_timeout_timer(timer_t timer_id, struct itimerspec *its, int sec, long msec);
void reset_timeout_timer(timer_t timer_id, struct itimerspec *its);
void set_timer(struct itimerspec*its,int sec,long msec);
void reset_timer(struct itimerspec*its);
void timer_handler(int sig, siginfo_t *si,void *uc);
int selective_repeat_sender(int sockfd,int fd,int byte_expected,struct sockaddr_in dest_addr);
void start_timeout_timer(timer_t timer_id, int sec, long msec);*/
