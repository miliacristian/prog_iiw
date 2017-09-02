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


int close_put_send_file2(struct shm_snd *shm_snd){
    //in questo stato ho ricevuto tutti gli ack (compreso l'ack della put),posso ricevere ack duplicati,FIN_ACK,start(fuori finestra)
    printf("close_put_send_file\n");
    struct temp_buffer temp_buff;
    alarm(TIMEOUT);
    send_message_in_window(shm_snd->shm->addr.sockfd, &(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->win_buf_snd, "FIN", FIN,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly, shm_snd->shm);
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer),MSG_DONTWAIT, (struct sockaddr *) &(shm_snd->shm->addr.dest_addr), &shm_snd->shm->addr.len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto close put send file con ack %d seq %d command %d\n"RESET, temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.command == FIN_ACK) {
                alarm(0);
                printf(GREEN"Fin_ack ricevuto\n"RESET);
                pthread_cancel(shm_snd->tid);
                return shm_snd->shm->byte_readed;//fine connesione
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd,shm_snd->shm->param.window, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        printf("errore close put ack file in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_file_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly,shm_snd->shm->dimension,&shm_snd->shm->byte_readed, shm_snd->shm);
                    }
                    else {
                        printf("\"errore close put ack_msg in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly, shm_snd->shm);
                    }
                }
                else {
                    printf("close put send_file ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv,shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            }
            else {
                printf("ignorato pacchetto close put send file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        }
        if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto close_put_send_file\n");
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel close_put_snd\n");
            pthread_exit(NULL);
        }
    }
}
int send_put_file2(struct shm_snd *shm_snd) {
    struct temp_buffer temp_buff;
    printf("send_put_file\n");
    alarm(TIMEOUT);
    while (1) {
        if (((shm_snd->shm->pkt_fly) < (shm_snd->shm->param.window)) && ((shm_snd->shm->byte_sent) < (shm_snd->shm->dimension))) {
            send_data_in_window(shm_snd->shm->addr.sockfd,shm_snd->shm->fd, &(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->win_buf_snd,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly,&shm_snd->shm->byte_sent,shm_snd->shm->dimension, shm_snd->shm);
        }
        while(recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                handle_error_with_exit("error rcv pkt connession\n");
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto send_put_file con ack %d seq %d command %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm_snd->shm->window_base_snd,shm_snd->shm->param.window, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {
                        rcv_ack_file_in_window(temp_buff,shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&(shm_snd->shm->pkt_fly),shm_snd->shm->dimension,&(shm_snd->shm->byte_readed), shm_snd->shm);
                        if ((shm_snd->shm->byte_readed) ==(shm_snd->shm->dimension)) {
                            close_put_send_file2(shm_snd);
                            printf("close sendfile\n");
                            return shm_snd->shm->byte_readed;
                        }
                    }
                    else{
                        rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly, shm_snd->shm);
                    }
                }
                else {
                    printf("send_put_file ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto send_put_file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto send_put_file\n");
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel send_put_file\n");
            pthread_exit(NULL);
        }
    }
}

void *put_client_job(void*arg){
    struct shm_snd *shm_snd=arg;
    struct temp_buffer temp_buff;
    char *path,dim_string[11];
    printf("thread snd creato\n");
    strcpy(temp_buff.payload, "put ");
    sprintf(dim_string, "%d", shm_snd->shm->dimension);
    strcpy(temp_buff.payload,dim_string);
    strcat(temp_buff.payload," ");
    strcat(temp_buff.payload,shm_snd->shm->md5_sent);
    strcat(temp_buff.payload," ");
    strcat(temp_buff.payload,shm_snd->shm->filename);
    //invia messaggio put
    send_message_in_window(shm_snd->shm->addr.sockfd,&(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_snd,temp_buff.payload,PUT,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly, shm_snd->shm);//mand
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer),0, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//attendo risposta del server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto wait for put start con ack %d seq %d command %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.command == START) {
                printf(GREEN"messaggio start ricevuto\n"RESET);
                rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len, temp_buff,shm_snd->shm->win_buf_rcv,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window);
                path = generate_full_pathname(shm_snd->shm->filename, dir_client);
                shm_snd->shm->fd= open(path, O_RDONLY);
                if (shm_snd->shm->fd == -1) {
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                send_put_file2(shm_snd);
                if (close(shm_snd->shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                printf("return wait for put start\n");
                //return shm->byte_readed;
                return NULL;
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd,shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly, shm_snd->shm);
                }
                else {
                    printf("wait for put ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto wait for put start con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        }
        if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
    return NULL;
}
void *put_client_rtx_job(void*arg){
    printf("thread rtx creato\n");
    struct shm_sel_repeat *shm=arg;
    struct temp_buffer temp_buff;
    struct node*node=NULL;
    long timer_ns_left;
    char to_rtx;
    struct timespec sleep_time;
    block_signal(SIGALRM);//il thread receiver non viene bloccato dal segnale di timeout
    /*node = alloca(sizeof(struct node));
    lock_mtx(&(shm->mtx));
    printf("lock preso\n");
    for(;;) {
        while (1) {
            if(delete_head(&shm->head,node)==-1){
                wait_on_a_condition(&(shm->list_not_empty),&shm->mtx);
            }
            else{
                if(!to_resend2(shm, *node)){
                    printf("pkt non da ritrasmettere\n");
                    continue;
                }
                else{
                    printf("pkt da ritrasmettere\n");
                    break;
                }
            }
        }
        unlock_mtx(&(shm->mtx));
        timer_ns_left=calculate_time_left(*node);
        if(timer_ns_left<=0){
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend2(shm, *node);
            if(node->lap!=shm->win_buf_snd[node->seq].lap){
                printf("wrong lap finestra %d lista %d\n", shm->win_buf_snd[node->seq].lap, node->lap);
            }
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                printf("no rtx immediata\n");
                continue;
            }
            else{
                printf("rtx immediata\n");
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                copy_buf1_in_buf2(temp_buff.payload,shm->win_buf_snd[node->seq].payload,MAXPKTSIZE-OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
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
            if(node->lap!=shm->win_buf_snd[node->seq].lap){
                printf("wrong lap finestra %d lista %d\n", shm->win_buf_snd[node->seq].lap, node->lap);
            }
            unlock_mtx(&(shm->mtx));
            printf("to_rtx %d\n", to_rtx);
            if(!to_rtx){
                printf("no rtx dopo sleep\n");
                continue;
            }
            else{
                printf("rtx dopo sleep\n");
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                copy_buf1_in_buf2(temp_buff.payload,shm->win_buf_snd[node->seq].payload,MAXPKTSIZE-OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                lock_mtx(&(shm->mtx));
                if(clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[node->seq].time))!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq,node->lap,shm->win_buf_snd[node->seq].time,shm->param.timer_ms,&shm->head,&shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
    }*/
    return NULL;
}
void put_client(struct shm_sel_repeat *shm){
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd,tid_rtx;
    struct shm_snd shm_snd;
    if(pthread_create(&tid_rtx,NULL,put_client_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rtx\n",tid_rtx);
    shm_snd.tid=tid_rtx;
    shm_snd.shm=shm;
    if(pthread_create(&tid_snd,NULL,put_client_job,&shm_snd)!=0){
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
    //ricorda di distruggere cond e rilasciare mtx
    return;
}

