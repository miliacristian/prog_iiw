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


int close_get_send_file(int sockfd, struct sockaddr_in cli_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W, double loss_prob, int *byte_readed) {//manda fin non in finestra senza sequenza e ack e chiudi
    stop_timeout_timer(timeout_timer_id_serv);
    send_message(sockfd, &cli_addr, len, temp_buff, "FIN", FIN, loss_prob);
    printf("close get send_file\n");
    return *byte_readed;
}

int send_file(int sockfd, struct sockaddr_in cli_addr, socklen_t len, int *seq_to_send, int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int fd, int *byte_readed, int dim, double loss_prob) {
    printf("send_file\n");
    int ack_dup=0;
    int value = 0,*byte_sent = &value;
    start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
    while (1) {
        if (*pkt_fly < W && (*byte_sent) < dim) {
            send_data_in_window_serv(sockfd, fd, &cli_addr, len, temp_buff, win_buf_snd, seq_to_send, loss_prob, W,pkt_fly, byte_sent, dim);
        }
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &cli_addr, &len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_serv);
            }
            printf("pacchetto ricevuto send_file con ack %d seq %d command %d win_base_snd %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command,*window_base_snd);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    if(temp_buff.command==DATA) {
                        rcv_ack_file_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly, dim,byte_readed);
                        printf("byte readed %d ack dup %d\n",*byte_readed,ack_dup);
                        if (*byte_readed == dim) {
                            close_get_send_file(sockfd, cli_addr, len, temp_buff, win_buf_snd, W, loss_prob,byte_readed);
                            printf("close sendfile\n");
                            return *byte_readed;
                        }
                    }
                    else{
                        rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                    }
                }
                else {
                    ack_dup++;
                    printf("send_file ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id_serv, TIMEOUT);
            } else {
                printf("ignorato pacchetto send_file con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf("il client non è in ascolto send file\n");
            stop_timeout_timer(timeout_timer_id_serv);
            return *byte_readed;
        }
    }
}

int execute_get(struct shm_sel_repeat *shm,struct temp_buffer temp_buff) {
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    int byte_readed = 0, fd, dimension,window_base_snd=0,window_base_rcv=0,seq_to_send=0;
    double loss_prob = param_serv.loss_prob;
    char *path, dim_string[11];
    /*rcv_msg_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, win_buf_rcv, window_base_rcv, loss_prob, W);
    path = generate_full_pathname(temp_buff.payload + 4, dir_server);
    if (check_if_file_exist(path)) {
        dimension = get_file_size(path);
        sprintf(dim_string, "%d", dimension);
        fd = open(path, O_RDONLY);
        if (fd == -1) {
            handle_error_with_exit("error in open\n");
        }
        send_message_in_window_serv(sockfd, &cli_addr, len, temp_buff, win_buf_snd, dim_string, DIMENSION, seq_to_send, loss_prob, W, pkt_fly);
    }
    else {
        send_message_in_window_serv(sockfd, &cli_addr, len, temp_buff, win_buf_snd, "il file non esiste", ERROR, seq_to_send, loss_prob, W, pkt_fly);
    }
    errno = 0;
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto execute get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly);
                } else {
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("execute get ack duplicato\n");
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
                send_file(sockfd, cli_addr, len, seq_to_send, window_base_snd, window_base_rcv, W, pkt_fly, temp_buff, win_buf_snd, fd, &byte_readed, dimension, loss_prob);
                if (close(fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                return byte_readed;
            } else {
                printf("ignorato pacchetto execute get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        } else if (errno != EINTR && errno!=0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf("il client non è in ascolto execut get\n");
            stop_timeout_timer(timeout_timer_id);
            stop_all_timers(win_buf_snd, W);
            return byte_readed;
        }
    }*/
}


