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
pthread_cond_t start_rcv;
pthread_cond_t all_acked;
pthread_cond_t buf_not_full;
pthread_cond_t buf_not_empty;
int close_put_send_file2(struct shm_snd *shm_snd){
    //in questo stato ho ricevuto tutti gli ack (compreso l'ack della put),posso ricevere ack duplicati,FIN_ACK,start(fuori finestra)
    printf("close_put_send_file\n");
    struct temp_buffer temp_buff;
    //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
    send_message_in_window_cli(shm_snd->shm->addr.sockfd, &(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->win_buf_snd, "FIN", FIN,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly);
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer),MSG_DONTWAIT, (struct sockaddr *) &(shm_snd->shm->addr.dest_addr), &shm_snd->shm->addr.len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                //stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto close put send file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.command == FIN_ACK) {
                //stop_timeout_timer(timeout_timer_id_client);
                printf("close put send file\n");
                return shm_snd->shm->byte_readed;//fine connesione
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd,shm_snd->shm->param.window, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        printf("errore close put ack file in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_file_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly,shm_snd->shm->dimension,&shm_snd->shm->byte_readed);
                    }
                    else {
                        printf("\"errore close put ack_msg in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly);
                    }
                }
                else {
                    printf("close put send_file ack duplicato\n");
                }
                //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv,shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->param.loss_prob);
                //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto close put send file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
        } else if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto close_put_send_file\n");
            //stop_timeout_timer(timeout_timer_id_client);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel close_put_snd\n");
            pthread_exit(NULL);
        }
    }
}
int send_put_file2(struct shm_snd *shm_snd) {
    struct temp_buffer temp_buff;
    printf("send_put_file\n");
    //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
    while (1) {
        //lock_mtx(&shm_snd->shm->mtx);
        if ((shm_snd->shm->pkt_fly) < (shm_snd->shm->param.window) && (shm_snd->shm->byte_sent) < (shm_snd->shm->dimension)) {
            send_data_in_window_cli(shm_snd->shm->addr.sockfd,shm_snd->shm->fd, &(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->win_buf_snd,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly,&shm_snd->shm->byte_sent,shm_snd->shm->dimension);
            //usleep(500);
        }
        else if((shm_snd->shm->pkt_fly)==(shm_snd->shm->param.window)){
            printf("seq to send %d pkt fly %d W %d sent %d dim %d\n",shm_snd->shm->seq_to_send,shm_snd->shm->pkt_fly,shm_snd->shm->param.window,shm_snd->shm->byte_sent,shm_snd->shm->dimension);
            printf("impossibile inviare\n");
        }
        else if((shm_snd->shm->byte_sent) == (shm_snd->shm->dimension)){
            printf("ricevuto %d mandato tutto \n",shm_snd->shm->byte_readed);
        }
        //unlock_mtx(&shm_snd->shm->mtx);
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                //stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto send_put_file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm_snd->shm->window_base_snd,shm_snd->shm->param.window, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {
                        //lock_mtx(&shm_snd->shm->mtx);
                        rcv_ack_file_in_window(temp_buff,shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly,shm_snd->shm->dimension,&shm_snd->shm->byte_readed);
                        //unlock_mtx(&shm_snd->shm->mtx);
                        if (shm_snd->shm->byte_readed ==shm_snd->shm->dimension) {
                            close_put_send_file2(shm_snd);
                            printf("close sendfile\n");
                            return shm_snd->shm->byte_readed;
                        }
                    }
                    else{
                        //lock_mtx(&shm_snd->shm->mtx);
                        rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly);
                        //unlock_mtx(&shm_snd->shm->mtx);
                    }
                }
                else {
                    printf("send_put_file ack duplicato\n");
                }
                //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len, temp_buff,shm_snd->shm->param.loss_prob);
                //start_timeout_timer(timeout_timer_id_client, TIMEOUT);
            } else {
                printf("ignorato pacchetto send_put_file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto send_put_file\n");
            //stop_timeout_timer(timeout_timer_id_client);
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
    strcat(temp_buff.payload,shm_snd->shm->filename);
    //invia messaggio put
    //lock_mtx(&shm_snd->shm->mtx);
    printf("lock acquired put client snd\n");
    send_message_in_window_cli(shm_snd->shm->addr.sockfd,&(shm_snd->shm->addr.dest_addr),shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_snd,temp_buff.payload,PUT,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly);//mand
    //unlock_thread_on_a_condition(&buf_not_empty);
    //unlock_mtx(&shm_snd->shm->mtx);
    //start_timeout_timer(timeout_timer_id_client,TIMEOUT);
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//attendo risposta del server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                //stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto wait for put start con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.command == START) {
                printf("messaggio start ricevuto\n");
                //lock_mtx(&shm_snd->shm->mtx);
                rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len, temp_buff,shm_snd->shm->win_buf_rcv,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window);
                //unlock_mtx(&shm_snd->shm->mtx);
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
                    //lock_mtx(&shm_snd->shm->mtx);
                    rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly);
                    //unlock_mtx(&shm_snd->shm->mtx);
                }
                else {
                    printf("wait for put ack duplicato\n");
                }
                //start_timeout_timer(timeout_timer_id_client, TIMEOUT);
            } else {
                printf("ignorato pacchetto wait for put start con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                //start_timeout_timer(timeout_timer_id_client, TIMEOUT);
            }
        } else if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            //stop_timeout_timer(timeout_timer_id_client);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
    return NULL;
}
void *put_client_rtx_job(void*arg){
    printf("thread rcv creato\n");
    block_signal(SIGRTMIN+1);//il thread receiver non viene bloccato dal segnale di timeout
    while(1){}

    /*struct shm_sel_repeat *shm=arg;
    lock_mtx(&(shm->mtx));
    printf("lock acquired put client rcv\n");
    if(shm->seq_to_send==shm->seq_to_scan){
        printf("buffer vuoto\n");
        wait_on_a_condition(&buf_not_empty,&(shm->mtx));
    }*/
    return NULL;
}
/*int close_put_send_file(int sockfd, struct sockaddr_in serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W, double loss_prob, int *byte_readed,int *window_base_snd,int *pkt_fly,int*window_base_rcv,int *seq_to_send,int dimension) {//manda fin non in finestra senza sequenza e ack e chiudi
    //in questo stato ho ricevuto tutti gli ack (compreso l'ack della put),posso ricevere ack duplicati,FIN_ACK,start(fuori finestra)
    printf("function close_put_send_file\n");
    start_timeout_timer(timeout_timer_id_client,TIMEOUT);
    stoppa_timer(win_buf_snd,W);//forza lo stop dei segnali pendenti
    send_message_in_window_cli(sockfd, &serv_addr, len, temp_buff,win_buf_snd, "FIN", FIN,seq_to_send, loss_prob,W,pkt_fly);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto close put send file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.command == FIN_ACK) {
                stop_all_timers(win_buf_snd, W);
                stop_timeout_timer(timeout_timer_id_client);
                printf("close put send file\n");
                return *byte_readed;//fine connesione
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    if(temp_buff.command==DATA){
                        printf("errore close put ack file in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_file_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly,dimension, byte_readed);
                    }
                    else {
                        printf("\"errore close put ack_msg in finestra\n");
                        printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                        handle_error_with_exit("");
                        rcv_ack_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly);
                    }
                }
                else {
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("close put send_file ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd, &serv_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto close put send file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
        } else if (errno != EINTR && errno!=0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto close_put_send_file\n");
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id_client);
            return *byte_readed;
        }
    }
}*/

