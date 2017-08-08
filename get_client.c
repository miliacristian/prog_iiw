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
#include "communication.h"

void send_message_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,char*message,char command,int *seq_to_send,double loss_prob,int W,int *pkt_fly){
    temp_buff.command=command;
    temp_buff.ack=NOT_AN_ACK;
    temp_buff.seq=*seq_to_send;
    strcpy(temp_buff.payload,message);
    strcpy(win_buf_snd[*seq_to_send].payload,temp_buff.payload);
    win_buf_snd[*seq_to_send].command=command;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
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
/*void send_data_in_window_cli(int sockfd,int fd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,int *seq_to_send,double loss_prob,int W,int *pkt_fly,int *byte_sent,int dim){
    ssize_t readed=0;
    temp_buff.command=DATA;
    temp_buff.ack=NOT_AN_ACK;
    temp_buff.seq=*seq_to_send;
    if((dim-(*byte_sent))<(MAXPKTSIZE-9)){
        readed=readn(fd,temp_buff.payload,(size_t)(dim-(*byte_sent)));
        if(readed<dim-(*byte_sent)){
            handle_error_with_exit("error in read\n");
        }
        *byte_sent+=(dim-(*byte_sent));
        copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload,temp_buff.payload,(dim-(*byte_sent)));
    }
    else {
        readed=readn(fd, temp_buff.payload, (MAXPKTSIZE - 9));
        if(readed<MAXPKTSIZE){
            handle_error_with_exit("error in read\n");
        }
        *byte_sent+=MAXPKTSIZE-9;
        copy_buf1_in_buf2(win_buf_snd[*seq_to_send].payload,temp_buff.payload,MAXPKTSIZE-9);
    }
    win_buf_snd[*seq_to_send].command=DATA;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
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
}*/

int close_connection(struct temp_buffer temp_buff,int *seq_to_send,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("close connection\n");
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"FIN",FIN,seq_to_send,loss_prob,W,pkt_fly);//manda messaggio di fin
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo fin_ack dal server
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                //ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (temp_buff.command==FIN_ACK) {
                stop_timeout_timer(timeout_timer_id);
                stop_all_timers(win_buf_snd, W);
                printf("return close connection 1\n");
                return *byte_written;
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato close connect pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return close connection 2\n");
            return *byte_written;
        }
    }
}

int  wait_for_fin(struct temp_buffer temp_buff,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("wait for fin\n");
    start_timeout_timer(timeout_timer_id,TIMEOUT);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                //ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
            if (temp_buff.command==FIN) {
                stop_timeout_timer(timeout_timer_id);
                stop_all_timers(win_buf_snd, W);
                printf("return wait_for_fin 1\n");
                return *byte_written;
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato close connect pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return wait_for_fin 2\n");
            return *byte_written;
        }
    }

}

int rcv_file(int sockfd,struct sockaddr_in serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,int fd,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written){
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"START",START,seq_to_send,loss_prob,W,pkt_fly);
    printf("messaggio start inviato\n");
    printf("rcv file\n");
    errno=0;
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                //ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if(seq_is_in_window(*window_base_rcv, W,temp_buff.seq)){
                rcv_data_send_ack_in_window(sockfd,fd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W,dimension,byte_written);
                if(*byte_written==dimension){
                    wait_for_fin(temp_buff,win_buf_snd,sockfd,serv_addr,len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                    printf("return rcv file 1\n");
                    return *byte_written;
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto wait dimension con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return rcv file 2\n");
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
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo risposta del server
            //mi blocco sulla risposta del server
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                //ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("ack duplicato non fare nulla\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (temp_buff.command == ERROR) {
                rcv_msg_send_ack_in_window(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                close_connection(temp_buff,seq_to_send,win_buf_snd,sockfd,serv_addr, len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                printf("return wait for dimension 1\n");
                return *byte_written;
            }
            else if (temp_buff.command == DIMENSION) {
                rcv_msg_send_ack_in_window(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                printf("dimensione %s\n",temp_buff.payload);//stampa dimensione
                path=generate_multi_copy(dir_client,filename);
                if(path==NULL){
                    handle_error_with_exit("error:there are too much copies of the file");
                }
                fd=open(path,O_WRONLY | O_CREAT,0666);
                if(fd==-1){
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                dimension=parse_integer(temp_buff.payload);
                printf("dimensione intera %d\n",dimension);
                rcv_file(sockfd,serv_addr,len,temp_buff,win_buf_snd,win_buf_rcv,seq_to_send,W,pkt_fly,fd,dimension,loss_prob,window_base_snd,window_base_rcv,byte_written);
                if(close(fd)==-1){
                    handle_error_with_exit("error in close file\n");
                }
                printf("return wait for dimension 2\n");
                return *byte_written;
            }
            else {
                printf("ignorato pacchetto wait dimension con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return wait for dimension 3\n");
            return *byte_written;
        }
    }
}