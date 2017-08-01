#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include "timer.h"
#include "Server.h"
#include "get_server.h"

int execute_get(int sockfd, struct sockaddr_in cli_addr, socklen_t len, int seq_to_send, int window_base_snd,int window_base_rcv, int W,int pkt_fly,struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv, struct window_snd_buf *win_buf_snd){
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    int byte_readed = 0, fd, byte_send, byte_left;
    double timer = param_serv.timer_ms, loss_prob = param_serv.loss_prob;
    char *command, *path, dim[11], first_pkt;
    path = generate_full_pathname(temp_buff.payload + 4, dir_server);
    printf("%s\n", path);
    temp_buff.ack = temp_buff.seq;
    temp_buff.seq= NOT_A_PKT;
    temp_buff.command = DATA;
    strcpy(temp_buff.payload, "ACK");
    send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr), param_serv.loss_prob);
    if (check_if_file_exist(path)) {
        byte_left = get_file_size(path);
        printf("dimensione del file %d\n", byte_left);
        sprintf(dim, "%d", byte_left);
        strcpy(temp_buff.payload, dim);//scrivo dentro tem_buff la dimensione del file
        temp_buff.ack = NOT_AN_ACK;
        temp_buff.seq = seq_to_send;
        temp_buff.command = DIMENSION;
    } else {
        temp_buff.ack = NOT_AN_ACK;
        temp_buff.seq = seq_to_send;
        temp_buff.command = ERROR;
        strcpy(temp_buff.payload, "il file non esiste\n");
    }
    strcpy(win_buf_snd[seq_to_send].payload, temp_buff.payload);
    win_buf_snd[seq_to_send].command = temp_buff.command;
    send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr), param_serv.loss_prob);
    start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
    seq_to_send = (seq_to_send + 1) % (2 * W);
    start_timeout_timer(timeout_timer_id, 5000);
    great_alarm = 0;
    //printf("sono qui\n");
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) !=
            -1) {//risposta del server
            stop_timer(timeout_timer_id);
            start_timeout_timer(timeout_timer_id, 5000);
            printf("pacchetto ricevuto con ack %d seq %d command %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.command, temp_buff.payload);
            if (temp_buff.seq == NOT_A_PKT) {
                stop_timer(win_buf_snd[temp_buff.ack].time_id);
                win_buf_snd[temp_buff.ack].acked = 1;
                if (temp_buff.ack ==
                    window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while (win_buf_snd[window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        win_buf_snd[window_base_snd].acked = 0;//resetta quando scorri finestra
                        window_base_snd = (window_base_snd + 1) % (2 * W);//avanza la finestra
                        pkt_fly--;
                    }
                }
            } else if (!seq_is_in_window(window_base_snd, window_base_snd + W - 1, W, temp_buff.seq)) {
                //start_timeout_timer(timeout_timer_id, 5000);
                temp_buff.command = DATA;
                temp_buff.ack = temp_buff.seq;
                temp_buff.seq = NOT_A_PKT;
                send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr), param_serv.loss_prob);
            } else if (temp_buff.command == FIN) {
                temp_buff.command = FIN_ACK;
                temp_buff.seq = seq_to_send;
                temp_buff.ack = NOT_AN_ACK;
                strcpy(temp_buff.payload, "FIN_ACK");
                send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr), param_serv.loss_prob);
                stop_all_timers(win_buf_snd, W);
                stop_timer(timeout_timer_id);
                return byte_readed;//fine connesione
            } else if (temp_buff.command == START) {
                temp_buff.command = DATA;
                temp_buff.ack = temp_buff.seq;
                temp_buff.seq = NOT_A_PKT;
                send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr), param_serv.loss_prob);
                win_buf_rcv[temp_buff.ack].received = 1;
                strcpy(win_buf_rcv[temp_buff.seq].payload, temp_buff.payload);//memorizzo il pacchetto n-esimo
                temp_buff.ack = temp_buff.seq;
                temp_buff.seq = NOT_A_PKT;
                if (temp_buff.seq == window_base_rcv) {//se pacchetto riempie un buco
                    // scorro la finestra fino al primo ancora non ricevuto
                    while (win_buf_rcv[window_base_rcv].received == 1) {
                        win_buf_rcv[window_base_rcv].received = 0;//segna pacchetto come non ricevuto
                        window_base_rcv = (window_base_rcv + 1) % (2 * W);//avanza la finestra con modulo di 2W
                    }
                }
                //send_file();
            }
        } else if (great_alarm == 1) {
            great_alarm = 0;
            printf("il client non è in ascolto\n");
            stop_all_timers(win_buf_snd, W);
            return byte_readed;
        }
    }
}