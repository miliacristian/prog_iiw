#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>
#include "basic.h"
#include "io.h"
#include <signal.h>
#include "sender2.h"
#include <assert.h>
#include <wchar.h>

void make_timers(struct window_snd_buf*win_buf,int W){
    struct sigevent te;
    int sigNo = SIGRTMIN;
    te.sigev_notify = SIGEV_SIGNAL;//quando scade il timer manda il segnale specificato
    te.sigev_signo = sigNo;//manda il segnale sigrtmin
    for(int i=0;i<2*W;i++){
        te.sigev_value.sival_int =i;//associo ad ogni timer un numero di sequenza
        timer_create(CLOCK_PROCESS_CPUTIME_ID, &te,&(win_buf[i].time_id));
    }
    return;
}
void set_timer(struct itimerspec*its,int sec,long msec){
    its->it_interval.tv_sec = 0;
    its->it_interval.tv_nsec = 0;
    its->it_value.tv_sec = sec;
    its->it_value.tv_nsec = msec * 1000000;//conversione nanosecondi millisecondi
    return;
}
void reset_timer(struct itimerspec*its){
    its->it_interval.tv_sec = 0;
    its->it_interval.tv_nsec = 0;
    its->it_value.tv_sec = 0;
    its->it_value.tv_nsec = 0;
    return;
}


struct addr*addr=NULL;
struct window_snd_buf *win_buffer=NULL;
struct itimerspec sett_timer,rst_timer;//timer e reset timer globali

void timer_handler(int sig, siginfo_t *si,void *uc){
    (void)sig;
    (void)si;
    (void)uc;
    int seq_numb=si->si_value.sival_int;
    sendto(addr->sockfd,&(win_buffer[seq_numb]),MAXPKTSIZE,0,(struct sockaddr*)&(addr->dest_addr),sizeof(addr->dest_addr));//ritrasmetto il pacchetto di cui è scaduto il timer
    timer_settime(win_buffer[seq_numb].time_id,0,&sett_timer,NULL);//avvio timer
}

//per timer adattivo sull'ack resettare il timer e impostare ultimo parametro di timer_settime
int selective_repeat_sender(int sockfd,int fd,int byte_expected,struct sockaddr_in dest_addr,int W,double tim,double loss_prob){
    int window_base=0,last_sent=0,pkt_fly=0,byte_readed=0,ack_numb,sigNo = SIGRTMIN;;//win_base->primo pacc non riscontrato
    //ack_numb->numero di ack ricevuto dal receiver
    struct addr temp_addr;
    struct sigaction sa;
    struct window_snd_buf win_buf[2*W];//alloco spazio per buffer trasmissione di 2W
    struct temp_buf temp_buf;//pacchetto da inviare
    socklen_t len=sizeof(struct sockaddr_in);//destaddr==a chi devo mandare i pacchetti

    make_timers(win_buf,W);//crea 2w timer
    set_timer(&sett_timer,2,200);//inizializza struct necessaria per scegliere il timer
    reset_timer(&rst_timer);//inizializza struct necessaria per resettare il timer

    win_buffer=win_buf;//inizializzo puntatore globale necessario per signal handler

    temp_addr.sockfd=sockfd;//inizializzo puntatore globale necessario per signal_handler
    temp_addr.dest_addr=dest_addr;
    addr=&temp_addr;
    //inizializza handler
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, NULL) == -1){
        printf("sigaction error\n");
        return -1;
    }
    tim=tim;
    loss_prob=loss_prob;
    memset(win_buf,0,sizeof(struct window_snd_buf)*(2*W));//inizializza a zero
    while(byte_readed<byte_expected){
        if(pkt_fly<W){//finquando i pacchetti inviati e non ancora riscontrati sono minori di W
            readn(fd,win_buf[last_sent].payload,(MAXPKTSIZE-4));//metto nel buffer il pacchetto proveniente dal file senza terminatore di stringa
            strcpy(temp_buf.payload,(win_buf[last_sent].payload));//copio dentro temp
            // buffer  i dati del pacchetto con eventuali terminatori di stringa
            temp_buf.seq_numb=last_sent;//memorizzo il numero di sequenza
            sendto(sockfd,&(temp_buf),MAXPKTSIZE,0,(struct sockaddr*)&dest_addr,sizeof(dest_addr));//invio temp_buf==invio pacchetto
            timer_settime(win_buf[last_sent].time_id, 0, &sett_timer, NULL);//avvio timer
            pkt_fly++;//segno che ho inviato un pacchetto
            last_sent=(last_sent+1)%(2*W);//incremento ultimo inviato
        }
        while(recvfrom(sockfd,&ack_numb,sizeof(int),MSG_DONTWAIT,(struct sockaddr*)&dest_addr,&len)!=-1){//flag non bloccante,
            //finquando ci sono ack segnali
            if(seq_is_in_window(window_base,window_base+W-1,W,ack_numb)){
                //se è dentro la finestra->ack con numero di sequenza dentro finestra ricevuto
                timer_settime(win_buf[ack_numb].time_id,0,&rst_timer,NULL);//resetta il timer
                win_buf[ack_numb].acked=1;//segna pkt come riscontrato
                if(ack_numb==window_base){//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while(win_buf[window_base].acked==1){//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        byte_readed+=strlen(win_buf[window_base].payload);//segno come letti
                        win_buf[window_base].acked=0;//resetta quando scorri finestra
                        window_base=(window_base+1)%(2*W);//avanza la finestra
                        pkt_fly--;
                    }
                }
            }
            else{//se è fuori la finestra ignoralo
            }
        }
    }
    return byte_readed;
}
