#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include "timer.h"
#include "Server.h"
#include "get_server.h"
//chiamata per riscontrare un ack perso
void rcv_msg_re_send_ack_in_window_serv(int sockfd,struct sockaddr_in *cli_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob,int W){
    //già memorizzato in finestra
    temp_buff.ack=temp_buff.seq;
    temp_buff.seq=NOT_A_PKT;
    strcpy(temp_buff.payload,"ACK");
    temp_buff.command=DATA;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) cli_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,temp_buff.command, temp_buff.payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff.ack, temp_buff.seq,temp_buff.command, temp_buff.payload);
    }
    return;
}

//chiamata dopo aver ricevuto un messaggio per riscontrarlo segnarlo in finestra ricezione
void rcv_msg_send_ack_in_window_serv(int sockfd,struct sockaddr_in *cli_addr,socklen_t len,struct temp_buffer temp_buff,struct window_rcv_buf *win_buf_rcv,int *window_base_rcv,double loss_prob,int W){
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
        if (sendto(sockfd, &ack_buff, MAXPKTSIZE, 0, (struct sockaddr *) cli_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", ack_buff.ack, ack_buff.seq,ack_buff.command, ack_buff.payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",ack_buff.ack, ack_buff.seq,ack_buff.command, ack_buff.payload);
    }
    return;
}
void rcv_ack_in_window_serv(struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,int W,int *window_base_snd,int *pkt_fly){
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

void send_message_in_window_serv(int sockfd,struct sockaddr_in *cli_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,char*message,char command,int *seq_to_send,double loss_prob,int W,int *pkt_fly){
    temp_buff.command=command;
    temp_buff.ack=NOT_AN_ACK;
    temp_buff.seq=*seq_to_send;
    strcpy(temp_buff.payload,message);
    strcpy(win_buf_snd[*seq_to_send].payload,temp_buff.payload);
    win_buf_snd[*seq_to_send].command=command;
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) cli_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,temp_buff.command, temp_buff.payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff.ack, temp_buff.seq,temp_buff.command, temp_buff.payload);
    }
    start_timer(win_buf_snd[*seq_to_send].time_id,&sett_timer);
    *seq_to_send = ((*seq_to_send) + 1) % (2 * W);
    (*pkt_fly)++;
    return;
}
void send_data_in_window_serv(){
    return;
}
void rcv_data_in_window_serv(){
    return;
}

void send_fin_serv(int sockfd,struct sockaddr_in *cli_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob){
    temp_buff.command=FIN;
    strcpy(temp_buff.payload,"FIN");
    //senza ack e senza sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) cli_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,temp_buff.command, temp_buff.payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff.ack,temp_buff.seq, temp_buff.command, temp_buff.payload);
    }
    return;

}

