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
#include "dynamic_list.h"
int rtx=0;
int close_connection_put(struct temp_buffer temp_buff, int *seq_to_send, struct window_snd_buf *win_buf_snd, int sockfd,
                         struct sockaddr_in serv_addr, socklen_t len, int *window_base_snd, int *window_base_rcv,
                         int *pkt_fly, int W, int *byte_written, double loss_prob,struct shm_sel_repeat *shm) {
    printf("close connection\n");
    send_message_in_window(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len, temp_buff,
                           shm->win_buf_snd, "FIN", FIN, &shm->seq_to_send,
                           shm->param.loss_prob, shm->param.window, &shm->pkt_fly,
                           shm);//manda messaggio di fin
    alarm(TIMEOUT);
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo fin_ack dal server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {//
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto close_conn get con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command,temp_buff.lap);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm->win_buf_snd, shm->param.window,
                                      &shm->window_base_snd, &shm->pkt_fly, shm);
                } else {
                    printf("close connect get ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (temp_buff.command == FIN_ACK) {
                alarm(0);
                pthread_cancel(shm->tid);
                printf("thread cancel close connection\n");
                printf(RED "il server ha troppe copie del file %s\n"RESET,shm->filename);
                pthread_exit(NULL);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm->addr.sockfd, &shm->addr.dest_addr,
                                                      shm->addr.len, temp_buff, shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato close connect pacchetto con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il server  non è in ascolto\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel close connection\n");
            printf(RED "il server ha troppe copie del file %s\n"RESET,shm->filename);
            pthread_exit(NULL);
        }
    }
}

int close_put_send_file(struct shm_sel_repeat *shm){
    //in questo stato ho ricevuto tutti gli ack (compreso l'ack della put),posso ricevere ack duplicati,FIN_ACK,start(fuori finestra)
    printf("close_put_send_file\n");
    struct temp_buffer temp_buff;
    alarm(TIMEOUT);
    send_message_in_window(shm->addr.sockfd, &(shm->addr.dest_addr),shm->addr.len, temp_buff,shm->win_buf_snd, "FIN", FIN,&shm->seq_to_send,shm->param.loss_prob,shm->param.window,&shm->pkt_fly, shm);
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE,MSG_DONTWAIT, (struct sockaddr *) &(shm->addr.dest_addr), &shm->addr.len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto close put send file con ack %d seq %d command %d lap %d\n"RESET, temp_buff.ack, temp_buff.seq,
                   temp_buff.command,temp_buff.lap);
            if (temp_buff.command == FIN_ACK) {
                alarm(0);
                printf(GREEN"Fin_ack ricevuto\n"RESET);
                printf("rtx %d\n",rtx);
                pthread_cancel(shm->tid);
                pthread_exit(NULL);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd,shm->param.window, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        printf("errore close put ack file in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n",shm->window_base_snd,shm->window_base_rcv);
                        handle_error_with_exit("");
                    }
                    else {
                        printf("errore close put ack_msg in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n",shm->window_base_snd,shm->window_base_rcv);
                        handle_error_with_exit("");
                    }
                }
                else {
                    printf("close put send_file ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm->window_base_rcv,shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm->addr.sockfd, &(shm->addr.dest_addr),shm->addr.len, temp_buff,shm->param.loss_prob);
                alarm(TIMEOUT);
            }
            else {
                printf("ignorato pacchetto close put send file con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n",shm->window_base_snd,shm->window_base_rcv);
                handle_error_with_exit("");
            }
        }
        if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto close_put_send_file\n");
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel close_put_snd\n");
            pthread_exit(NULL);
        }
    }
}
int send_put_file(struct shm_sel_repeat *shm) {
    struct temp_buffer temp_buff;
    printf("send_put_file\n");
    alarm(TIMEOUT);
    while (1) {
        if (((shm->pkt_fly) < (shm->param.window)) && ((shm->byte_sent) < (shm->dimension))) {
                send_data_in_window(shm->addr.sockfd,shm->fd, &(shm->addr.dest_addr),shm->addr.len, temp_buff,shm->win_buf_snd,&shm->seq_to_send,shm->param.loss_prob,shm->param.window,&shm->pkt_fly,&shm->byte_sent,shm->dimension, shm);
        }
        while(recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, MSG_DONTWAIT, (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto send_put_file con ack %d seq %d command %d lap %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command,temp_buff.lap);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd,shm->param.window, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {
                        rcv_ack_file_in_window(temp_buff,shm->win_buf_snd, shm->param.window,&shm->window_base_snd,&(shm->pkt_fly),shm->dimension,&(shm->byte_readed), shm);
                        if ((shm->byte_readed) ==(shm->dimension)) {
                            close_put_send_file(shm);
                            printf("close sendfile\n");
                            return shm->byte_readed;
                        }
                    }
                    else{
                        rcv_ack_in_window(temp_buff,shm->win_buf_snd,shm->param.window,&shm->window_base_snd,&shm->pkt_fly, shm);
                    }
                }
                else {
                    printf("send_put_file ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm->addr.sockfd,&(shm->addr.dest_addr),shm->addr.len, temp_buff,shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto send_put_file con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n",shm->window_base_snd,shm->window_base_rcv);
                handle_error_with_exit("");
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto send_put_file\n");
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel send_put_file\n");
            pthread_exit(NULL);
        }
    }
}

