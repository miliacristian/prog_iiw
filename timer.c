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

/*void make_timers(struct window_snd_buf *win_buf, int W) {
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
    printf("timer creati\n");
    return;
}*/

void set_timer(struct itimerspec *its, int msec) {
    int msec2 = msec%1000;
    int sec =(msec-msec2)/1000;
    its->it_interval.tv_sec = 0;
    its->it_interval.tv_nsec = 0;
    its->it_value.tv_sec = sec;
    its->it_value.tv_nsec = msec2 * 1000000;//conversione nanosecondi millisecondi
    return;
}
/*void stop_all_timers(struct window_snd_buf* win_buf_snd, int W){
    for (int i = 0; i <( 2*W); i++){
        timer_delete(win_buf_snd[i].time_id);
    }
    printf("tutti i timer sono stoppati\n");
    return;
}*/