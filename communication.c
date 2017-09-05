#include <sys/time.h>
#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "timer.h"
#include "Server.h"
#include "Client.h"
#include "communication.h"
#include "dynamic_list.h"

void send_message(int sockfd, struct sockaddr_in *addr, socklen_t len, struct temp_buffer temp_buff, char *data,
                  char command, double loss_prob) {
    strcpy(temp_buff.payload, data);
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = NOT_A_PKT;
    temp_buff.command = command;
    //niente ack e sequenza
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) addr, len) ==
            -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf(CYAN"pacchetto inviato con ack %d seq %d command %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command);
    } else {
        printf(BLUE"pacchetto con ack %d, seq %d command %d perso\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command);
    }
    return;
}

void resend_message(int sockfd,struct temp_buffer*temp_buff,struct sockaddr_in *addr,socklen_t len, double loss_prob) {
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE,0, (struct sockaddr *) addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf(YELLOW"pacchetto ritrasmesso con ack %d seq %d command %d\n"RESET, temp_buff->ack, temp_buff->seq,temp_buff->command);
    }
    else{
        printf(YELLOW"pacchetto ritrasmesso con ack %d, seq %d command %d perso\n" RESET,temp_buff->ack,temp_buff->seq,temp_buff->command);
    }
    return;
}

void send_syn(int sockfd,struct sockaddr_in *serv_addr, socklen_t len, double loss_prob) {
    struct temp_buffer temp_buff;
    temp_buff.seq=NOT_AN_ACK;
    temp_buff.command=SYN;
    temp_buff.ack=NOT_AN_ACK;
    strcpy(temp_buff.payload,"SYN");
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd,&temp_buff,MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in syn sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto syn mandato\n");
    }
    else{
        printf("pacchetto syn perso\n");
    }
    return;
}
void send_syn_ack(int sockfd,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob) {
    struct temp_buffer temp_buff;
    temp_buff.seq=NOT_AN_ACK;
    temp_buff.command=SYN_ACK;
    temp_buff.ack=NOT_AN_ACK;
    strcpy(temp_buff.payload,"SYN_ACK");
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd,&temp_buff,MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto syn ack mandato\n");
    }
    else{
        printf("pacchetto syn ack perso\n");
    }
    return;
}

