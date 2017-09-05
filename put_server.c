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
#include "put_server.h"
#include "dynamic_list.h"

int rtx=0;

int wait_for_fin_put(struct shm_snd *shm_snd){
    printf("wait for fin\n");
    struct temp_buffer temp_buff;
    alarm(2);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff,MAXPKTSIZE,0, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo,bloccante o non bloccante??
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto wait for fin con ack %d seq %d command %d\n"RESET, temp_buff.ack, temp_buff.seq,temp_buff.command);
            if (temp_buff.command==FIN){
                alarm(0);
                send_message(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,"FIN_ACK",FIN_ACK,shm_snd->shm->param.loss_prob);
                printf(GREEN "FIN ricevuto\n" RESET);
                check_md5(shm_snd->shm->filename,shm_snd->shm->md5_sent);
                pthread_cancel(shm_snd->tid);
                pthread_exit(NULL);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly, shm_snd->shm);
                }
                else{
                    printf("wait for fin ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv,shm_snd->shm->param.window,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            }
            else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
            }
        }
        if(errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_serv = 0;
            alarm(0);
            check_md5(shm_snd->shm->filename,shm_snd->shm->md5_sent);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
}
void rcv_ack_error(struct shm_snd *shm_snd){
    struct temp_buffer temp_buff;
    alarm(2);
    for(;;) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {
            printf(MAGENTA"pacchetto ricevuto rcv put file con ack %d seq %d command %d lap %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command,temp_buff.lap);
            if (temp_buff.command == ERROR) {
                alarm(0);
                pthread_cancel(shm_snd->tid);
                printf("thread cancel put client\n");
                pthread_exit(NULL);
            }
        }
        if (great_alarm_serv == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_serv = 0;
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
    return;
}

int rcv_put_file(struct shm_snd *shm_snd){
    //in questo stato posso ricevere put(fuori finestra),ack start(in finestra),parti di file
    struct temp_buffer temp_buff;
    alarm(TIMEOUT);
    if(shm_snd->shm->fd!=-1) {
        send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_snd, "START", START, &shm_snd->shm->seq_to_send, shm_snd->shm->param.loss_prob, shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, shm_snd->shm);
    }
    else{
        send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_snd, "ERROR", ERROR, &shm_snd->shm->seq_to_send, shm_snd->shm->param.loss_prob, shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, shm_snd->shm);
        //chiusura temporizzata,è esagerato mandare solo errore e terminare?
        rcv_ack_error(shm_snd);
    }
    errno=0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff,MAXPKTSIZE,0, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {
            //bloccante o non bloccante??
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto rcv put file con ack %d seq %d command %d lap %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command,temp_buff.lap);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(shm_snd->shm->window_base_snd,shm_snd->shm->param.window, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly, shm_snd->shm);
                }
                else{
                    printf("rcv put file ack duplicato\n");
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            }
            else if(seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window,temp_buff.seq)){
                if(temp_buff.command==DATA){
                    rcv_data_send_ack_in_window(shm_snd->shm->addr.sockfd,shm_snd->shm->fd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_rcv,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,shm_snd->shm->dimension,&shm_snd->shm->byte_written);
                    if((shm_snd->shm->byte_written)==(shm_snd->shm->dimension)){
                        wait_for_fin_put(shm_snd);
                        printf("return rcv file\n");
                        return shm_snd->shm->byte_written;
                    }
                }
                else{
                    printf("errore rcv put file\n");
                    printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                    handle_error_with_exit("");
                }
                alarm(TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
            }
        }
        if(errno != EINTR && errno != 0){//aggiungere altri controlli
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_serv = 0;
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
}

void*put_server_rtx_job(void*arg){
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
void*put_server_job(void*arg){
    struct shm_snd *shm_snd=arg;
    rcv_put_file(shm_snd);
    return NULL;
}
void put_server(struct shm_sel_repeat *shm){
    pthread_t tid_snd,tid_rtx;
    struct shm_snd shm_snd;
    if(pthread_create(&tid_rtx,NULL,put_server_rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rcv\n",tid_rtx);
    shm_snd.tid=tid_rtx;
    shm_snd.shm=shm;
    if(pthread_create(&tid_snd,NULL,put_server_job,&shm_snd)!=0){
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

//ricevuto pacchetto con put dimensione e filename
int execute_put(struct shm_sel_repeat*shm,struct temp_buffer temp_buff){
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    char*path,*first,*payload;
    payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD));
    if(payload==NULL){
        handle_error_with_exit("error in payload\n");
    }
    better_strcpy(payload,temp_buff.payload);
    first=payload;
    shm->dimension=parse_integer_and_move(&payload);
    payload++;
    strncpy(shm->md5_sent,payload,MD5_LEN);
    shm->md5_sent[MD5_LEN]='\0';
    printf("md5 %s\n",shm->md5_sent);
    payload+=MD5_LEN;
    payload++;
    path=generate_multi_copy(dir_server,payload);
    shm->filename=malloc(sizeof(char)*MAXFILENAME);
    if(shm->filename==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    if(path!=NULL) {
        better_strcpy(shm->filename,path);
        shm->fd = open(path, O_WRONLY | O_CREAT, 0666);
        if (shm->fd == -1) {
            handle_error_with_exit("error in open\n");
        }
        free(path);
    }
    else{
        shm->fd=-1;
    }
    free(first);
    payload=NULL;
    rcv_msg_send_ack_command_in_window(shm->addr.sockfd,&shm->addr.dest_addr,shm->addr.len, temp_buff,shm->win_buf_rcv,&shm->window_base_rcv,shm->param.loss_prob,shm->param.window);//invio ack della put
    put_server(shm);
    if(shm->fd!=-1) {
        if (close(shm->fd) == -1) {
            handle_error_with_exit("error in close file\n");
        }
    }
    printf("return execute put\n");
    return shm->byte_written;
}