void send_fin_ack_serv(int sockfd,struct sockaddr_in *cli_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob){
    temp_buff.command=FIN_ACK;
    strcpy(temp_buff.payload,"FIN_ACK");
    //senza ack e senza sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) cli_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n",temp_buff.ack,temp_buff.seq, temp_buff.command, temp_buff.payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff.ack,temp_buff.seq, temp_buff.command, temp_buff.payload);
    }
    return;
}
void send_message_serv(int sockfd,struct sockaddr_in *cli_addr,socklen_t len,struct temp_buffer temp_buff,char*data,char command,double loss_prob){
    strcpy(temp_buff.payload,data);
    temp_buff.command=command;
    //niente ack e sequenza
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) cli_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d dati %s:\n",temp_buff.ack,temp_buff.seq, temp_buff.command, temp_buff.payload);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d dati %s perso\n",temp_buff.ack,temp_buff.seq, temp_buff.command, temp_buff.payload);
    }
    return;
}
void close_get_send_file(int sockfd,struct sockaddr_in cli_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,int W,double loss_prob){//manda fin non in finestra senza sequenza e ack e chiudi
    send_message_serv(sockfd,&cli_addr,len,temp_buff,"FIN",FIN,loss_prob);
    stop_timer(timeout_timer_id);
    stop_all_timers(win_buf_snd, W);
    return;
}
int send_file(int sockfd, struct sockaddr_in cli_addr, socklen_t len, int seq_to_send, int window_base_snd,int window_base_rcv, int W,int pkt_fly,struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv, struct window_snd_buf *win_buf_snd,int fd,int byte_readed,int dim,double loss_prob){
    errno=0;
    start_timeout_timer(timeout_timer_id, 5000);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) != -1) {//risposta del server
            stop_timer(timeout_timer_id);
            printf("pacchetto ricevuto con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.command, temp_buff.payload);
            if (temp_buff.seq == NOT_A_PKT) {
                if(seq_is_in_window(window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window_serv(temp_buff,win_buf_snd,W,&window_base_snd,&pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,5000);
            }
            else if (!seq_is_in_window(window_base_rcv, W, temp_buff.seq)) {
                printf("pacchetto fuori finestra execut get \n");
                rcv_msg_re_send_ack_in_window_serv(sockfd,&cli_addr,len,temp_buff,loss_prob,W);
                start_timeout_timer(timeout_timer_id,5000);
            }

            else {
                printf("ignorato pacchetto execute get con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command, temp_buff.payload);
                printf("winbase snd %d winbase rcv %d",window_base_snd,window_base_rcv);
                start_timeout_timer(timeout_timer_id,5000);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        else if (great_alarm == 1) {
            great_alarm = 0;
            printf("il client non è in ascolto\n");
            stop_all_timers(win_buf_snd, W);
            return byte_readed;
        }
    }
}
int execute_get(int sockfd, struct sockaddr_in cli_addr, socklen_t len, int seq_to_send, int window_base_snd,int window_base_rcv, int W,int pkt_fly,struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv, struct window_snd_buf *win_buf_snd){
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    int byte_readed = 0, fd,dimension;
    double timer = param_serv.timer_ms, loss_prob = param_serv.loss_prob;
    char *command, *path, dim[11], first_pkt;
    rcv_msg_send_ack_in_window_serv(sockfd,&cli_addr,len,temp_buff,win_buf_rcv,&window_base_rcv,loss_prob,W);
    path = generate_full_pathname(temp_buff.payload + 4, dir_server);
    printf("%s\n", path);
    if (check_if_file_exist(path)) {
        dimension = get_file_size(path);
        printf("dimensione del file %d\n",dimension);
        sprintf(dim, "%d",dimension);
        fd=open(path,O_RDONLY);
        if(fd==-1){
            handle_error_with_exit("error in open\n");
        }
        send_message_in_window_serv(sockfd,&cli_addr,len,temp_buff,win_buf_snd,dim,DIMENSION,&seq_to_send,loss_prob,W,&pkt_fly);
    }
    else {
        send_message_in_window_serv(sockfd,&cli_addr,len,temp_buff,win_buf_snd,"il file non esiste",ERROR,&seq_to_send,loss_prob,W,&pkt_fly);
    }
    errno=0;
    start_timeout_timer(timeout_timer_id, 5000);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) !=
            -1) {//risposta del server
            stop_timer(timeout_timer_id);
            printf("pacchetto ricevuto con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.command, temp_buff.payload);
            if (temp_buff.seq == NOT_A_PKT) {
                if(seq_is_in_window(window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window_serv(temp_buff,win_buf_snd,W,&window_base_snd,&pkt_fly);
                }
                else{
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,5000);
            }
            else if (!seq_is_in_window(window_base_rcv, W, temp_buff.seq)) {
                printf("pacchetto fuori finestra execut get \n");
                rcv_msg_re_send_ack_in_window_serv(sockfd,&cli_addr,len,temp_buff,loss_prob,W);
                start_timeout_timer(timeout_timer_id,5000);
            }
            else if (temp_buff.command == FIN) {
                send_message_serv(sockfd,&cli_addr,len,temp_buff,"FIN_ACK",FIN_ACK,loss_prob);
                stop_all_timers(win_buf_snd, W);
                stop_timer(timeout_timer_id);
                return byte_readed;//fine connesione
            }
            else if (temp_buff.command == START) {
                printf("messaggio start ricevuto\n");
                rcv_msg_send_ack_in_window_serv(sockfd,&cli_addr,len,temp_buff,win_buf_rcv,&window_base_rcv,loss_prob,W);
                send_file(sockfd,cli_addr,len,seq_to_send,window_base_snd,window_base_rcv, W,pkt_fly,temp_buff,win_buf_rcv,win_buf_snd,fd,byte_readed,dimension,loss_prob);
            }

            else {
                printf("ignorato pacchetto execute get con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command, temp_buff.payload);
                printf("winbase snd %d winbase rcv %d",window_base_snd,window_base_rcv);
                start_timeout_timer(timeout_timer_id,5000);
            }
        }
        else if(errno!=EINTR){
            handle_error_with_exit("error in recvfrom\n");
        }
        else if (great_alarm == 1) {
            great_alarm = 0;
            printf("il client non è in ascolto\n");
            stop_all_timers(win_buf_snd, W);
            return byte_readed;
        }
    }
}

