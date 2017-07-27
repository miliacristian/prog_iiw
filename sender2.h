#include "basic.h"
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

struct window_snd_buf{//struttura per memorizzare info sui pacchetti da inviare
// se diventa pesante come memoria è meglio allocata nell'heap?
    int acked;
    char payload[MAXPKTSIZE-4];
    timer_t time_id;
};
struct temp_buf{//struttura per mandare i pacchetti
    int seq_numb;
    char payload[MAXPKTSIZE-4];
};

struct addr{
    int sockfd;
    struct sockaddr_in dest_addr;
};

void make_timers(struct window_snd_buf*win_buf,int W);
void set_timer(struct itimerspec*its,int sec,long msec);
void reset_timer(struct itimerspec*its);
void timer_handler(int sig, siginfo_t *si,void *uc);
int selective_repeat_sender(int sockfd,int fd,int byte_expected,struct sockaddr_in dest_addr,int W,double tim,double loss_prob);
