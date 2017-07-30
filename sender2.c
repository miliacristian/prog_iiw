#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

char connection_failed = 0;

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

void set_timer(struct itimerspec *its, int sec, long msec) {
    its->it_interval.tv_sec = 0;
    its->it_interval.tv_nsec = 0;
    its->it_value.tv_sec = sec;
    its->it_value.tv_nsec = msec * 1000000;//conversione nanosecondi millisecondi
    return;
}

void reset_timer(struct itimerspec *its) {
    its->it_interval.tv_sec = 0;
    its->it_interval.tv_nsec = 0;
    its->it_value.tv_sec = 0;
    its->it_value.tv_nsec = 0;
    return;
}


/*void server_timeout_handler(int sig, siginfo_t *si, void *uc) {
    connection_failed = 1;
    return;
}

void timer_handler_adaptive(int sig, siginfo_t *si, void *uc) {
    (void) sig;
    (void) si;
    (void) uc;
    struct window_snd_buf *win_buffer = si->si_value.sival_ptr;
    //int seq_numb=si->si_value.sival_int;
    if (sendto(addr->sockfd, ((*win_buffer).payload), MAXPKTSIZE, 0, (struct sockaddr *) &(addr->dest_addr),
               sizeof(addr->dest_addr)) == -1) {//ritrasmetto il pacchetto di cui è scaduto il timer
        handle_error_with_exit("error in sendto\n");
    }
    //da aggiornare ext_rtt
    if (timer_settime((*win_buffer).time_id, 0, &sett_timer, NULL) == -1) {//avvio timer adattativo da riscrivere
        handle_error_with_exit("error in timer_settime\n");
    }
}

//per timer adattivo sull'ack resettare il timer e impostare ultimo parametro di timer_settime
int file_sender(int sockfd, int fd, int byte_expected, struct sockaddr_in dest_addr) {
    int window_base = 0, seq_to_send = 0, pkt_fly = 0, left_bytes = byte_expected, byte_readed = 0, ack_numb, sigNo = SIGRTMIN, W = param_serv.window;//win_base->primo pacc non riscontrato
//primo pacchetto della finestra->primo non riscontrato
    double timer = param_serv.timer_ms, loss_prob = param_serv.loss_prob;
    //ack_numb->numero di ack ricevuto dal receiver
    struct addr temp_addr;
    struct sigaction sa;
    struct window_snd_buf win_buf[2 * W];//alloco spazio per buffer trasmissione di 2W
    struct temp_buf temp_buf;//pacchetto da inviare
    socklen_t len = sizeof(struct sockaddr_in);//destaddr==a chi devo mandare i pacchetti

    make_timers(win_buf, W);//crea 2w timer
    set_timer(&sett_timer, 1, 10);//inizializza struct necessaria per scegliere il timer
    reset_timer(&rst_timer);//inizializza struct necessaria per resettare il timer

    temp_addr.sockfd = sockfd;
    temp_addr.dest_addr = dest_addr;
    addr = &temp_addr;//inizializzo puntatore globale necessario per signal_handler
    //inizializza handler
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(sigNo, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }

    memset(win_buf, 0, sizeof(struct window_snd_buf) * (2 * W));//inizializza a zero
    while (byte_readed < byte_expected) {
        if (pkt_fly < W) {//finquando i pacchetti inviati e non ancora riscontrati sono minori di W
            readn(fd, win_buf[seq_to_send].payload,
                  (MAXPKTSIZE - 4));//metto nel buffer il pacchetto proveniente dal file senza terminatore di stringa
            //controllo su readn??
            strcpy(temp_buf.payload,
                   (win_buf[seq_to_send].payload));//copio dentro temp_buf i dati del pacchetto con eventuali terminatori di stringa
            temp_buf.seq_numb = seq_to_send;//memorizzo il numero di sequenza
            if (sendto(sockfd, &(temp_buf), MAXPKTSIZE, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) ==
                -1) {//invio temp_buf==invio pacchetto
                handle_error_with_exit("error in sendto\n");
            }
            timer_settime(win_buf[seq_to_send].time_id, 0, &sett_timer, NULL);//avvio timer
            pkt_fly++;//segno che ho inviato un pacchetto
            seq_to_send = (seq_to_send + 1) % (2 * W);//incremento ultimo inviato
        }
        while (recvfrom(sockfd, &ack_numb, sizeof(int), MSG_DONTWAIT, (struct sockaddr *) &dest_addr, &len) !=
               -1) {//flag non bloccante,
            //finquando ci sono ack prendili
            if (seq_is_in_window(window_base, window_base + W - 1, W, ack_numb)) {
                //se è dentro la finestra->ack con numero di sequenza dentro finestra ricevuto
                if (timer_settime(win_buf[ack_numb].time_id, 0, &rst_timer, NULL) == -1) {//resetta il timer
                    handle_error_with_exit("error in timer_settime\n");
                }
                win_buf[ack_numb].acked = 1;//segna pkt come riscontrato
                if (ack_numb == window_base) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while (win_buf[window_base].acked == 1) {//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        byte_readed += strlen(win_buf[window_base].payload);//segno come letti
                        win_buf[window_base].acked = 0;//resetta quando scorri finestra
                        window_base = (window_base + 1) % (2 * W);//avanza la finestra
                        pkt_fly--;
                    }
                }
            } else {//se è fuori la finestra ignoralo
            }
        }
    }
    strcpy(temp_buf.payload,"FIN");
    temp_buf.seq_numb = -2;//fin number
    if (sendto(sockfd, &temp_buf, MAXPKTSIZE, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) == -1) {
        handle_error_with_exit("error in sendto\n");
    }
    return byte_readed;
}


//media pesata sul rtt in ms scritta nella struct globale sett_timer

int file_sender_adaptive(int sockfd, int fd, int byte_expected, struct sockaddr_in dest_addr) {
    int window_base = 0, seq_to_send = 0, pkt_fly = 0, left_bytes = byte_expected, byte_readed = 0, ack_numb, sigNo = SIGRTMIN, W = param_serv.window;//win_base->primo pacc non riscontrato
//primo pacchetto della finestra->primo non riscontrato
    double timer = param_serv.timer_ms, loss_prob = param_serv.loss_prob;
    //ack_numb->numero di ack ricevuto dal receiver
    struct addr temp_addr;
    struct sigaction sa;
    struct window_snd_buf win_buf[2 * W];//alloco spazio per buffer trasmissione di 2W
    struct temp_buf temp_buf;//pacchetto da inviare
    socklen_t len = sizeof(struct sockaddr_in);//destaddr==a chi devo mandare i pacchetti

    make_timers(win_buf, W);//crea 2w timer
    set_timer(&sett_timer, 1, 10);//inizializza struct necessaria per scegliere il timer
    reset_timer(&rst_timer);//inizializza struct necessaria per resettare il timer

    temp_addr.sockfd = sockfd;
    temp_addr.dest_addr = dest_addr;
    addr = &temp_addr;//inizializzo puntatore globale necessario per signal_handler
    //inizializza handler
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler_adaptive;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(sigNo, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }

    memset(win_buf, 0, sizeof(struct window_snd_buf) * (2 * W));//inizializza a zero
    while (byte_readed < byte_expected) {
        if (pkt_fly < W) {//finquando i pacchetti inviati e non ancora riscontrati sono minori di W
            readn(fd, win_buf[seq_to_send].payload,
                  (MAXPKTSIZE - 4));//metto nel buffer il pacchetto proveniente dal file senza terminatore di stringa
            //controllo su readn??
            strcpy(temp_buf.payload,
                   (win_buf[seq_to_send].payload));//copio dentro temp_buf i dati del pacchetto con eventuali terminatori di stringa
            temp_buf.seq_numb = seq_to_send;//memorizzo il numero di sequenza
            if (sendto(sockfd, &(temp_buf), MAXPKTSIZE, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) ==
                -1) {//invio temp_buf==invio pacchetto
                handle_error_with_exit("error in sendto\n");
            }
            timer_settime(win_buf[seq_to_send].time_id, 0, &sett_timer, NULL);//avvio timer
            pkt_fly++;//segno che ho inviato un pacchetto
            seq_to_send = (seq_to_send + 1) % (2 * W);//incremento ultimo inviato
        }
        while (recvfrom(sockfd, &ack_numb, sizeof(int), MSG_DONTWAIT, (struct sockaddr *) &dest_addr, &len) !=
               -1) {//flag non bloccante,
            //finquando ci sono ack prendili
            if (seq_is_in_window(window_base, window_base + W - 1, W, ack_numb)) {
                //se è dentro la finestra->ack con numero di sequenza dentro finestra ricevuto
                if (timer_settime(win_buf[ack_numb].time_id, 0, &rst_timer, NULL) == -1) {//resetta il timer
                    handle_error_with_exit("error in timer_settime\n");
                }
                win_buf[ack_numb].acked = 1;//segna pkt come riscontrato
                if (ack_numb == window_base) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while (win_buf[window_base].acked == 1) {//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        byte_readed += strlen(win_buf[window_base].payload);//segno come letti
                        win_buf[window_base].acked = 0;//resetta quando scorri finestra
                        window_base = (window_base + 1) % (2 * W);//avanza la finestra
                        pkt_fly--;
                    }
                }
            } else {//se è fuori la finestra ignoralo
            }
        }
    }
    if (sendto(sockfd, "FIN", MAXPKTSIZE, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) == -1) {
        handle_error_with_exit("error in sendto\n");
    }
    return byte_readed;
}


struct temp_buf initialize_put_file(int sockfd, struct sockaddr_in dest_addr, char *command) {//-1 errore sender put
    int window_base = 0, seq_to_send = 0, ack_numb, sigNo = SIGRTMIN, W = param_serv.window;//win_base->primo pacc non riscontrato
//primo pacchetto della finestra->primo non riscontrato
    double timer = param_serv.timer_ms, loss_prob = param_serv.loss_prob;
    //ack_numb->numero di ack ricevuto dal receiver
    struct addr temp_addr;
    struct sigaction sa, s_timeout;
    struct window_snd_buf win_buf[2 * W];//alloco spazio per buffer trasmissione di 2W
    struct temp_buf temp_buf;//pacchetto da inviare
    socklen_t len = sizeof(struct sockaddr_in);//destaddr==a chi devo mandare i pacchetti

    make_timers(win_buf, W);//crea 2w timer
    set_timer(&sett_timer, 1, 10);//inizializza struct necessaria per scegliere il timer
    reset_timer(&rst_timer);//inizializza struct necessaria per resettare il timer

    temp_addr.sockfd = sockfd;
    temp_addr.dest_addr = dest_addr;
    addr = &temp_addr;//inizializzo puntatore globale necessario per signal_handler
    //inizializza handler
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(sigNo, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    s_timeout.sa_flags = SA_SIGINFO;
    s_timeout.sa_sigaction = server_timeout_handler;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    if (sigaction(SIGALRM, &s_timeout, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }

    memset(win_buf, 0, sizeof(struct window_snd_buf) * (2 * W));//inizializza a zero
    if (sendto(sockfd, &(temp_buf), MAXPKTSIZE, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) ==
        -1) {//invio temp_buf==invio pacchetto
        handle_error_with_exit("error in sendto\n");
    }
    alarm(15);
    timer_settime(win_buf[seq_to_send].time_id, 0, &sett_timer, NULL);//avvio timer
    while (connection_failed == 0) {
        if (recvfrom(sockfd, &temp_buf, sizeof(int), 0, (struct sockaddr *) &dest_addr, &len) != -1) {//flag bloccante,
            alarm(0);
            if (timer_settime(win_buf[seq_to_send].time_id, 0, &rst_timer, NULL) == -1) {
                handle_error_with_exit("error in timer_settime\n");
            }
            if (seq_is_in_window(window_base, window_base + W - 1, W, ack_numb)) {
                //se è dentro la finestra->ack con numero di sequenza dentro finestra ricevuto
                win_buf[ack_numb].acked = 1;//segna pkt come riscontrato
                if (ack_numb == window_base) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while (win_buf[window_base].acked == 1) {//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        win_buf[window_base].acked = 0;//resetta quando scorri finestra
                        window_base = (window_base + 1) % (2 * W);//avanza la finestra
                    }
                }
            }
            return temp_buf;
        }
    }
    temp_buf.seq_numb = -1;
    strcpy(temp_buf.payload,"Server unreachable");
    return temp_buf;
}*/
