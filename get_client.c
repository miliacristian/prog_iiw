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


int close_connection_get(struct temp_buffer temp_buff,int *seq_to_send,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("close connection\n");
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"FIN",FIN,seq_to_send,loss_prob,W,pkt_fly);//manda messaggio di fin
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo fin_ack dal server
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto close_conn get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("close connect get ack duplicato\n");
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
                rcv_msg_re_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
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
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return close connection 2\n");
            return *byte_written;
        }
    }
}

int  wait_for_fin_get(struct temp_buffer temp_buff,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("wait for fin\n");
    start_timeout_timer(timeout_timer_id,TIMEOUT);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto wait for fin_get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
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
                    printf("wait for fin get ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return wait_for_fin 2\n");
            return *byte_written;
        }
    }
}

int rcv_get_file(int sockfd,struct sockaddr_in serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,int fd,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written){
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"START",START,seq_to_send,loss_prob,W,pkt_fly);
    printf("messaggio start inviato\n");
    printf("rcv file\n");
    errno=0;
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto rcv_get_file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("rcv_get_file ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if(seq_is_in_window(*window_base_rcv, W,temp_buff.seq)){
                rcv_data_send_ack_in_window(sockfd,fd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W,dimension,byte_written);
                if(*byte_written==dimension){
                    wait_for_fin_get(temp_buff,win_buf_snd,sockfd,serv_addr,len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                    printf("return rcv file 1\n");
                    return *byte_written;
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv get file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR && errno!=0){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return rcv file 2\n");
            return *byte_written;
        }
    }
}

int wait_for_get_dimension(int sockfd, struct sockaddr_in serv_addr, socklen_t  len, char *filename, int *byte_written , int *seq_to_send , int *window_base_snd , int *window_base_rcv, int W, int *pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd) {
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
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto wait_get_dim con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    printf("wait get_dim ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (temp_buff.command == ERROR) {
                rcv_msg_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                close_connection_get(temp_buff,seq_to_send,win_buf_snd,sockfd,serv_addr, len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                printf("return wait for dimension 1\n");
                return *byte_written;
            }
            else if (temp_buff.command == DIMENSION) {
                rcv_msg_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
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
                rcv_get_file(sockfd,serv_addr,len,temp_buff,win_buf_snd,win_buf_rcv,seq_to_send,W,pkt_fly,fd,dimension,loss_prob,window_base_snd,window_base_rcv,byte_written);
                if(close(fd)==-1){
                    handle_error_with_exit("error in close file\n");
                }
                printf("return wait for dimension 2\n");
                return *byte_written;
            }
            else {
                printf("ignorato pacchetto wait get dimension con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            printf("return wait for dimension 3\n");
            return *byte_written;
        }
    }
}