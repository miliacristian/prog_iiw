#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include <time.h>
#include "timer.h"
#include "Client.h"
#include "get_client.h"

//chiamata per riscontrare un ack perso
void rcv_msg_re_send_ack_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob,int W){
    //già memorizzato in finestra
    temp_buff.ack=temp_buff.seq;
    temp_buff.seq=NOT_A_PKT;
    strcpy(temp_buff.payload,"ACK");
    temp_buff.command=DATA;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    return;
}

//chiamata dopo aver ricevuto un messaggio per riscontrarlo segnarlo in finestra ricezione
void rcv_msg_send_ack_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_rcv_buf *win_buf_rcv,int *window_base_rcv,double loss_prob,int W){
    struct temp_buffer ack_buff;
    win_buf_rcv[temp_buff.seq].command=temp_buff.command;
    strcpy(win_buf_rcv[temp_buff.seq].payload,temp_buff.payload);
    win_buf_rcv[temp_buff.seq].received=1;
    ack_buff.ack=temp_buff.seq;
    ack_buff.seq=NOT_A_PKT;
    strcpy(ack_buff.payload,"ACK");
    ack_buff.command=DATA;
    if (temp_buff.seq == *window_base_rcv) {//se pacchetto riempie un buco
        // scorro la finestra fino al primo ancora non ricevuto
        while (win_buf_rcv[*window_base_rcv].received == 1) {
            win_buf_rcv[*window_base_rcv].received = 0;//segna pacchetto come non ricevuto
            *window_base_rcv = ((*window_base_rcv) + 1) % (2 * W);//avanza la finestra con modulo di 2W
        }
    }
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &ack_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d\n", ack_buff.ack, ack_buff.seq,ack_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",ack_buff.ack, ack_buff.seq,ack_buff.command);
    }
    return;
}
void rcv_ack_in_window_cli(struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,int W,int *window_base_snd,int *pkt_fly){
    stop_timer(win_buf_snd[temp_buff.ack].time_id);
    win_buf_snd[temp_buff.ack].acked=1;
    if (temp_buff.ack == *window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
        while (win_buf_snd[*window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
            //avanzo la finestra
            win_buf_snd[*window_base_snd].acked = 0;//resetta quando scorri finestra
            *window_base_snd = ((*window_base_snd) + 1) % (2 * W);//avanza la finestra
            (*pkt_fly)--;
        }
    }

}

void send_message_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,char*message,char command,int *seq_to_send,double loss_prob,int W,int *pkt_fly){
    temp_buff.command=command;
    temp_buff.ack=NOT_AN_ACK;
    temp_buff.seq=*seq_to_send;
    strcpy(temp_buff.payload,message);
    strcpy(win_buf_snd[*seq_to_send].payload,temp_buff.payload);
    win_buf_snd[*seq_to_send].command=command;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d \n", temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    start_timer(win_buf_snd[*seq_to_send].time_id,&sett_timer);
    *seq_to_send = ((*seq_to_send) + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}
void send_data_in_window_cli(int sockfd,int fd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,int *seq_to_send,double loss_prob,int W,int *pkt_fly,int *byte_sent,int dim){
    temp_buff.command=DATA;
    temp_buff.ack=NOT_AN_ACK;
    temp_buff.seq=*seq_to_send;
    if((dim-(*byte_sent))<(MAXPKTSIZE-9)){
        readn(fd,temp_buff.payload,(size_t)(dim-(*byte_sent)));
        *byte_sent+=(dim-(*byte_sent));
        copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload,temp_buff.payload,(dim-(*byte_sent)));
    }
    else {
        readn(fd, temp_buff.payload, (MAXPKTSIZE - 9));
        *byte_sent+=MAXPKTSIZE-9;
        copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload,temp_buff.payload,MAXPKTSIZE-9);
    }
    win_buf_snd[*seq_to_send].command=DATA;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d \n", temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    start_timer(win_buf_snd[*seq_to_send].time_id,&sett_timer);
    *seq_to_send = ((*seq_to_send) + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}
