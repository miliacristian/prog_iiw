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
#include "put_client.h"

int close_put_send_file(int sockfd, struct sockaddr_in serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W, double loss_prob, int *byte_readed,int *window_base_snd,int *pkt_fly,int*window_base_rcv,int *seq_to_send,int dimension) {//manda fin non in finestra senza sequenza e ack e chiudi
    //in questo stato ho ricevuto tutti gli ack (compreso l'ack della put),posso ricevere ack duplicati,FIN_ACK,start(fuori finestra)
    printf("function close_put_send_file\n");
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    stoppa_timer(win_buf_snd,W);//forza lo stop dei segnali pendenti
    send_message_in_window_cli(sockfd, &serv_addr, len, temp_buff,win_buf_snd, "FIN", FIN,seq_to_send, loss_prob,W,pkt_fly);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto close put send file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.command == FIN_ACK) {
                stop_all_timers(win_buf_snd, W);
                stop_timeout_timer(timeout_timer_id);
                printf("close put send file\n");
                return *byte_readed;//fine connesione
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        printf("errore close put ack file in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_file_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly,dimension, byte_readed);
                    }
                    else {
                        printf("\"errore close put ack_msg in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly);
                    }
                }
                else {
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("close put send_file ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd, &serv_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto close put send file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        } else if (errno != EINTR && errno!=0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto close_put_send_file\n");
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            return *byte_readed;
        }
    }
}

int send_put_file(int sockfd, struct sockaddr_in serv_addr, socklen_t len, int *seq_to_send, int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int fd, int *byte_readed, int dim, double loss_prob) {
    //in questo stato ho già ricevuto almeno una volta START,posso ricevere ack put,start e ack del file
    printf("send_file\n");
    int value = 0,*byte_sent = &value;
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    while (1) {
        if (*pkt_fly < W && (*byte_sent) < dim) {
            send_data_in_window_cli(sockfd, fd, &serv_addr, len, temp_buff, win_buf_snd, seq_to_send, loss_prob, W,pkt_fly, byte_sent, dim);
        }
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &serv_addr, &len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto send_put_file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {
                        rcv_ack_file_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly, dim, byte_readed);
                        if (*byte_readed == dim) {
                            close_put_send_file(sockfd, serv_addr, len, temp_buff, win_buf_snd, W, loss_prob,
                                                byte_readed, window_base_snd, pkt_fly, window_base_rcv, seq_to_send,dim);
                            printf("close sendfile\n");
                            return *byte_readed;
                        }
                    }
                    else{
                        rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                    }
                }else {
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("send_put_file ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd, &serv_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id, TIMEOUT);
            } else {
                printf("ignorato pacchetto send_put_file con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto send_put_file\n");
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            return *byte_readed;
        }
    }
}
int wait_for_put_start(int sockfd, struct sockaddr_in serv_addr, socklen_t  len,char*filename, int *byte_readed , int *seq_to_send , int *window_base_snd , int *window_base_rcv, int W, int *pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,int dimension){
    //in questo stato si può ricevere ack della put e messaggio di start/errore
    errno=0;
    int fd;
    char*path,dim_string[11];
    double loss_prob=param_client.loss_prob;
    strcpy(temp_buff.payload, "put ");
    sprintf(dim_string, "%d", dimension);
    strcpy(temp_buff.payload,dim_string);
    strcat(temp_buff.payload," ");
    strcat(temp_buff.payload, filename);
    //invia messaggio put
    send_message_in_window_cli(sockfd,&serv_addr,len,temp_buff,win_buf_snd,temp_buff.payload,PUT,seq_to_send,loss_prob,W,pkt_fly);//manda messaggio get
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
            printf("pacchetto ricevuto wait for put start con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.command == START) {
                printf("messaggio start ricevuto\n");
                rcv_msg_send_ack_command_in_window(sockfd,&serv_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                path=generate_full_pathname(filename,dir_client);
                fd=open(path,O_RDONLY);
                if(fd==-1){
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                send_put_file(sockfd,serv_addr,len,seq_to_send,window_base_snd,window_base_rcv,W,pkt_fly,temp_buff,win_buf_snd,fd,byte_readed,dimension,loss_prob);
                if(close(fd)==-1){
                    handle_error_with_exit("error in close file\n");
                }
                printf("return wait for put start\n");
                return *byte_readed;
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("wait for put ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }

            else {
                printf("ignorato pacchetto wait for put start con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",*window_base_snd,*window_base_rcv);
                handle_error_with_exit("");
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
            return *byte_readed;
        }
    }
}