void send_list_in_window(int sockfd,char**list, struct sockaddr_in *serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int *seq_to_send, double loss_prob, int W, int *pkt_fly, int *byte_sent, int dim,struct shm_sel_repeat *shm) {
    temp_buff.command = DATA;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = *seq_to_send;
    win_buf_snd[*seq_to_send].acked = 0;
    if ((dim - (*byte_sent)) < (MAXPKTSIZE - OVERHEAD)) {//byte mancanti da inviare
        copy_buf1_in_buf2(temp_buff.payload,*list,MAXPKTSIZE-OVERHEAD);
        copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload,*list,MAXPKTSIZE-OVERHEAD);
        *byte_sent += (dim - (*byte_sent));
        *list+=dim - (*byte_sent);
    } else {
        copy_buf1_in_buf2(temp_buff.payload,*list,(MAXPKTSIZE - OVERHEAD));
        *byte_sent += (MAXPKTSIZE - OVERHEAD);
        copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload,*list,( MAXPKTSIZE - OVERHEAD));
        *list+=(MAXPKTSIZE-OVERHEAD);
    }
    win_buf_snd[*seq_to_send].command = DATA;
    lock_mtx(&(shm->mtx));
    (win_buf_snd[*seq_to_send].lap)+=1;
    temp_buff.lap=(win_buf_snd[*seq_to_send].lap);
    if(clock_gettime(CLOCK_MONOTONIC, &(win_buf_snd[*seq_to_send].time))!=0){
        handle_error_with_exit("error in get_time\n");
    }
    insert_ordered(*seq_to_send,win_buf_snd[*seq_to_send].lap,win_buf_snd[*seq_to_send].time,shm->param.timer_ms, &(shm->head), &(shm->tail));
    unlock_thread_on_a_condition(&(shm->list_not_empty));
    unlock_mtx(&(shm->mtx));
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) ==
            -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
    } else {
        printf("pacchetto con ack %d, seq %d command %d perso\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
    }
    *seq_to_send = ((*seq_to_send) + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}
void send_data_in_window(int sockfd, int fd, struct sockaddr_in *serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int *seq_to_send, double loss_prob, int W, int *pkt_fly, int *byte_sent, int dim, struct shm_sel_repeat *shm) {
    ssize_t readed=0;
    temp_buff.command = DATA;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = *seq_to_send;
    win_buf_snd[*seq_to_send].acked = 0;
    if ((dim - (*byte_sent)) < (MAXPKTSIZE - OVERHEAD)) {//byte mancanti da inviare
        readed=readn(fd, temp_buff.payload, (size_t) (dim - (*byte_sent)));
        if(readed<dim-(*byte_sent)){
            handle_error_with_exit("error in read 2\n");
        }
        *byte_sent += (dim - (*byte_sent));
    }
    else {
        readed=readn(fd, temp_buff.payload, (MAXPKTSIZE - OVERHEAD));
        if(readed<(MAXPKTSIZE-OVERHEAD)){
            handle_error_with_exit("error in read 3\n");
        }
        *byte_sent +=( MAXPKTSIZE - OVERHEAD);
    }
    copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload, temp_buff.payload,(MAXPKTSIZE - OVERHEAD));
    win_buf_snd[*seq_to_send].command = DATA;
    lock_mtx(&(shm->mtx));
    (win_buf_snd[*seq_to_send].lap)+=1;
    temp_buff.lap=(win_buf_snd[*seq_to_send].lap);
    if(clock_gettime(CLOCK_MONOTONIC, &(win_buf_snd[*seq_to_send].time))!=0){
        handle_error_with_exit("error in get_time\n");
    }
    insert_ordered(*seq_to_send,win_buf_snd[*seq_to_send].lap,win_buf_snd[*seq_to_send].time,shm->param.timer_ms, &(shm->head), &(shm->tail));
    unlock_thread_on_a_condition(&(shm->list_not_empty));
    unlock_mtx(&(shm->mtx));
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf(CYAN"pacchetto inviato con ack %d seq %d command %d lap %d\n" RESET, temp_buff.ack, temp_buff.seq, temp_buff.command,temp_buff.lap);
    } else {
        printf(BLUE"pacchetto con ack %d, seq %d command %d lap %d perso\n" RESET, temp_buff.ack, temp_buff.seq, temp_buff.command,temp_buff.lap);
    }
    *seq_to_send = ((*seq_to_send) + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}