void rcv_data_send_ack_in_window_cli(int sockfd,int fd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_rcv_buf *win_buf_rcv,int *window_base_rcv,double loss_prob,int W,int dim,int *byte_written){
    struct temp_buffer ack_buff;
    win_buf_rcv[temp_buff.seq].command=temp_buff.command;
    copy_buf1_in_buf2(win_buf_rcv[temp_buff.seq].payload,temp_buff.payload,MAXPKTSIZE-9);//memorizzzo tutti i dati
    // alcuni saranno scartati sull'ultimo pacchetto
    win_buf_rcv[temp_buff.seq].received=1;
    ack_buff.ack=temp_buff.seq;
    ack_buff.seq=NOT_A_PKT;
    strcpy(ack_buff.payload,"ACK");
    ack_buff.command=DATA;
    if (temp_buff.seq == *window_base_rcv) {//se pacchetto riempie un buco
        // scorro la finestra fino al primo ancora non ricevuto
        while (win_buf_rcv[*window_base_rcv].received == 1) {
            if(win_buf_rcv[*window_base_rcv].command==DATA) {
                if(dim-*byte_written>=MAXPKTSIZE-9){//byte rimanenti
                    writen(fd,win_buf_rcv[*window_base_rcv].payload,MAXPKTSIZE-9);
                    *byte_written +=MAXPKTSIZE-9;//aggiorna byte scritti
                }
                else {
                    writen(fd, win_buf_rcv[*window_base_rcv].payload,(size_t)dim-*byte_written);
                    *byte_written +=dim-*byte_written;//aggiorna byte scritti
                }
                win_buf_rcv[*window_base_rcv].received = 0;//segna pacchetto come non ricevuto
                *window_base_rcv = ((*window_base_rcv) + 1) % (2 * W);//avanza la finestra con modulo di 2W
            }
        }
    }
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &ack_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d \n", ack_buff.ack, ack_buff.seq,ack_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",ack_buff.ack, ack_buff.seq,ack_buff.command);
    }
    return;
}

void send_fin_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob){
    temp_buff.command=FIN;
    strcpy(temp_buff.payload,"FIN");
    //senza ack e senza sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",temp_buff.ack,temp_buff.seq, temp_buff.command);
    }
    return;

}

void send_fin_ack_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob){
    temp_buff.command=FIN_ACK;
    strcpy(temp_buff.payload,"FIN_ACK");
    //senza ack e senza sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d\n",temp_buff.ack,temp_buff.seq, temp_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",temp_buff.ack,temp_buff.seq, temp_buff.command);
    }
    return;
}
void send_message_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,char*data,char command,double loss_prob){
    strcpy(temp_buff.payload,data);
    temp_buff.command=command;
    //niente ack e sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d\n",temp_buff.ack,temp_buff.seq, temp_buff.command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",temp_buff.ack,temp_buff.seq, temp_buff.command);
    }
    return;
}

int close_connection(struct temp_buffer temp_buff,int *seq_to_send,char*filename,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("close connection\n");
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"FIN",FIN,seq_to_send,loss_prob,W,pkt_fly);//manda messaggio di fin
    start_timeout_timer(timeout_timer_id, 5000);
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo fin_ack dal server
            stop_timer(timeout_timer_id);
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window_cli(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,5000);
            }
            else if (temp_buff.command==FIN_ACK) {
                stop_timer(timeout_timer_id);
                stop_all_timers(win_buf_snd, W);
                return *byte_written;
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window_cli(sockfd,&serv_addr,len,temp_buff,loss_prob,W);
                start_timeout_timer(timeout_timer_id, 5000);
            }
            else {
                printf("ignorato close connect pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                start_timeout_timer(timeout_timer_id,5000);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        else if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timer(timeout_timer_id);
            return *byte_written;
        }
    }
}

