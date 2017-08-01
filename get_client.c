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

int close_connection(struct temp_buffer temp_buff,int seq_to_send,char*filename,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int window_base_snd,int window_base_rcv,int pkt_fly,int W,int byte_rcv){
    temp_buff.ack=NOT_AN_ACK;
    temp_buff.seq=seq_to_send;
    temp_buff.command=FIN;
    strcpy(temp_buff.payload,"FIN");
    strcpy(win_buf_snd[seq_to_send].payload, temp_buff.payload);
    win_buf_snd[seq_to_send].command=temp_buff.command;
    send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr), param_client.loss_prob);//mando il fin
    start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
    seq_to_send = (seq_to_send + 1) % (2 * W);
    //start_timeout_timer(timeout_timer_id, 5000);
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//risposta del server
            stop_timer(timeout_timer_id);
            start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
            printf("pacchetto ricevuto con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command, temp_buff.payload);

            if (temp_buff.seq == NOT_A_PKT) {
                start_timeout_timer(timeout_timer_id, 5000);

                //stop_timer(timeout_timer_id);
                stop_timer(win_buf_snd[temp_buff.ack].time_id);
                win_buf_snd[temp_buff.ack].acked = 1;
                if (temp_buff.ack == window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while (win_buf_snd[window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        win_buf_snd[window_base_snd].acked = 0;//resetta quando scorri finestra
                        window_base_snd = (window_base_snd + 1) % (2 * W);//avanza la finestra
                        pkt_fly--;
                    }
                }
            }
            else if (temp_buff.command==FIN_ACK) {
                stop_timer(timeout_timer_id);
                stop_all_timers(win_buf_snd, W);
                return byte_rcv;
            }
            else if (!seq_is_in_window(window_base_rcv, window_base_rcv+ W - 1, W,temp_buff.seq)) {
                //start_timeout_timer(timeout_timer_id, 5000);
                temp_buff.command=DATA;
                temp_buff.ack = temp_buff.seq;
                temp_buff.seq = NOT_A_PKT;
                send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr), param_client.loss_prob);
            }
            else {
                printf("ignorato pacchetto con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command, temp_buff.payload);
            }
        }
        else if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timer(timeout_timer_id);
            return byte_rcv;
        }
    }
}

int wait_for_dimension(int sockfd, struct sockaddr_in serv_addr, socklen_t  len, char *filename, int byte_written , int seq_to_send , int window_base_snd , int window_base_rcv, int W, int pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd) {
    int byte_rcv=0;
    strcpy(temp_buff.payload, "get ");
    strcat(temp_buff.payload, filename);
    temp_buff.seq = seq_to_send;
    temp_buff.ack = NOT_AN_ACK;
    temp_buff.command = COMMAND;
    strcpy(win_buf_snd[seq_to_send].payload, temp_buff.payload);//memorizzo pacchetto in finestra
    win_buf_snd[seq_to_send].command=temp_buff.command;
    send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr),param_client.loss_prob); //invio pacchetto con probabilità loss_prob
    strcpy(win_buf_snd[seq_to_send].payload,temp_buff.payload);
    win_buf_snd[seq_to_send].command=temp_buff.command;
    start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
    seq_to_send = (seq_to_send + 1) % (2 * W);
    start_timeout_timer(timeout_timer_id, 5000);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//risposta del server
            stop_timer(timeout_timer_id);
            start_timeout_timer(timeout_timer_id, 5000);
            printf("pacchetto ricevuto con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.command, temp_buff.payload);
            if (temp_buff.seq == NOT_A_PKT) {
                stop_timer(win_buf_snd[temp_buff.ack].time_id);
                win_buf_snd[temp_buff.ack].acked = 1;
                if (temp_buff.ack == window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while (win_buf_snd[window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        win_buf_snd[window_base_snd].acked = 0;//resetta quando scorri finestra
                        window_base_snd = (window_base_snd + 1) % (2 * W);//avanza la finestra
                        pkt_fly--;
                    }
                }
            } else if (temp_buff.command == ERROR) {
                temp_buff.ack = temp_buff.seq;
                temp_buff.seq = NOT_A_PKT;
                temp_buff.command=DATA;
                send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr), param_client.loss_prob);
                window_base_rcv=(window_base_rcv+1)%(2*W);
                close_connection(temp_buff,seq_to_send,filename,win_buf_snd,sockfd,serv_addr, len,window_base_snd,window_base_rcv,pkt_fly,W,byte_rcv);
            } else if (temp_buff.command == DIMENSION) {
                temp_buff.ack = temp_buff.seq;
                temp_buff.seq = NOT_A_PKT;
                temp_buff.command=DATA;
                send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr), param_client.loss_prob);
                //transmission();
            } else {
                printf("ignorato pacchetto con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command, temp_buff.payload);
            }
        }
        else if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timer(timeout_timer_id);
            return byte_rcv;
        }
    }
}