/*int send_put_file(int sockfd, struct sockaddr_in serv_addr, socklen_t len, int *seq_to_send, int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int fd, int *byte_readed, int dim, double loss_prob) {
    //in questo stato ho già ricevuto almeno una volta START,posso ricevere ack put,start e ack del file
    printf("send_file\n");
    int value = 0,*byte_sent = &value;
    start_timeout_timer(timeout_timer_id_client,TIMEOUT);
    while (1) {
        if (*pkt_fly < W && (*byte_sent) < dim) {
            send_data_in_window_cli(sockfd, fd, &serv_addr, len, temp_buff, win_buf_snd, seq_to_send, loss_prob, W,pkt_fly, byte_sent, dim);
        }
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &serv_addr, &len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_client);
            }
            printf("pacchetto ricevuto send_put_file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(*window_base_snd, W, temp_buff.ack)) {
                    if (temp_buff.command == DATA) {
                        rcv_ack_file_in_window(temp_buff, win_buf_snd, W, window_base_snd, pkt_fly, dim, byte_readed);
                        if (*byte_readed == dim) {
                            close_put_send_file(sockfd, serv_addr, len, temp_buff, win_buf_snd, W, loss_prob,
                                                byte_readed, window_base_snd, pkt_fly, window_base_rcv, seq_to_send,dim);
                            printf("close sendfile\n");
                            return *byte_readed;
                        }
                    }
                    else{
                        rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                    }
                }else {
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("send_put_file ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd, &serv_addr, len, temp_buff, loss_prob);
                start_timeout_timer(timeout_timer_id_client, TIMEOUT);
            } else {
                printf("ignorato pacchetto send_put_file con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_client,TIMEOUT);
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            great_alarm_client = 0;
            printf("il server non è in ascolto send_put_file\n");
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id_client);
            return *byte_readed;
        }
    }
}*/
void put_client(struct shm_sel_repeat *shm){
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd,tid_rcv;
    struct shm_snd shm_snd;
    //initialize_cond(&start_rcv);
    //initialize_cond(&all_acked);
    //initialize_cond(&buf_not_empty);
    //initialize_cond(&buf_not_full);
    if(pthread_create(&tid_rcv,NULL,put_client_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rcv\n",tid_rcv);
    shm_snd.tid=tid_rcv;
    shm_snd.shm=shm;
    if(pthread_create(&tid_snd,NULL,put_client_job,&shm_snd)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_snd\n",tid_snd);
    block_signal(SIGRTMIN+1);//il thread principale non viene interrotto dal segnale di timeout,ci sono altri thread?(waitpid ecc?)
    if(pthread_join(tid_snd,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    if(pthread_join(tid_rcv,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    //ricorda di distruggere cond e rilasciare mtx
    return;
}

