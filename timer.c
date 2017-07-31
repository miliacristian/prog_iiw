#include <time.h>
#include "timer.h"
#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

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

void make_timeout_timer(timer_t* timer_id){
    struct sigevent te;
    int sigNo = SIGRTMIN+1;
    te.sigev_notify = SIGEV_SIGNAL;//quando scade il timer manda il segnale specificato
    te.sigev_signo = sigNo;
    te.sigev_value.sival_int=12;
    if (timer_create(CLOCK_REALTIME, &te,timer_id) == -1) {//inizializza nella struct il timer i-esimo
        handle_error_with_exit("error in timer_create\n");
    }
    //printf("timer id is 0x%lx\n",(long)*ptr);
    return;

}

void start_timeout_timer(timer_t timer_id, long msec){
    struct itimerspec its;
    set_timer(&its, msec);
    if (timer_settime(timer_id, 0, &its, NULL) == -1) {//avvio timer
        handle_error_with_exit("error in timer_settime\n");
    }
    return;
}

void make_timers(struct window_snd_buf *win_buf, int W) {
    struct sigevent te;
    memset(&te,0,sizeof(struct sigevent));
    int sigNo = SIGRTMIN;
    te.sigev_notify = SIGEV_SIGNAL;//quando scade il timer manda il segnale specificato
    te.sigev_signo = sigNo;//manda il segnale sigrtmin
    for (int i = 0; i < 2 * W; i++) {
        te.sigev_value.sival_ptr = &(win_buf[i]);//associo ad ogni timer l'indirizzo della struct i-esima
        if (timer_create(CLOCK_REALTIME, &te,&(win_buf[i].time_id)) == -1) {//inizializza nella struct il timer i-esimo
            handle_error_with_exit("error in timer_create\n");
        }
        //printf("timer id is 0x%lx\n",(long)*ptr);
    }
    return;
}

void set_timer(struct itimerspec *its, long msec) {
    int msec2 = msec%1000;
    int sec =(msec-msec2)/10000;
    its->it_interval.tv_sec = 0;
    its->it_interval.tv_nsec = 0;
    its->it_value.tv_sec = sec;
    its->it_value.tv_nsec = msec2 * 1000000;//conversione nanosecondi millisecondi
    printf("msec %d, sec %d\n", msec2, sec);
    return;
}

void stop_all_timers(struct window_snd_buf* win_buf_snd, int W){
    for (int i = 0; i < 2*W; i++){
        timer_delete(win_buf_snd[i].time_id);
    }
    return;
}