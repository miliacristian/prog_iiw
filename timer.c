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
    time_ms_cur=(time_current.tv_nsec/1000000)+(time_current.tv_sec*1000);
    time_ms_tx=(tx_time.tv_nsec/1000000)+(tx_time.tv_sec*1000);
    sample_RTT = time_ms_cur - time_ms_tx;
    if ( sample_RTT == 0){
        return 1;
    }else if( sample_RTT < 0){
        handle_error_with_exit("wrong sample_RTT\n");
    }
    return sample_RTT;
}

int calculate_est_RTT(int est_RTT, int sample_RTT){
    est_RTT = est_RTT -(est_RTT>>3) + sample_RTT>>3;
    return est_RTT;
}
int calculate_dev_RTT(int est_RTT, int sample_RTT, int dev_RTT){
    int diff = abs(sample_RTT-est_RTT);
    dev_RTT = dev_RTT-(dev_RTT>>2)+((diff)>>2);
    return dev_RTT;
}

int calculate_timeout(int est_RTT, int dev_RTT){
    int timeout = est_RTT + (dev_RTT<<2);
    return timeout;
}

void sleep_struct(struct timespec* sleep_time, long timer_ns_left){
    if(sleep_time==NULL){
        handle_error_with_exit("error in sleep_struct\n");
    }
    sleep_time->tv_nsec = timer_ns_left % 1000000000;
    sleep_time->tv_sec = (timer_ns_left - (sleep_time->tv_nsec))/1000000000;
    return;
}
