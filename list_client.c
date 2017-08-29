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


int close_connection_list(struct temp_buffer temp_buff,int *seq_to_send,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("close connection\n");
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"FIN",FIN,seq_to_send,loss_prob,W,pkt_fly);//manda messaggio di fin
    start_timeout_timer(timeout_timer_id_client,TIMEOUT);
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo fin_ack dal server
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto close connection con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly);
                }
                else{
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if (temp_buff.command==FIN_ACK) {
                stop_timeout_timer(timeout_timer_id_client);
                stop_all_timers(win_buf_snd, W);
                printf("return close connection\n");
                return *byte_written;
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else {
                printf("ignorato close connection pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id_client);
            printf("return close connection\n");
            return *byte_written;
        }
    }
}

int  wait_for_fin_list(struct temp_buffer temp_buff,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    printf("wait for fin list\n");
    start_timeout_timer(timeout_timer_id_client,TIMEOUT);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto wait for fin_list con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
            if (temp_buff.command==FIN) {
                stop_timeout_timer(timeout_timer_id_client);
                stop_all_timers(win_buf_snd, W);
                printf("fin ricevuto\n");
                return *byte_written;
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("wait for fin ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d payload %s\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command,temp_buff.payload);
                printf("winbase snd %d winbase rcv %d\n",*window_base_snd,*window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client==1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id_client);
            printf("return wait_for_fin\n");
            return *byte_written;
        }
    }
}


/*int rcv_list(int sockfd,struct sockaddr_in serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,char*list,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written){
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"START",START,seq_to_send,loss_prob,W,pkt_fly);
    printf("messaggio start inviato\n");
    printf("rcv list\n");
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
                rcv_msg_re_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if(seq_is_in_window(*window_base_rcv, W,temp_buff.seq)){
                rcv_list_send_ack_in_window(sockfd,&list,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W,dimension,byte_written);
                if(*byte_written==dimension){
                    wait_for_fin_list(temp_buff,win_buf_snd,sockfd,serv_addr,len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                    return *byte_written;
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv list con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
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
            printf("return rcv list\n");
            return *byte_written;
        }
    }
}*/
int rcv_list2(int sockfd,struct sockaddr_in serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,char**list,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written){
    start_timeout_timer(timeout_timer_id_client,TIMEOUT);
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,"START",START,seq_to_send,loss_prob,W,pkt_fly);
    printf("messaggio start inviato\n");
    printf("rcv list\n");
    errno=0;
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto rcv_list2 con ack %d seq %d command %d payload %s\n", temp_buff.ack, temp_buff.seq, temp_buff.command, temp_buff.payload);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("rcv_list2 ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if(seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                if (temp_buff.command == DATA) {
                    rcv_list_send_ack_in_window(sockfd, list, &serv_addr, len, temp_buff, win_buf_rcv, window_base_rcv,loss_prob, W, dimension, byte_written);
                    if (*byte_written==dimension) {
                        wait_for_fin_list(temp_buff, win_buf_snd, sockfd, serv_addr, len, window_base_snd,
                                          window_base_rcv, pkt_fly, W, byte_written, loss_prob);
                        return *byte_written;
                    }
                }
                else{
                    printf("errore rcv_list2\n");
                    printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                    handle_error_with_exit("");
                    rcv_msg_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                }
                start_timeout_timer(timeout_timer_id_client, TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv list2 con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",*window_base_snd,*window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id_client);
            printf("return rcv list\n");
            return *byte_written;
        }
    }
}

int wait_for_list_dimension(int sockfd, struct sockaddr_in serv_addr, socklen_t  len, int *byte_written , int *seq_to_send , int *window_base_snd , int *window_base_rcv, int W, int *pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd) {
    errno = 0;
    int dimension=0;
    double loss_prob = param_client.loss_prob;
    char*list,*first;
    strcpy(temp_buff.payload, "list");
    send_message_in_window_cli(sockfd, &serv_addr, len, temp_buff, win_buf_snd, temp_buff.payload,LIST, seq_to_send,loss_prob, W, pkt_fly);//manda messaggio get
    start_timeout_timer(timeout_timer_id_client, TIMEOUT);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) !=
            -1) {//attendo risposta del server
            //mi blocco sulla risposta del server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto wait for list_dim con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly);
                } else {
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("ack duplicato non fare nulla\n");
                }
                start_timeout_timer(timeout_timer_id_client, TIMEOUT);
            } else if (temp_buff.command == ERROR) {
                rcv_msg_send_ack_command_in_window(sockfd, &serv_addr, len, temp_buff, win_buf_rcv, window_base_rcv, loss_prob,
                                           W);
                close_connection_list(temp_buff, seq_to_send, win_buf_snd, sockfd, serv_addr, len, window_base_snd,
                                 window_base_rcv, pkt_fly, W, byte_written, loss_prob);
                printf("lista vuota\n");
                return *byte_written;
            } else if (temp_buff.command == DIMENSION) {
                rcv_msg_send_ack_command_in_window(sockfd, &serv_addr, len, temp_buff, win_buf_rcv, window_base_rcv, loss_prob,
                                           W);
                dimension = parse_integer(temp_buff.payload);
                printf("dimensione ricevuta %d\n",dimension);
                list=malloc(sizeof(char)*dimension);
                if(list==NULL){
                  handle_error_with_exit("error in malloc\n");
                }
                memset(list,'\0',dimension);
                first=list;
                rcv_list2(sockfd, serv_addr, len, temp_buff, win_buf_snd, win_buf_rcv, seq_to_send, W, pkt_fly,&list,
                         dimension, loss_prob, window_base_snd, window_base_rcv, byte_written);
                printf("dimension %d\n",dimension);
                printf("byte_written %d\n",*byte_written);
                printf("%d\n",(int)strlen(first));
                if(*byte_written==dimension){
                    printf("list:\n%s",first);//stampa della lista ottenuta
                }
                else{
                    printf("errore,lista non correttamente ricevuta\n");
                }
                free(first);//il puntatore di list è stato spostato per inviare la lista
                list=NULL;
                return *byte_written;
            } else {
                printf("ignorato pacchetto wait list dim con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_client, TIMEOUT);
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id_client);
            return *byte_written;
        }
    }
}