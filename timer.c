#include <time.h>
#include "basic.h"
#include "io.h"
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
    printf("time_cur %ld time_tx %ld\n", time_ms_cur, time_ms_tx);
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
double calculate_dev_RTT(double est_RTT, double sample_RTT, double dev_RTT){//calcola il dev_rtt
    if(est_RTT<0 || sample_RTT<0 || dev_RTT<0){
        handle_error_with_exit("error in calculate dev_RTT\n");
    }
    double diff_positive = absolute(sample_RTT-est_RTT);
    if(diff_positive<0){
        handle_error_with_exit("error in calculate dev_RTT\n");
    }
    dev_RTT = dev_RTT-(dev_RTT/4)+((diff_positive)/4);
    return dev_RTT;
}
int calculate_timeout(double est_RTT, double dev_RTT){//calcola il timeout finale
    if(est_RTT<0 || dev_RTT<0){
        handle_error_with_exit("error calculate timeout\n");
    }
    double timeout = est_RTT + (dev_RTT*4);
    if(timeout<0){
        handle_error_with_exit("error in calculate timeout\n");
    }
    else if(timeout<=1){
        return 1;
    }
    else if (timeout >=1000){
        return 1000;
    }
    return (int)timeout;
}

void adaptive_timer(struct shm_sel_repeat* shm, int seq){//dopo aver ricevuto l'ack calcola il nuovo tempo di ritrasmissione
    int timeout;
    int sample;
    if(shm==NULL || seq<0 || seq>(shm->param.window*2-1)){
        handle_error_with_exit("error in adaptive_timer\n");
    }
    //printf("timer in finestra sec %ld nsec %ld acked %d\n", (shm->win_buf_snd[seq].time).tv_sec,(shm->win_buf_snd[seq].time).tv_nsec, (shm->win_buf_snd[seq].acked));
    sample= calculate_sample_RTT((shm->win_buf_snd[seq].time));
    //printf("sample %d\n", sample);
    shm->est_RTT_ms = calculate_est_RTT(shm->est_RTT_ms, sample);
    shm->dev_RTT_ms=calculate_dev_RTT(shm->est_RTT_ms,sample,shm->dev_RTT_ms);
    timeout = calculate_timeout(shm->est_RTT_ms,shm->dev_RTT_ms);
    //printf("timeout %d\n", timeout);
    shm->param.timer_ms = timeout;
    return;
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
