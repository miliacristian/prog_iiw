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


int close_connection_list(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    printf("close_connection_list\n");
    send_message_in_window(temp_buff,shm, FIN,"FIN");//manda messaggio di fin
    alarm(TIMEOUT);
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo fin_ack dal server
            printf("pacchetto ricevuto close connection list con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command,temp_buff.lap);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN_ACK) {
                alarm(0);
                pthread_cancel(shm->tid);
                printf(RED "list is empty\n" RESET);
                printf("thread cancel close_connection_list\n");
                pthread_exit(NULL);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {
                        handle_error_with_exit("impossibile ricevere dati dopo aver ricevuto messaggio errore\n");
                    }
                    else {
                        rcv_ack_in_window(temp_buff, shm);
                    }
                } else {
                    printf("close_connection_list ack duplicato\n");
                }
                alarm(TIMEOUT);
            }  else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                printf("ignorato close connection pacchetto con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il server non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf(RED "list is empty\n" RESET);
            printf("thread cancel close_connection_list\n");
            pthread_exit(NULL);
        }
    }
}

int
wait_for_fin_list(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    printf("wait for fin list\n");
    alarm(TIMEOUT);//chiusura temporizzata
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            printf("pacchetto ricevuto wait_for_fin_list con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command,temp_buff.lap);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN) {
                printf(GREEN "FIN ricevuto\n" RESET);
                alarm(0);
                pthread_cancel(shm->tid);
                printf("thread cancel wait_for_fin_list\n");
                return shm->byte_written;
                //non terminare il thread fallo ritornare al chiamante che termina lui
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        handle_error_with_exit("error ack wait for fin list\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    printf("wait_for_fin_list ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf(BLUE"FIN non ricevuto\n"RESET);
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel wait_for_fin_list\n");
            return shm->byte_written;
        }
    }
}

int rcv_list(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    alarm(TIMEOUT);
    send_message_in_window(temp_buff,shm, START,"START" );
    printf("messaggio start inviato\n");
    printf("rcv list\n");
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            printf("pacchetto ricevuto rcv_list con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                   temp_buff.seq, temp_buff.command, temp_buff.lap);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        handle_error_with_exit("errore ack in rcv_list\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    printf("rcv_list ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else if (seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                if (temp_buff.command == DATA) {
                    rcv_list_send_ack_in_window(temp_buff, shm);
                    if (shm->byte_written == shm->dimension) {
                        wait_for_fin_list(temp_buff,shm);
                        return shm->byte_written;
                    }
                }
                else {
                    printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                    handle_error_with_exit(RED "ricevuto messaggio speciale in finestra durante ricezione file\n"RESET);
                }
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto rcv list con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR && errno!=0 ) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il server non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel rcv_list\n");
            pthread_exit(NULL);
        }
    }
}

int
wait_for_list_dimension(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    errno = 0;
    char *first;
    better_strcpy(temp_buff.payload, "list");
    send_message_in_window(temp_buff,shm , LIST,temp_buff.payload);//manda messaggio get
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo risposta del server
            //mi blocco sulla risposta del server
            printf("pacchetto ricevuto wait for list_dim con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command,temp_buff.lap);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == ERROR) {
                rcv_msg_send_ack_command_in_window(temp_buff, shm);
                close_connection_list(temp_buff,shm);
            }
            else if (temp_buff.command == DIMENSION) {
                rcv_msg_send_ack_command_in_window(temp_buff, shm);
                shm->dimension = parse_integer(temp_buff.payload);
                printf("dimensione ricevuta %d\n", shm->dimension);
                shm->list = malloc(sizeof(char) * shm->dimension);
                if (shm->list == NULL) {
                    handle_error_with_exit("error in malloc\n");
                }
                memset(shm->list, '\0',(size_t) shm->dimension);
                first = shm->list;
                rcv_list(temp_buff,shm);
                if (shm->byte_written == shm->dimension) {
                    printf("file's list:\n%s", first);//stampa della lista ottenuta
                } else {
                    printf("errore,lista non correttamente ricevuta\n");
                }
                free(first);//il puntatore di list è stato spostato per inviare la lista
                shm->list = NULL;
                return shm->byte_written;
            }
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        handle_error_with_exit("errore in ack wait for list dim\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    printf("wait_for_list_dim ack duplicato\n");
                }
                alarm(TIMEOUT);
            }  else {
                printf("ignorato pacchetto wait_for_list_dim con ack %d seq %d command %d lap %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command,temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR && errno!=0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il server non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel wait for list dimension\n");
            pthread_exit(NULL);
        }
    }
}

void *list_client_job(void *arg) {
    struct shm_sel_repeat *shm = arg;
    struct temp_buffer temp_buff;
    wait_for_list_dimension(temp_buff,shm);
    return NULL;
}

void *list_client_rtx_job(void *arg) {
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

void list_client(struct shm_sel_repeat *shm) {
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd,tid_rtx;
    if(pthread_create(&tid_rtx,NULL,list_client_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread list_client_rtx\n");
    }
    printf("%lu tid_rtx\n",tid_rtx);
    shm->tid=tid_rtx;
    if(pthread_create(&tid_snd,NULL,list_client_job,shm)!=0){
        handle_error_with_exit("error in create thread list_client\n");
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