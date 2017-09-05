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
#include "dynamic_list.h"
int rtx_list_server=0;
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
            send_list_in_window(sockfd,&list, &cli_addr, len, temp_buff, win_buf_snd, seq_to_send, loss_prob, W,pkt_fly, byte_sent, dim);
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
                        rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
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
    rcv_msg_send_ack_command_in_window(sockfd, &cli_addr, len, temp_buff, win_buf_rcv, window_base_rcv, loss_prob, W);
    dimension=count_char_dir(dir_server);
    if(dimension!=0){
        sprintf(dim, "%d", dimension);
        send_message_in_window_serv(sockfd, &cli_addr, len, temp_buff, win_buf_snd, dim, DIMENSION, seq_to_send, loss_prob, W, pkt_fly);
    }
    else {
        send_message_in_window_serv(sockfd, &cli_addr, len, temp_buff, win_buf_snd, "la lista è vuota", ERROR, seq_to_send, loss_prob, W, pkt_fly);
    }
    errno = 0;
    alarm(TIMEOUT);
    while (1) {
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
    }
}
void *list_server_rtx_job(void*arg){
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
                copy_buf1_in_buf2(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                rtx_list_server++;
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
                copy_buf1_in_buf2(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                rtx_list_server++;
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
void *list_server_job(void*arg){
    struct shm_snd *shm_snd=arg;
    struct temp_buffer temp_buff;
    wait_for_??(shm_snd->shm->addr.sockfd,shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,shm_snd->shm->filename,
                   &shm_snd->shm->byte_written,&shm_snd->shm->seq_to_send,&shm_snd->shm->window_base_snd,
                   &shm_snd->shm->window_base_rcv,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly,temp_buff,
                   shm_snd->shm->win_buf_rcv,shm_snd->shm->win_buf_snd,shm_snd);
    return NULL;
}
void list_server(struct shm_sel_repeat *shm){
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd,tid_rtx;
    struct shm_snd shm_snd;
    if(pthread_create(&tid_rtx,NULL,list_server_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rtx\n",tid_rtx);
    shm_snd.tid=tid_rtx;
    shm_snd.shm=shm;
    if(pthread_create(&tid_snd,NULL,list_server_job,&shm_snd)!=0){
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