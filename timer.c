#include <time.h>
#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "timer.h"
#include "Client.h"
#include "Server.h"
#include "list_client.h"
#include "list_server.h"
#include "get_client.h"
#include "get_server.h"
#include "communication.h"
#include "dynamic_list.h"



void start_timer(timer_t timer_id, struct itimerspec *its){
    if (timer_settime(timer_id, 0, its, NULL) == -1) {//avvio timer
        handle_error_with_exit("error in timer_settime\n");
    }
}

void stop_timer(timer_t timer_id){
    struct itimerspec its;
    set_timer(&its, 0);
    if (timer_settime(timer_id, 0, &its, NULL) == -1) {//arresto timer
        handle_error_with_exit("error in timer_settime\n");
    }
}
void stop_timeout_timer(timer_t timer_id){
    struct itimerspec its;
    set_timer(&its, 0);
    if (timer_settime(timer_id, 0, &its, NULL) == -1) {//arresto timer
        handle_error_with_exit("error in timer_settime\n");
    }
    printf("timer stoppato\n");
}

void make_timeout_timer(timer_t* timer_id){
    struct sigevent te;
    int sigNo = SIGRTMIN+1;
    te.sigev_notify = SIGEV_SIGNAL;//quando scade il timer manda il segnale specificato
    te.sigev_signo = sigNo;
    //te.sigev_value.sival_int=0;
    if (timer_create(CLOCK_REALTIME, &te,timer_id) == -1) {//inizializza nella struct il timer i-esimo
        handle_error_with_exit("error in timer_create\n");
    }
    //printf("timer id is 0x%lx\n",(long)*ptr);
    return;

}

void start_timeout_timer(timer_t timer_id, int msec){
    struct itimerspec its;
    set_timer(&its, msec);
    if (timer_settime(timer_id, 0, &its, NULL) == -1) {//avvio timer
        handle_error_with_exit("error in timer_settime\n");
    }
    printf("timer avviato\n");
    return;
}
long  calculate_time_left(struct node node){
    //ritorna il numero di millisecondi(tv-getttimeofday)
    struct timespec time_current;
    long time_ns_cur;
    long time_ns_timespec;
    long time_ns_left;
    if(clock_gettime(CLOCK_MONOTONIC,&time_current)!=0){
        handle_error_with_exit("error in gettimeofday\n");
    }
    initialize_timeval(&(node.tv), node.timer_ms);
    time_ns_cur=(time_current.tv_nsec)+(time_current.tv_sec*1000000000);
    time_ns_timespec=(node.tv.tv_nsec)+(node.tv.tv_sec*1000000000);
    time_ns_left= time_ns_timespec-time_ns_cur;
    return time_ns_left;
}

void sleep_struct(struct timespec* sleep_time, long timer_ns_left){
    sleep_time->tv_nsec = timer_ns_left % 1000000000;
    sleep_time->tv_sec = (timer_ns_left - (sleep_time->tv_nsec))/100000000;
    return;
}

void set_timer(struct itimerspec *its, int msec) {
    int msec2 = msec%1000;
    int sec =(msec-msec2)/1000;
    its->it_interval.tv_sec = 0;
    its->it_interval.tv_nsec = 0;
    its->it_value.tv_sec = sec;
    its->it_value.tv_nsec = msec2 * 1000000;//conversione nanosecondi millisecondi
    return;
}