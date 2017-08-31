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

int close_list(int sockfd, struct sockaddr_in cli_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W, double loss_prob, int *byte_readed) {//manda fin non in finestra senza sequenza e ack e chiudi
    alarm(0);
    send_message(sockfd, &cli_addr, len, temp_buff, "FIN", FIN, loss_prob);
    printf("close send_list\n");
    return *byte_readed;
}

int send_list(int sockfd, struct sockaddr_in cli_addr, socklen_t len, int *seq_to_send, int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int *byte_readed, int dim, double loss_prob) {
    printf("send_list\n");
    int value = 0,*byte_sent = &value;
    char*list,*temp_list;//creare la lista e poi inviarla in parti
    list=files_in_dir(dir_server,dim);
    temp_list=list;
    alarm(TIMEOUT);
    while (1) {
        if (*pkt_fly < W && (*byte_sent) < dim) {
            //send_list_in_window(sockfd,&list, &cli_addr, len, temp_buff, win_buf_snd, seq_to_send, loss_prob, W,pkt_fly, byte_sent, dim);
        }
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &cli_addr, &len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf("pacchetto ricevuto send_list con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    if(temp_buff.command==DATA) {//se è un messaggio contenente dati
                        //rcv_ack_list_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly, dim, byte_readed);
                        if (*byte_readed == dim) {
                            close_list(sockfd, cli_addr, len, temp_buff, win_buf_snd, W, loss_prob, byte_readed);
                            printf("close sendlist\n");
                            free(temp_list);//liberazione memoria della lista,il puntatore di list è stato spostato per ricevere la lista
                            list = NULL;
                            return *byte_readed;
                        }
                    }
                    else{//se è un messaggio speciale
                        //rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                    }
                }
                else {
                    printf("send_list ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto send_list con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf("il client non è in ascolto send file\n");
            alarm(0);
            return *byte_readed;
        }
    }
}

int execute_list(struct shm_sel_repeat*shm,struct temp_buffer temp_buff) {
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    int byte_readed = 0,seq_to_send=0,window_base_snd=0,window_base_rcv=0;
    double loss_prob = param_serv.loss_prob;
    char dim[11];
    /*rcv_msg_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, win_buf_rcv, window_base_rcv, loss_prob, W);
    dimension=count_char_dir(dir_server);
    if(dimension!=0){
        sprintf(dim, "%d", dimension);
        send_message_in_window_serv(sockfd, &cli_addr, len, temp_buff, win_buf_snd, dim, DIMENSION, seq_to_send, loss_prob, W, pkt_fly);
    }
    else {
        send_message_in_window_serv(sockfd, &cli_addr, len, temp_buff, win_buf_snd, "la lista è vuota", ERROR, seq_to_send, loss_prob, W, pkt_fly);
    }*/
    errno = 0;
    alarm(TIMEOUT);
    /*while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto execute list con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly);
                } else {
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("execute list ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            } else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            } else if (temp_buff.command == FIN) {
                send_message(sockfd, &cli_addr, len, temp_buff, "FIN_ACK", FIN_ACK, loss_prob);
                stop_all_timers(win_buf_snd, W);
                stop_timeout_timer(timeout_timer_id);
                return byte_readed;//fine connesione
            } else if (temp_buff.command == START) {
                printf("messaggio start ricevuto\n");
                rcv_msg_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, win_buf_rcv, window_base_rcv,
                                           loss_prob, W);
                send_list(sockfd, cli_addr, len, seq_to_send, window_base_snd, window_base_rcv, W, pkt_fly, temp_buff, win_buf_snd, &byte_readed, dimension, loss_prob);
                if(byte_readed==dimension){
                    printf("lista correttamente inviata\n");
                }
                else{
                    printf("errore nell'invio della lista\n");
                }
                return byte_readed;
            } else {
                printf("ignorato pacchetto execute get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf("il client non è in ascolto execut get\n");
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id);
            return byte_readed;
        }
    }*/
}


