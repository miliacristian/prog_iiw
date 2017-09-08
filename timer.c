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

int calculate_sample_RTT(struct timespec tx_time){
    if (tx_time.tv_sec == 0 && tx_time.tv_nsec == 0){
        handle_error_with_exit("sample_RTT di pacchetto già riscontrato \n");
    }
    struct timespec time_current;
    long time_ms_cur;
    long time_ms_tx;
    int sample_RTT;
    if(clock_gettime(CLOCK_MONOTONIC,&time_current)!=0){
        handle_error_with_exit("error in gettimeofday\n");
    }
    //printf("time current sec %ld nsec %ld\n", time_current.tv_sec ,time_current.tv_nsec);
    //printf("time tx sec %ld nsec %ld\n", tx_time.tv_sec ,tx_time.tv_nsec);
    time_ms_cur=(time_current.tv_nsec/1000000)+(time_current.tv_sec*1000);
    time_ms_tx=(tx_time.tv_nsec/1000000)+(tx_time.tv_sec*1000);
    //printf("time_cur %ld time_tx %ld\n", time_ms_cur, time_ms_tx);
    sample_RTT = time_ms_cur - time_ms_tx;
    if ( sample_RTT == 0){
        return 1;
    }else if( sample_RTT < 0){
        handle_error_with_exit("wrong sample_RTT\n");
    }
    return sample_RTT;
}

double absolute(double value){
    if (value<0){
        return (value*(-1));
    }
    return value;
}
double calculate_est_RTT(double est_RTT, double sample_RTT){
    est_RTT = est_RTT -(est_RTT/8) + (sample_RTT/8);
    return est_RTT;
}
double calculate_dev_RTT(double est_RTT, double sample_RTT, double dev_RTT){
    double diff = absolute(sample_RTT-est_RTT);
    dev_RTT = dev_RTT-(dev_RTT/4)+((diff)/4);
    return dev_RTT;
}
int calculate_timeout(double est_RTT, double dev_RTT){
    double timeout = est_RTT + (dev_RTT*4);
    if(timeout<=1){
        return 1;
    }
    else if (timeout > 1000){
        return 1000;
    }
    return (int)timeout;
}

void adaptive_timer(struct shm_sel_repeat* shm, int seq){
    int timeout;
    //printf("adaptive timer struct in finestra, sec %ld nsec %ld\n", shm->win_buf_snd[seq].time.tv_sec, shm->win_buf_snd[seq].time.tv_nsec);
   // printf ("seq %d\n", seq);
    int sample = calculate_sample_RTT((shm->win_buf_snd[seq].time));
    //printf("sample %d\n", sample);
    shm->est_RTT_ms = calculate_est_RTT(shm->est_RTT_ms, sample);
    //printf("est_timer %f\n", shm->est_RTT_ms);
    shm->dev_RTT_ms=calculate_dev_RTT(shm->est_RTT_ms,sample,shm->dev_RTT_ms);
    //printf("dev_timer %f\n", shm->dev_RTT_ms);
    timeout = calculate_timeout(shm->est_RTT_ms,shm->dev_RTT_ms);
    printf("timeout %d\n", timeout);
    shm->param.timer_ms = timeout;
    //handle_error_with_exit("");
}

void sleep_struct(struct timespec* sleep_time, long timer_ns_left){
    if(sleep_time==NULL){
        handle_error_with_exit("error in sleep_struct\n");
    }
    sleep_time->tv_nsec = timer_ns_left % 1000000000;
    sleep_time->tv_sec = (timer_ns_left - (sleep_time->tv_nsec))/1000000000;
    return;
}
