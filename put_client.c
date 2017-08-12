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

int close_put_send_file(int sockfd, struct sockaddr_in serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W, double loss_prob, int *byte_readed,int *window_base_snd,int *pkt_fly,int*window_base_rcv,int *seq_to_send) {//manda fin non in finestra senza sequenza e ack e chiudi
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    send_message_in_window_cli(sockfd, &serv_addr, len, temp_buff,win_buf_snd, "FIN", FIN,seq_to_send, loss_prob,W,pkt_fly);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                //ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly);
                } else {
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            } else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window(sockfd, &serv_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            } else if (temp_buff.command == FIN_ACK) {
                stop_all_timers(win_buf_snd, W);
                stop_timeout_timer(timeout_timer_id);
                return byte_readed;//fine connesione
            }else {
                printf("ignorato pacchetto execute get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d", *window_base_snd, *window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            great_alarm = 0;
            printf("il client non è in ascolto execut get\n");
            stop_all_timers(win_buf_snd, W);
            return byte_readed;
        }
    }
}

int send_file(int sockfd, struct sockaddr_in serv_addr, socklen_t len, int *seq_to_send, int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int fd, int *byte_readed, int dim, double loss_prob) {
    printf("send_file\n");
    int value = 0,*byte_sent = &value;
    start_timeout_timer(timeout_timer_id,TIMEOUT);
    while (1) {
        if (*pkt_fly < W && (*byte_sent) < dim) {
            send_data_in_window_serv(sockfd, fd, &serv_addr, len, temp_buff, win_buf_snd, seq_to_send, loss_prob, W,pkt_fly, byte_sent, dim);
        }
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &serv_addr, &len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                //ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id);
            }
            printf("pacchetto ricevuto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    rcv_ack_file_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly, dim, byte_readed);
                    if (*byte_readed == dim) {
                        close_put_send_file(sockfd, serv_addr, len, temp_buff, win_buf_snd, W, loss_prob, byte_readed,window_base_snd,pkt_fly,window_base_rcv,seq_to_send);
                        printf("close sendfile\n");
                        return *byte_readed;
                    }
                } else {
                    printf("ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window(sockfd, &serv_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id, TIMEOUT);
            } else {
                printf("ignorato pacchetto execute get con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d", *window_base_snd, *window_base_rcv);
                start_timeout_timer(timeout_timer_id,TIMEOUT);
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm == 1) {
            great_alarm = 0;
            printf("il client non è in ascolto send file\n");
            stop_all_timers(win_buf_snd, W);
            return *byte_readed;
        }
    }
}
int wait_for_put_start(int sockfd, struct sockaddr_in serv_addr, socklen_t  len,char*filename, int *byte_readed , int *seq_to_send , int *window_base_snd , int *window_base_rcv, int W, int *pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,int dimension){
    return 0;
}

