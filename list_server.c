#include "basic.h"
#include "io.h"
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

int close_list(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {//manda fin non in finestra senza sequenza e ack e chiudi
    alarm(0);
    send_message(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len, temp_buff, "FIN",
                 FIN, shm->param.loss_prob);
    pthread_cancel(shm->tid);
    printf("thread cancel close list\n");
    pthread_exit(NULL);
}

int send_list( struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    printf("send_list\n");
    char *temp_list;//creare la lista e poi inviarla in parti
    shm->list = files_in_dir(dir_server, shm->dimension);
    temp_list = shm->list;
    alarm(TIMEOUT);
    while (1) {
        if (shm->pkt_fly < shm->param.window && (shm->byte_sent) < shm->dimension) {
            send_list_in_window(temp_buff, shm);
        }
        while(recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, MSG_DONTWAIT,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            printf("pacchetto ricevuto send_list con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command,temp_buff.lap);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }

            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {//se è un messaggio contenente dati
                        rcv_ack_list_in_window(temp_buff, shm);
                        if (shm->byte_readed == shm->dimension) {
                            close_list(temp_buff, shm);
                            free(temp_list);//liberazione memoria della lista,il puntatore di list è stato spostato per ricevere la lista
                            shm->list = NULL;
                            return shm->byte_readed;
                        }
                    } else {//se è un messaggio speciale
                        rcv_ack_in_window(temp_buff, shm);
                        if(shm->byte_readed==shm->dimension){
                            close_list(temp_buff,shm);
                            free(temp_list);//liberazione memoria della lista,il puntatore di list è stato spostato per ricevere la lista
                            shm->list = NULL;
                            return shm->byte_readed;
                        }
                    }
                } else {
                    printf("send_list ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto send_list con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf("il client non è in ascolto send file\n");
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel send list\n");
            pthread_exit(NULL);
        }
    }
}

int wait_for_start_list(struct shm_sel_repeat *shm, struct temp_buffer temp_buff) {
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    char dim[11];
    shm->dimension = count_char_dir(dir_server);
    if (shm->dimension != 0) {
        sprintf(dim, "%d", shm->dimension);
        send_message_in_window(temp_buff,shm , DIMENSION,dim);
    } else {
        send_message_in_window(temp_buff ,shm, ERROR,"la lista è vuota");
    }
    errno = 0;
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            printf("pacchetto ricevuto wait_for_start_list con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command,temp_buff.lap);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                printf("alarm stoppato\n");
                alarm(0);
            }
            if (temp_buff.command == FIN) {
                send_message(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len,
                             temp_buff, "FIN_ACK", FIN_ACK, shm->param.loss_prob);
                alarm(0);
                pthread_cancel(shm->tid);
                printf("thread cancel wait_for_start_list\n");
                pthread_exit(NULL);
            }
            else if (temp_buff.command == START) {
                printf("messaggio start ricevuto\n");
                rcv_msg_send_ack_command_in_window(temp_buff, shm);
                send_list(temp_buff,shm);
                if (shm->byte_readed == shm->dimension) {
                    printf("lista correttamente inviata\n");
                } else {
                    printf("errore nell'invio della lista\n");
                }
                return shm->byte_readed;
            }
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    printf("wait_for_start_list ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            }  else {
                printf("ignorato pacchetto wait_for_start_list con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        }
         else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if(great_alarm_serv == 1){
            great_alarm_serv = 0;
            printf("il client non è in ascolto wait_for_start_list\n");
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel wait for start list\n");
            pthread_exit(NULL);
        }
    }
}

void *list_server_rtx_job(void *arg) {
    printf("thread rtx creato\n");
    struct shm_sel_repeat *shm=arg;
    struct temp_buffer temp_buff;
    struct node*node=NULL;
    long timer_ns_left;
    char to_rtx;
    struct timespec sleep_time, rtx_time;
    block_signal(SIGALRM);//il thread receiver non viene bloccato dal segnale di timeout
    node = alloca(sizeof(struct node));
    for(;;) {
        lock_mtx(&(shm->mtx));
        while (1) {
            if(delete_head(&shm->head,node)==-1){
                wait_on_a_condition(&(shm->list_not_empty),&shm->mtx);
            }
            else{
                if(!to_resend(shm, *node)){
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
            to_rtx = to_resend(shm, *node);
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
                lock_mtx(&(shm->mtx));
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                if(clock_gettime(CLOCK_MONOTONIC, &(rtx_time))!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq,node->lap,rtx_time,shm->param.timer_ms,&shm->head,&shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
        else{
            sleep_struct(&sleep_time, timer_ns_left);
            nanosleep(&sleep_time , NULL);
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                continue;
            }
            else{
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                //printf("rtx dopo sleep\n");
                lock_mtx(&(shm->mtx));
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                if(clock_gettime(CLOCK_MONOTONIC, &(rtx_time))!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq,node->lap,rtx_time,shm->param.timer_ms,&shm->head,&shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
    }
    return NULL;
}

void *list_server_job(void *arg) {
    struct shm_sel_repeat *shm= arg;
    struct temp_buffer temp_buff;
    wait_for_start_list(shm, temp_buff);
    return NULL;
}

void list_server(struct shm_sel_repeat *shm) {
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd,tid_rtx;
    if(pthread_create(&tid_rtx,NULL,list_server_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread list_server_rtx\n");
    }
    printf("%lu tid_rtx\n",tid_rtx);
    shm->tid=tid_rtx;
    if(pthread_create(&tid_snd,NULL,list_server_job,shm)!=0){
        handle_error_with_exit("error in create thread list_server\n");
    }
    printf("%lu tid_snd\n",tid_snd);
    block_signal(SIGALRM);//il thread principale non viene interrotto dal segnale di timeout,ci sono altri thread?(waitpid ecc?)
    if(pthread_join(tid_snd,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    if(pthread_join(tid_rtx,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    unlock_signal(SIGALRM);
    return;
}

int execute_list(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    rcv_msg_send_ack_command_in_window(temp_buff, shm);
    list_server(shm);
    return shm->byte_readed;
}