void send_message_in_window(int sockfd, struct sockaddr_in *cli_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, char *message, char command, int *seq_to_send, double loss_prob, int W, int *pkt_fly, struct shm_sel_repeat *shm) {
    temp_buff.command = command;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.seq = *seq_to_send;
    strcpy(temp_buff.payload, message);
    //strcpy(win_buf_snd[*seq_to_send].payload, temp_buff.payload);
    copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload,temp_buff.payload,MAXPKTSIZE-OVERHEAD);
    win_buf_snd[*seq_to_send].command = command;
    win_buf_snd[*seq_to_send].acked = 0;
    lock_mtx(&(shm->mtx));
    if(clock_gettime(CLOCK_MONOTONIC, &(win_buf_snd[*seq_to_send].time))!=0){
        handle_error_with_exit("error in get_time\n");
    }
    (win_buf_snd[*seq_to_send].lap)+=1;
    temp_buff.lap=win_buf_snd[*seq_to_send].lap;
    insert_ordered(*seq_to_send,win_buf_snd[*seq_to_send].lap, win_buf_snd[*seq_to_send].time,shm->param.timer_ms, &(shm->head), &(shm->tail));
    unlock_thread_on_a_condition(&(shm->list_not_empty));
    unlock_mtx(&(shm->mtx));
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) cli_addr, len) ==
            -1) {
            handle_error_with_exit("error in sendto\n");
        }
        printf(CYAN "pacchetto inviato con ack %d seq %d command %d\n" RESET, temp_buff.ack, temp_buff.seq, temp_buff.command);
    } else {
        printf(BLUE"pacchetto con ack %d, seq %d command %d perso\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command);
    }
    *seq_to_send = ((*seq_to_send) + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}


void rcv_msg_re_send_ack_command_in_window(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob){
    //già memorizzato in finestra
    temp_buff.ack=temp_buff.seq;
    temp_buff.seq=NOT_A_PKT;
    strcpy(temp_buff.payload,"ACK");
    //lascia invariato il tipo di comando
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf(CYAN"pacchetto inviato con ack %d seq %d command %d\n"RESET, temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    else{
        printf(BLUE"pacchetto con ack %d, seq %d command %d perso\n"RESET,temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    return;
}

void rcv_list_send_ack_in_window(int sockfd,char**list, struct sockaddr_in *serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv, int *window_base_rcv, double loss_prob, int W, int dim, int *byte_written){
    //ricevi parte di lista e invia ack
    struct temp_buffer ack_buff;
    if(win_buf_rcv[temp_buff.seq].received ==0) {
        if ((win_buf_rcv[temp_buff.seq].lap) == (temp_buff.lap - 1)) {
            win_buf_rcv[temp_buff.seq].lap=temp_buff.lap;
            win_buf_rcv[temp_buff.seq].command = temp_buff.command;
            copy_buf1_in_buf2(win_buf_rcv[temp_buff.seq].payload, temp_buff.payload, (MAXPKTSIZE - OVERHEAD));
            win_buf_rcv[temp_buff.seq].received = 1;
        }
        else{
            //ignora pacchetto
        }
    }
    ack_buff.ack = temp_buff.seq;
    ack_buff.seq = NOT_A_PKT;
    strcpy(ack_buff.payload, "ACK");
    ack_buff.command = DATA;
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &ack_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) ==
            -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf(CYAN "pacchetto inviato con ack %d seq %d command %d\n" RESET, ack_buff.ack, ack_buff.seq, ack_buff.command);
    } else {
        printf(BLUE "pacchetto con ack %d, seq %d command %d perso\n" RESET, ack_buff.ack, ack_buff.seq, ack_buff.command);
    }
    if (temp_buff.seq == *window_base_rcv) {//se pacchetto riempie un buco
        // scorro la finestra fino al primo ancora non ricevuto
        while (win_buf_rcv[*window_base_rcv].received == 1) {
            if (dim - *byte_written >=(MAXPKTSIZE - OVERHEAD)) {
                copy_buf1_in_buf2(*list,temp_buff.payload,(MAXPKTSIZE - OVERHEAD));//scrivo in list la parte di lista
                *byte_written += (MAXPKTSIZE - OVERHEAD);
                *list+=(MAXPKTSIZE-OVERHEAD);
            } else {
                copy_buf1_in_buf2(*list,temp_buff.payload,dim - *byte_written);//scrivo in list la parte di lista
                //questa copy_buf potrebbe dar problemi
                *byte_written += dim - *byte_written;
                *list+=dim-*byte_written;
            }
            win_buf_rcv[*window_base_rcv].received = 0;//segna pacchetto come non ricevuto
            *window_base_rcv = ((*window_base_rcv) + 1) % (2 * W);//avanza la finestra con modulo di 2W
        }
    }
    return;
}

void rcv_data_send_ack_in_window(int sockfd, int fd, struct sockaddr_in *serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv, int *window_base_rcv, double loss_prob, int W, int dim, int *byte_written) {
    struct temp_buffer ack_buff;
    int written=0;
    if(win_buf_rcv[temp_buff.seq].received ==0) {
        if ((win_buf_rcv[temp_buff.seq].lap) == (temp_buff.lap - 1)) {
            win_buf_rcv[temp_buff.seq].lap=temp_buff.lap;
            win_buf_rcv[temp_buff.seq].command = temp_buff.command;
            copy_buf1_in_buf2(win_buf_rcv[temp_buff.seq].payload, temp_buff.payload, (MAXPKTSIZE - OVERHEAD));//è giusto copiarli tutti e eventualmente scriverne solo alcuni?
            win_buf_rcv[temp_buff.seq].received = 1;
        }
        else{
            //ignora pacchetto
        }
    }
    ack_buff.ack = temp_buff.seq;
    ack_buff.seq = NOT_A_PKT;
    strcpy(ack_buff.payload, "ACK");
    ack_buff.command = DATA;
    if (flip_coin(loss_prob)) {
        if (sendto(sockfd, &ack_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) ==
            -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf(CYAN"pacchetto inviato con ack %d seq %d command %d \n"RESET, ack_buff.ack, ack_buff.seq, ack_buff.command);
    }
    else {
        printf(BLUE"pacchetto con ack %d, seq %d command %d perso\n"RESET, ack_buff.ack, ack_buff.seq, ack_buff.command);
    }
    if (temp_buff.seq == *window_base_rcv) {//se pacchetto riempie un buco
        // scorro la finestra fino al primo ancora non ricevuto
        while (win_buf_rcv[*window_base_rcv].received == 1) {
                if (dim - *byte_written >=(MAXPKTSIZE - OVERHEAD)) {
                    written=(int)writen(fd, win_buf_rcv[*window_base_rcv].payload,(MAXPKTSIZE - OVERHEAD));
                    if(written<(MAXPKTSIZE-OVERHEAD)){
                        handle_error_with_exit("error in write\n");
                    }
                    *byte_written += (MAXPKTSIZE - OVERHEAD);
                }
                else {
                    written=(int)writen(fd, win_buf_rcv[*window_base_rcv].payload, (size_t) dim - *byte_written);
                    if(written<dim - *byte_written){
                        handle_error_with_exit("error in write\n");
                    }
                    *byte_written += dim - *byte_written;
                }
                printf("byte written %d\n",*byte_written);
                win_buf_rcv[*window_base_rcv].received = 0;//segna pacchetto come non ricevuto
                *window_base_rcv = ((*window_base_rcv) + 1) % (2 * W);//avanza la finestra con modulo di 2W
        }
    }
    return;
}

//chiamata dopo aver ricevuto un messaggio per riscontrarlo segnarlo in finestra ricezione
void rcv_msg_send_ack_command_in_window(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_rcv_buf *win_buf_rcv,int *window_base_rcv,double loss_prob,int W){
    struct temp_buffer ack_buff;
    if(win_buf_rcv[temp_buff.seq].received ==0) {
        if ((win_buf_rcv[temp_buff.seq].lap) == (temp_buff.lap - 1)) {
            win_buf_rcv[temp_buff.seq].lap=temp_buff.lap;
            win_buf_rcv[temp_buff.seq].command = temp_buff.command;
            strcpy(win_buf_rcv[temp_buff.seq].payload, temp_buff.payload);//meglio copybuf al posto di strcpy?
            win_buf_rcv[temp_buff.seq].received = 1;
        }
        else{
            //ignora pacchetto
        }
    }
    ack_buff.ack=temp_buff.seq;
    ack_buff.seq=NOT_A_PKT;
    strcpy(ack_buff.payload,"ACK");
    ack_buff.command=temp_buff.command;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &ack_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf(CYAN"pacchetto inviato con ack %d seq %d command %d\n"RESET, ack_buff.ack, ack_buff.seq,ack_buff.command);
    }
    else{
        printf(BLUE"pacchetto con ack %d, seq %d command %d perso\n"RESET,ack_buff.ack, ack_buff.seq,ack_buff.command);
    }
    if (temp_buff.seq == *window_base_rcv) {//se pacchetto riempie un buco
        // scorro la finestra fino al primo ancora non ricevuto
        while (win_buf_rcv[*window_base_rcv].received == 1) {
            win_buf_rcv[*window_base_rcv].received = 0;//segna pacchetto come non ricevuto
            *window_base_rcv = ((*window_base_rcv) + 1) % (2 * W);//avanza la finestra con modulo di 2W
        }
    }
    return;
}

void rcv_ack_in_window(struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W, int *window_base_snd,int *pkt_fly, struct shm_sel_repeat *shm) {
    lock_mtx(&(shm->mtx));
    win_buf_snd[temp_buff.ack].acked = 1;
    if (temp_buff.ack == *window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
        while (win_buf_snd[*window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
            //avanzo la finestra
            win_buf_snd[*window_base_snd].acked = 2;//resetta quando scorri finestra
            win_buf_snd[*window_base_snd].time.tv_nsec=0;
            win_buf_snd[*window_base_snd].time.tv_sec=0;
            *window_base_snd = ((*window_base_snd) + 1) % (2 * W);//avanza la finestra
            (*pkt_fly)--;
        }
    }
    unlock_mtx(&(shm->mtx));
}

void rcv_ack_file_in_window(struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W,
                                 int *window_base_snd, int *pkt_fly, int dim, int *byte_readed, struct shm_sel_repeat *shm) {
    //tempbuff.command deve essere uguale a data
    lock_mtx(&(shm->mtx));
    win_buf_snd[temp_buff.ack].acked = 1;
    if (temp_buff.ack == *window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
        while (win_buf_snd[*window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
            //avanzo la finestra
            win_buf_snd[*window_base_snd].acked = 2;//resetta quando scorri finestra
            win_buf_snd[*window_base_snd].time.tv_nsec=0;
            win_buf_snd[*window_base_snd].time.tv_sec=0;
            *window_base_snd = ((*window_base_snd) + 1) % (2 * W);//avanza la finestra
            (*pkt_fly)--;
            if (dim - *byte_readed >= (MAXPKTSIZE - OVERHEAD)) {
                *byte_readed += (MAXPKTSIZE - OVERHEAD);
                printf("byte readed %d\n",*byte_readed);
            }
            else {
                *byte_readed += dim - *byte_readed;
                printf("byte readed %d\n",*byte_readed);
            }
        }
    }
    unlock_mtx(&(shm->mtx));
    return;
}
void rcv_ack_list_in_window(struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W,
                            int *window_base_snd, int *pkt_fly, int dim, int *byte_readed, struct shm_sel_repeat *shm) {//ack di un messaggio contenente
    // parte di lista,tempbuff.command deve essere uguale a data
    lock_mtx(&(shm->mtx));
    win_buf_snd[temp_buff.ack].acked = 1;
    if (temp_buff.ack == *window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
        while (win_buf_snd[*window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
            //avanzo la finestra
            win_buf_snd[*window_base_snd].acked =2;//resetta quando scorri finestra
            win_buf_snd[*window_base_snd].time.tv_nsec=0;
            win_buf_snd[*window_base_snd].time.tv_sec=0;
            *window_base_snd = ((*window_base_snd) + 1) % (2 * W);//avanza la finestra
            (*pkt_fly)--;
            if (dim - *byte_readed >= (MAXPKTSIZE - OVERHEAD)) {
                *byte_readed +=( MAXPKTSIZE - OVERHEAD);
                printf("byte readed %d\n",*byte_readed);
            }
            else {
                *byte_readed += dim - *byte_readed;
                printf("byte readed %d\n",*byte_readed);
            }
        }
    }
    unlock_mtx(&(shm->mtx));
    return;

}