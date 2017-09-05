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

int rtx_get_server=0;
int close_get_send_file(int sockfd, struct sockaddr_in cli_addr, socklen_t len, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int W, double loss_prob, int *byte_readed,struct shm_snd*shm_snd) {//manda fin non in finestra senza sequenza e ack e chiudi
    alarm(0);
    send_message(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff, "FIN", FIN, shm_snd->shm->param.loss_prob);
    printf("close get send_file\n");
    pthread_cancel(shm_snd->tid);
    printf("thread cancel \n");
    pthread_exit(NULL);
}

int send_file(int sockfd, struct sockaddr_in cli_addr, socklen_t len, int *seq_to_send, int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly, struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int fd, int *byte_readed, int dim, double loss_prob,struct shm_snd*shm_snd) {
    printf("send_file\n");
    int ack_dup=0;
    int value = 0,*byte_sent = &value;
    alarm(TIMEOUT);
    while (1) {
        printf("pkt fly %d byte sent %d dim %d\n",shm_snd->shm->pkt_fly,shm_snd->shm->byte_sent,shm_snd->shm->dimension);
        if (shm_snd->shm->pkt_fly < shm_snd->shm->param.window && (shm_snd->shm->byte_sent) < shm_snd->shm->dimension) {
            printf("entrato\n");
            send_data_in_window(shm_snd->shm->addr.sockfd, shm_snd->shm->fd,
                                &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len,
                                temp_buff, shm_snd->shm->win_buf_snd, &shm_snd->shm->seq_to_send,
                                shm_snd->shm->param.loss_prob, shm_snd->shm->param.window,&shm_snd->shm->pkt_fly, &shm_snd->shm->byte_sent,
                                shm_snd->shm->dimension,shm_snd->shm);
        }
        if (recvfrom(sockfd, &temp_buff,MAXPKTSIZE, MSG_DONTWAIT, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf("pacchetto ricevuto send_file con ack %d seq %d command %d win_base_snd %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command,shm_snd->shm->window_base_snd);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    if(temp_buff.command==DATA) {
                        rcv_ack_file_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                               &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly, shm_snd->shm->dimension,&shm_snd->shm->byte_readed,shm_snd->shm);
                        printf("byte readed %d ack dup %d\n",shm_snd->shm->byte_readed,ack_dup);
                        if (shm_snd->shm->byte_readed == shm_snd->shm->dimension) {
                            close_get_send_file(shm_snd->shm->addr.sockfd, shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window, shm_snd->shm->param.loss_prob,&shm_snd->shm->byte_readed,shm_snd);
                            printf("close sendfile\n");
                            pthread_cancel(shm_snd->tid);
                            printf("thread cancel\n");
                            pthread_exit(NULL);
                        }
                    }
                    else{
                        rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,
                                          &shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly,shm_snd->shm);
                    }
                }
                else {
                    printf("send_file ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,
                                                      &shm_snd->shm->addr.dest_addr,
                                                      shm_snd->shm->addr.len, temp_buff,shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto send_file con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd, shm_snd->shm->window_base_rcv);
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
            pthread_cancel(shm_snd->tid);
            printf("thread cancel \n");
            pthread_exit(NULL);
        }
    }
}