int  wait_for_fin(struct temp_buffer temp_buff,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("wait for fin\n");
    start_timeout_timer(timeout_timer_id, 5000);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            stop_timer(timeout_timer_id);
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window_cli(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,5000);
            }
            else if (temp_buff.command==FIN) {
                stop_timer(timeout_timer_id);
                stop_all_timers(win_buf_snd, W);
                return *byte_written;
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window_cli(sockfd,&serv_addr,len,temp_buff,loss_prob,W);
                start_timeout_timer(timeout_timer_id, 5000);

            }
            else {
                printf("ignorato close connect pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,5000);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        else if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timer(timeout_timer_id);
            return *byte_written;
        }
    }

}
int rcv_file(int sockfd,struct sockaddr_in serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,int fd,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written){
    start_timeout_timer(timeout_timer_id,5000);
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"START",START,seq_to_send,loss_prob,W,pkt_fly);
    printf("messaggio start inviato\n");
    printf("rcv file\n");
    errno=0;
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            stop_timer(timeout_timer_id);
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window_cli(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,5000);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window_cli(sockfd,&serv_addr,len,temp_buff,loss_prob,W);
                start_timeout_timer(timeout_timer_id, 5000);
            }
            else if(seq_is_in_window(*window_base_rcv, W,temp_buff.seq)){
                rcv_data_send_ack_in_window_cli(sockfd,fd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W,dimension,byte_written);
                if(*byte_written==dimension){
                    wait_for_fin(temp_buff,win_buf_snd,sockfd,serv_addr,len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                    return *byte_written;
                }
                start_timeout_timer(timeout_timer_id, 5000);
            }
            else {
                printf("ignorato pacchetto wait dimension con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,5000);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        else if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timer(timeout_timer_id);
            return *byte_written;
        }
    }

}
int wait_for_dimension(int sockfd, struct sockaddr_in serv_addr, socklen_t  len, char *filename, int *byte_written , int *seq_to_send , int *window_base_snd , int *window_base_rcv, int W, int *pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd) {
    errno=0;
    int fd,dimension;
    char*path;
    double loss_prob=param_client.loss_prob;
    strcpy(temp_buff.payload, "get ");
    strcat(temp_buff.payload, filename);
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,temp_buff.payload,GET,seq_to_send,loss_prob,W,pkt_fly);//manda messaggio get
    start_timeout_timer(timeout_timer_id, 5000);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo risposta del server
            //mi blocco sulla risposta del server
            stop_timer(timeout_timer_id);
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window_cli(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato non fare nulla\n");
                }
                start_timeout_timer(timeout_timer_id,5000);
            }
            else if (temp_buff.command == ERROR) {
                rcv_msg_send_ack_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                close_connection(temp_buff,seq_to_send,filename,win_buf_snd,sockfd,serv_addr, len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                return *byte_written;
            }
            else if (temp_buff.command == DIMENSION) {
                rcv_msg_send_ack_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                printf("dimensione %s\n",temp_buff.payload);//stampa dimensione
                path=generate_full_pathname(filename,dir_client);
                printf("full pathname %s\n",path);
                fd=open(path,O_WRONLY | O_CREAT,0666);
                if(fd==-1){
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                dimension=parse_integer(temp_buff.payload);
                printf("dimensione intera %d\n",dimension);
                rcv_file(sockfd,serv_addr,len,temp_buff,win_buf_snd,win_buf_rcv,seq_to_send,W,pkt_fly,fd,dimension,loss_prob,window_base_snd,window_base_rcv,byte_written);
                return *byte_written;
            }
            else {
                printf("ignorato pacchetto wait dimension con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,5000);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        else if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timer(timeout_timer_id);
            return *byte_written;
        }
    }
}

/*void send_command_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_snd_buf *win_buf_snd,int *seq_to_send,char command,char*command_name,double loss_prob,int W,int *pkt_fly){
    strcpy(temp_buff->payload,command_name);
    temp_buff->command=command;
    temp_buff->ack=NOT_AN_ACK;
    temp_buff->seq=*seq_to_send;
    strcpy(win_buf_snd[*seq_to_send].payload,temp_buff->payload);
    win_buf_snd[*seq_to_send].command=command;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    start_timer(win_buf_snd[*seq_to_send].time_id,&sett_timer);
    *seq_to_send = (*seq_to_send + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}

void send_fin_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_snd_buf *win_buf_snd,int *seq_to_send,double loss_prob,int W,int *pkt_fly){
    strcpy(temp_buff->payload,"FIN");
    temp_buff->command=FIN;
    temp_buff->ack=NOT_AN_ACK;
    temp_buff->seq=*seq_to_send;
    strcpy(win_buf_snd[*seq_to_send].payload,temp_buff->payload);
    win_buf_snd[*seq_to_send].command=FIN;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    start_timer(win_buf_snd[*seq_to_send].time_id,&sett_timer);
    *seq_to_send = (*seq_to_send + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}
void send_fin_ack_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_snd_buf *win_buf_snd,int *seq_to_send,double loss_prob,int W,int *pkt_fly){
    strcpy(temp_buff->payload,"FIN_ACK");
    temp_buff->command=FIN_ACK;
    temp_buff->ack=NOT_AN_ACK;
    temp_buff->seq=*seq_to_send;
    strcpy(win_buf_snd[*seq_to_send].payload,temp_buff->payload);
    win_buf_snd[*seq_to_send].command=FIN_ACK;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    start_timer(win_buf_snd[*seq_to_send].time_id,&sett_timer);
    *seq_to_send = (*seq_to_send + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}*/



