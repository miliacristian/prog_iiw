#include <time.h>
#include "basic.h"
#include "timer.h"
#include "Client.h"
#include "Server.h"
#include "dynamic_list.h"


long  calculate_time_left(struct node node){//ritorna il numero di millisecondi(tv-getttimeofday)
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

void sleep_struct(struct timespec* sleep_time, long timer_ns_left){//scrive i timer_ns_left nanosecondi dentro la struct timespec
    if(timer_ns_left<0){
        handle_error_with_exit("error in sleep struct\n");
    }
    if(sleep_time==NULL){
        handle_error_with_exit("error in sleep_struct\n");
    }
    sleep_time->tv_nsec = timer_ns_left % 1000000000;
    sleep_time->tv_sec = (timer_ns_left - (sleep_time->tv_nsec))/1000000000;
    return;
}

int calculate_sample_RTT(struct timespec tx_time){//calcola il sample_rtt
// sample==istante di tempo attuale-istante di tempo in cui è stato mandato il pacchetto
    if ((tx_time.tv_sec == 0) && (tx_time.tv_nsec == 0)){
        handle_error_with_exit("sample_RTT di pacchetto già riscontrato \n");
    }
    struct timespec time_current;
    long time_ms_cur;
    long time_ms_tx;
    int sample_RTT;
    if(clock_gettime(CLOCK_MONOTONIC,&time_current)!=0){
        handle_error_with_exit("error in clock gettime\n");
    }
    time_ms_cur=(time_current.tv_nsec/1000000)+(time_current.tv_sec*1000);
    time_ms_tx=(tx_time.tv_nsec/1000000)+(tx_time.tv_sec*1000);
    sample_RTT =time_ms_cur - time_ms_tx;
    if ( sample_RTT == 0){
        return 1;
    }else if( sample_RTT < 0){
        handle_error_with_exit("wrong sample_RTT\n");
    }
    return sample_RTT;
}

double calculate_est_RTT(double est_RTT, double sample_RTT){//calcola l'estimated_rtt
    if(est_RTT<0 || sample_RTT<0){
        handle_error_with_exit("error in calculate est_RTT\n");
    }
    est_RTT = est_RTT -(est_RTT/8) + (sample_RTT/8);
    return est_RTT;
}

void adaptive_timer(struct shm_sel_repeat* shm, int seq){//dopo aver ricevuto l'ack calcola il nuovo tempo di ritrasmissione
    int sample;
    if(shm==NULL || seq<0 || seq>(shm->param.window*2-1)){
        handle_error_with_exit("error in adaptive_timer\n");
    }
    sample= calculate_sample_RTT((shm->win_buf_snd[seq].time));
    printf("sample %d\n", sample);
    shm->est_RTT_ms = calculate_est_RTT(shm->est_RTT_ms, sample);
    shm->param.timer_ms = (int)(shm->est_RTT_ms)<<1;
    if(shm->param.timer_ms>1000){
        shm->param.timer_ms=1000;
    }
    printf("timeout %d\n",shm->param.timer_ms);
    return;
}