int wait_for_start(int sockfd,struct sockaddr_in cli_addr,socklen_t len,char*filename,int *byte_written,int *seq_to_send,int *window_base_snd,int *window_base_rcv,int W,int *pkt_fly,struct temp_buffer temp_buff,struct window_rcv_buf*win_buf_rcv,struct window_snd_buf*win_buf_snd,struct shm_snd*shm_snd) {
    char*path, dim_string[11];
    printf("pkt fly %d byte sent %d dim %d\n",shm_snd->shm->pkt_fly);
    path = generate_full_pathname(shm_snd->shm->filename, dir_server);
    if (path == NULL) {
        handle_error_with_exit("error in generate full path\n");
    }
    if (check_if_file_exist(path)) {
        shm_snd->shm->dimension = get_file_size(path);
        sprintf(dim_string, "%d",shm_snd->shm->dimension);
        shm_snd->shm->fd = open(path, O_RDONLY);
        if (shm_snd->shm->fd == -1) {
            handle_error_with_exit("error in open\n");
        }
        calc_file_MD5(path,shm_snd->shm->md5_sent);
        strcpy(temp_buff.payload,dim_string);//non dovrebbe essere strcat?
        strcat(temp_buff.payload," ");
        strcat(temp_buff.payload,shm_snd->shm->md5_sent);
        free(path);
        send_message_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len,
                               temp_buff, shm_snd->shm->win_buf_snd, dim_string, DIMENSION,
                               &shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,
                               shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, shm_snd->shm);
    }
    else {
        send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                               shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_snd,
                               "il file non esiste", ERROR, &shm_snd->shm->seq_to_send,
                               shm_snd->shm->param.loss_prob, shm_snd->shm->param.window,
                               &shm_snd->shm->pkt_fly, shm_snd->shm);
    }
    errno = 0;
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0, (struct sockaddr *)
                &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf("pacchetto ricevuto execute get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd,
                                      shm_snd->shm->param.window, &shm_snd->shm->window_base_snd,
                                      &shm_snd->shm->pkt_fly,shm_snd->shm);
                } else {
                    printf("execute get ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else if (temp_buff.command == FIN) {
                send_message(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff, "FIN_ACK", FIN_ACK,shm_snd->shm->param.loss_prob);
                alarm(0);
                pthread_cancel(shm_snd->tid);
                printf("thread cancel close_put_snd\n");
                pthread_exit(NULL);
            } else if (temp_buff.command == START) {
                printf("messaggio start ricevuto\n");
                rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_rcv, &shm_snd->shm->window_base_rcv, shm_snd->shm->param.loss_prob, W);
                send_file(shm_snd->shm->addr.sockfd,shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,&shm_snd->shm->seq_to_send,&shm_snd->shm->window_base_snd,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->fd, &shm_snd->shm->byte_readed,shm_snd->shm->dimension,shm_snd->shm->param.loss_prob,shm_snd);
                if (close(shm_snd->shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                pthread_cancel(shm_snd->tid);
                printf("thread cancel close_put_snd\n");
                pthread_exit(NULL);
            } else {
                printf("ignorato pacchetto execute get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd, shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR && errno!=0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf("il client non è in ascolto wait for start\n");
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel\n");
            pthread_exit(NULL);
        }
    }
}

void *get_server_job(void*arg){
    struct shm_snd *shm_snd=arg;
    struct temp_buffer temp_buff;
    wait_for_start(shm_snd->shm->addr.sockfd,shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,shm_snd->shm->filename,&shm_snd->shm->byte_written,&shm_snd->shm->seq_to_send,&shm_snd->shm->window_base_snd,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly,temp_buff,shm_snd->shm->win_buf_rcv,shm_snd->shm->win_buf_snd,shm_snd);
    return NULL;
}
void *get_server_rtx_job(void*arg){
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
                rtx_get_server++;
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
                rtx_get_server++;
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
void get_server(struct shm_sel_repeat *shm){
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd,tid_rtx;
    struct shm_snd shm_snd;
    if(pthread_create(&tid_rtx,NULL,get_server_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rtx\n",tid_rtx);
    shm_snd.tid=tid_rtx;
    shm_snd.shm=shm;
    if(pthread_create(&tid_snd,NULL,get_server_job,&shm_snd)!=0){
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
int execute_get(struct shm_sel_repeat*shm,struct temp_buffer temp_buff) {
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    shm->filename=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD));
    if(shm->filename==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    rcv_msg_send_ack_command_in_window(shm->addr.sockfd,&shm->addr.dest_addr,shm->addr.len, temp_buff,shm->win_buf_rcv,&shm->window_base_rcv,shm->param.loss_prob,shm->param.window);
    strcpy(shm->filename,temp_buff.payload + 4);
    get_server(shm);
    if(close(shm->fd)==-1){
        handle_error_with_exit("error in close file\n");
    }
    return shm->byte_readed;
}