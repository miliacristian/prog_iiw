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
#include "put_server.h"
int  wait_for_fin_put(struct temp_buffer temp_buff,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in cli_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob,int *seq_to_send){
    printf("wait for fin\n");
    start_timeout_timer(timeout_timer_id,TIMEOUT-3000);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto wait for fin con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
            if (temp_buff.command==FIN){
                stop_timeout_timer(timeout_timer_id);
                stop_all_timers(win_buf_snd, W);
                send_message(sockfd,&cli_addr,len,temp_buff,"FIN_ACK",FIN_ACK,loss_prob);
                printf("return wait_for_fin\n");
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
                rcv_msg_re_send_ack_command_in_window(sockfd,&cli_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",*window_base_snd,*window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR && errno!=0){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            return *byte_written;
        }
    }
}
int rcv_put_file(int sockfd,struct sockaddr_in cli_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,int fd,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written){
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    send_message_in_window_serv(sockfd,&cli_addr,len,temp_buff,win_buf_snd,"START",START,seq_to_send,loss_prob,W,pkt_fly);
    printf("messaggio start inviato\n");
    errno=0;
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) != -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
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
                rcv_msg_re_send_ack_command_in_window(sockfd,&cli_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if(seq_is_in_window(*window_base_rcv, W,temp_buff.seq)){
                if(temp_buff.command==DATA){
                    rcv_data_send_ack_in_window(sockfd,fd,&cli_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W,dimension,byte_written);
                    if(*byte_written==dimension){
                        wait_for_fin_put(temp_buff,win_buf_snd,sockfd,cli_addr,len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob,seq_to_send);
                        printf("return rcv file\n");
                        return *byte_written;
                    }
                }
                else{
                    printf("errore rcv put file\n");
                    rcv_msg_send_ack_command_in_window(sockfd,&cli_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        else if(errno!=EINTR && errno!=0){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            return *byte_written;
        }
    }
}

//ricevuto pacchetto con put dimensione e filename
int execute_put(int sockfd,int *seq_to_send,struct temp_buffer temp_buff,int *window_base_rcv,int *window_base_snd,int *pkt_fly,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,struct sockaddr_in cli_addr,socklen_t len,int W){
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    int byte_written= 0, fd,dimension;
    double loss_prob = param_serv.loss_prob;
    char*path,*first,*payload;
    payload=malloc(sizeof(char)*(MAXPKTSIZE-9));
    if(payload==NULL){
        handle_error_with_exit("error in payload\n");
    }
    strcpy(payload,temp_buff.payload);
    first=payload;
    dimension=parse_integer_and_move(&payload);
    printf("dimensione del file put %d\n",dimension);
    payload++;
    path=generate_multi_copy(dir_server,payload);
    fd = open(path, O_WRONLY | O_CREAT,0666);
    if (fd == -1) {
        handle_error_with_exit("error in open\n");
    }
    free(path);
    free(first);
    payload=NULL;
    rcv_msg_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, win_buf_rcv, window_base_rcv, loss_prob, W);//invio ack della put
    rcv_put_file(sockfd,cli_addr,len,temp_buff,win_buf_snd,win_buf_rcv,seq_to_send,W,pkt_fly,fd,dimension,loss_prob,window_base_snd,window_base_rcv,&byte_written);
    if(close(fd)==-1){
        handle_error_with_exit("error in close file\n");
    }
    printf("return execute put\n");
    return byte_written;
}