void *put_client_job(void*arg){
    struct shm_sel_repeat *shm=arg;
    struct temp_buffer temp_buff;
    char *path,dim_string[11];
    sprintf(dim_string, "%d", shm->dimension);
    better_strcpy(temp_buff.payload,dim_string);//non dovrebbe essere strcat?
    better_strcat(temp_buff.payload," ");
    better_strcat(temp_buff.payload,shm->md5_sent);
    better_strcat(temp_buff.payload," ");
    better_strcat(temp_buff.payload,shm->filename);
    //invia messaggio put
    send_message_in_window(shm->addr.sockfd,&(shm->addr.dest_addr),shm->addr.len,temp_buff,shm->win_buf_snd,temp_buff.payload,PUT,&shm->seq_to_send,shm->param.loss_prob,shm->param.window,&shm->pkt_fly, shm);//mand
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE,0, (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) != -1) {//attendo risposta del server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto wait for put start con ack %d seq %d command %d lap %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command,temp_buff.lap);
            if (temp_buff.command == START) {
                printf(GREEN"messaggio start ricevuto\n"RESET);
                rcv_msg_send_ack_command_in_window(shm->addr.sockfd,&shm->addr.dest_addr,shm->addr.len, temp_buff,shm->win_buf_rcv,&shm->window_base_rcv,shm->param.loss_prob,shm->param.window);
                path = generate_full_pathname(shm->filename, dir_client);
                if(path==NULL){
                    handle_error_with_exit("error in generate full path\n");
                }
                shm->fd= open(path, O_RDONLY);
                if (shm->fd == -1) {
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                send_put_file(shm);
                if (close(shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                printf("return wait for put start\n");
                return NULL;
            }
            if(temp_buff.command==ERROR) {
                rcv_msg_send_ack_command_in_window(shm->addr.sockfd,&shm->addr.dest_addr,shm->addr.len, temp_buff,shm->win_buf_rcv,&shm->window_base_rcv,shm->param.loss_prob,shm->param.window);
                close_connection_put(temp_buff, &shm->seq_to_send, shm->win_buf_snd,
                                     shm->addr.sockfd, shm->addr.dest_addr, shm->addr.len,
                                     &shm->window_base_snd, &shm->window_base_rcv,
                                     &shm->pkt_fly, shm->param.window, &shm->byte_written,
                                     shm->param.loss_prob, shm);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd,shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff,shm->win_buf_snd,shm->param.window,&shm->window_base_snd,&shm->pkt_fly, shm);
                }
                else {
                    printf("wait for put ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto wait for put start con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n",shm->window_base_snd,shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        }
        if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il server non è in ascolto put_client_job\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
    return NULL;
}
void *put_client_rtx_job(void*arg){
    printf("thread rtx creato\n");
    int byte_left;
    struct shm_sel_repeat *shm=arg;
    struct temp_buffer temp_buff;
    struct node*node=NULL;
    long timer_ns_left;
    char to_rtx;
    struct timespec sleep_time;
    block_signal(SIGALRM);//il thread receiver non viene bloccato dal segnale di timeout
    node = alloca(sizeof(struct node));
    for(;;) {
        lock_mtx(&(shm->mtx));
        while (1) {
            if(delete_head(&shm->head,node)==-1){
                wait_on_a_condition(&(shm->list_not_empty),&shm->mtx);
            }
            else{
                if(!to_resend2(shm, *node)){
                    //printf("pkt non da ritrasmettere\n");
                    continue;
                }
                else{
                    //printf("pkt da ritrasmettere\n");
                    break;
                }
            }
        }
        unlock_mtx(&(shm->mtx));
        timer_ns_left=calculate_time_left(*node);
        if(timer_ns_left<=0){
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend2(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                //printf("no rtx immediata\n");
                continue;
            }
            else{
                //printf("rtx immediata\n");
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                rtx++;
                lock_mtx(&(shm->mtx));
                if(clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[node->seq].time))!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq,node->lap,shm->win_buf_snd[node->seq].time,shm->param.timer_ms,&shm->head,&shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
        else{
            sleep_struct(&sleep_time, timer_ns_left);
            nanosleep(&sleep_time , NULL);
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend2(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                continue;
            }
            else{
                //printf("rtx dopo sleep\n");
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                rtx++;
                lock_mtx(&(shm->mtx));
                if(clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[node->seq].time))!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq,node->lap,shm->win_buf_snd[node->seq].time,shm->param.timer_ms,&shm->head,&shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
    }
    return NULL;
}
void put_client(struct shm_sel_repeat *shm){
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd,tid_rtx;
    if(pthread_create(&tid_rtx,NULL,put_client_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rtx\n",tid_rtx);
    shm->tid=tid_rtx;
    if(pthread_create(&tid_snd,NULL,put_client_job,shm)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_snd\n",tid_snd);
    block_signal(SIGALRM);//il thread principale non viene interrotto dal segnale di timeout,ci sono altri thread?(waitpid ecc?)
    if(pthread_join(tid_snd,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    if(pthread_join(tid_rtx,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    return;
}