void send_message_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_snd_buf *win_buf_snd,char*message,char command,int *seq_to_send,double loss_prob,int W){
    strcpy(temp_buff->payload,command);
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
    return;
}
void send_command_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_snd_buf *win_buf_snd,int *seq_to_send,char command,char*command_name,double loss_prob,int W){
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
    return;
}

void send_fin_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_snd_buf *win_buf_snd,int *seq_to_send,double loss_prob,int W){
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
}
void send_data_in_window_cli(){
    return;
}
void rcv_data_in_window_cli(){
    return;
}
void send_message_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,char*data,char command,double loss_prob){
    strcpy(temp_buff->payload,data);
    temp_buff->command=command;
    //niente ack e sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    return;
}
void send_fin_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,double loss_prob){
    temp_buff->command=FIN;
    strcpy(temp_buff->payload,"FIN");
    //senza ack e senza sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    return;

}

void send_fin_ack_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,double loss_prob){
    temp_buff->command=FIN_ACK;
    strcpy(temp_buff->payload,"FIN_ACK");
    //senza ack e senza sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    return;
}
//chiamata per riscontrare un ack perso
void rcv_msg_re_send_ack_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,double loss_prob,int W){
    temp_buff->ack=temp_buff->seq;
    temp_buff->seq=NOT_A_PKT;
    strcpy(temp_buff->payload,"ACK");
    temp_buff->command=DATA;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    return;
}

//chiamata dopo aver ricevuto un messaggio per riscontrarlo segnarlo in finestra ricezione
void rcv_msg_send_ack_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_rcv_buf *win_buf_rcv,int *window_base_rcv,double loss_prob,int W){
    win_buf_rcv[temp_buff->seq].command=temp_buff->command;
    strcpy(win_buf_rcv[temp_buff->seq].payload,temp_buff->payload);
    win_buf_rcv[temp_buff->seq].received=1;
    temp_buff->ack=temp_buff->seq;
    temp_buff->seq=NOT_A_PKT;
    strcpy(temp_buff->payload,"ACK");
    temp_buff->command=DATA;
    if (temp_buff->seq == *window_base_rcv) {//se pacchetto riempie un buco
        // scorro la finestra fino al primo ancora non ricevuto
        while (win_buf_rcv[*window_base_rcv].received == 1) {
            win_buf_rcv[*window_base_rcv].received = 0;//segna pacchetto come non ricevuto
            *window_base_rcv = (*window_base_rcv + 1) % (2 * W);//avanza la finestra con modulo di 2W
        }
    }
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff->ack, temp_buff->seq,temp_buff->command, temp_buff->payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command, temp_buff-> payload);
    }
    return;
}
void rcv_ack_in_window_cli(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer *temp_buff,struct window_snd_buf *win_buf_snd,char*message,char command,int *seq_to_send,double loss_prob,int W,int *window_base_snd,int *pkt_fly){
    stop_timer(win_buf_snd[temp_buff->ack].time_id);
    win_buf_snd[temp_buff->ack].acked=1;
    if (temp_buff->ack == *window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
        while (win_buf_snd[*window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
            //avanzo la finestra
            win_buf_snd[*window_base_snd].acked = 0;//resetta quando scorri finestra
            *window_base_snd = (*window_base_snd + 1) % (2 * W);//avanza la finestra
            (*pkt_fly)--;
        }
    }

}