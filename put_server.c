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

int wait_for_fin_put2(struct shm_snd *shm_snd){
    printf("wait for fin\n");
    char md5[MD5_LEN + 1];
    struct temp_buffer temp_buff;
    if(close(shm_snd->shm->fd)==-1){
        handle_error_with_exit("error in close file\n");
    }
    alarm(2);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer),0, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//attendo messaggio di fin,
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
                if(!calc_file_MD5(shm_snd->shm->filename,md5)){
                    handle_error_with_exit("error in calculate md5\n");
                }
                printf("md5 del file ricevuto %s\n",md5);
                if(strcmp(shm_snd->shm->md5_sent,md5)!=0){
                    printf(RED "file corrupted\n" RESET);
                }
                else{
                    printf(GREEN "file rightly received\n" RESET);
                }
                pthread_cancel(shm_snd->tid);
                return shm_snd->shm->byte_written;
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
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            }
        }
        if(errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK){
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

int rcv_put_file2(struct shm_snd *shm_snd){
    //in questo stato posso ricevere put(fuori finestra),ack start(in finestra),parti di file
    struct temp_buffer temp_buff;
    alarm(TIMEOUT);
    send_message_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_snd,"START",START,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly, shm_snd->shm);
    errno=0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer),0, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {
            //bloccante o non bloccante??
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            printf(MAGENTA"pacchetto ricevuto rcv put file con ack %d seq %d command %d\n"RESET, temp_buff.ack, temp_buff.seq, temp_buff.command);
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
                    rcv_data_send_ack_in_window(shm_snd->shm->addr.sockfd,shm_snd->shm->fd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_rcv,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,shm_snd->shm->dimension,&shm_snd->shm->byte_written, shm_snd->shm);
                    if((shm_snd->shm->byte_written)==(shm_snd->shm->dimension)){
                        wait_for_fin_put2(shm_snd);
                        printf("return rcv file\n");
                        return shm_snd->shm->byte_written;
                    }
                }
                else{
                    printf("errore rcv put file\n");
                    printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                    handle_error_with_exit("");
                    rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_rcv,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window);
                }
                alarm(TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
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
    struct shm_sel_repeat *shm=arg;
    struct temp_buffer temp_buff;
    struct node*node=NULL;
    long timer_ns_left;
    char to_rtx;
    struct timespec sleep_time;
    block_signal(SIGALRM);//il thread receiver non viene bloccato dal segnale di timeout
    node = alloca(sizeof(struct node));
    lock_mtx(&(shm->mtx));
    printf("lock preso\n");
    for(;;) {
        while (1) {
            if(delete_head(&shm->head,node)==-1){
                printf("before wait on cond\n");
                wait_on_a_condition(&(shm->list_not_empty),&shm->mtx);
                printf("after wait on cond\n");
            }
            else{
                if(!to_resend(shm, *node)){
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
            printf("rtx immediata\n");
            temp_buff.ack = NOT_AN_ACK;
            temp_buff.seq = node->seq;
            copy_buf1_in_buf2(temp_buff.payload,shm->win_buf_snd[node->seq].payload,MAXPKTSIZE-9);
            temp_buff.command=shm->win_buf_snd[node->seq].command;
            resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
            lock_mtx(&(shm->mtx));
            if(clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[node->seq].time))!=0){
                handle_error_with_exit("error in get_time\n");
            }
            insert_ordered(node->seq,node->lap,shm->win_buf_snd[node->seq].time,shm->param.timer_ms,&shm->head,&shm->tail);
            unlock_mtx(&(shm->mtx));
        }
        else{
            sleep_struct(&sleep_time, timer_ns_left);
            nanosleep(&sleep_time , NULL);
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                printf("no rtx dopo sleep\n");
                continue;
            }
            else{
                printf("rtx dopo sleep\n");
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                copy_buf1_in_buf2(temp_buff.payload,shm->win_buf_snd[node->seq].payload,MAXPKTSIZE-9);
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
    }
    return NULL;
}
void*put_server_job(void*arg){
    struct shm_snd *shm_snd=arg;
    rcv_put_file2(shm_snd);
    return NULL;
}
void put_server(struct shm_sel_repeat *shm){
    //initialize_cond();inizializza tutte le cond
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
    //ricorda di distruggere cond e rilasciare mtx
    return;
}

//ricevuto pacchetto con put dimensione e filename
int execute_put(struct shm_sel_repeat*shm,struct temp_buffer temp_buff){
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    char*path,*first,*payload;
    payload=malloc(sizeof(char)*(MAXPKTSIZE-9));
    if(payload==NULL){
        handle_error_with_exit("error in payload\n");
    }
    strcpy(payload,temp_buff.payload);
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
    strcpy(shm->filename,path);
    shm->fd=open(path, O_WRONLY | O_CREAT,0666);
    if (shm->fd == -1) {
        handle_error_with_exit("error in open\n");
    }
    free(path);
    free(first);
    payload=NULL;
    rcv_msg_send_ack_command_in_window(shm->addr.sockfd,&shm->addr.dest_addr,shm->addr.len, temp_buff,shm->win_buf_rcv,&shm->window_base_rcv,shm->param.loss_prob,shm->param.window);//invio ack della put
    put_server(shm);
    //if(close(shm->fd)==-1){
      //  handle_error_with_exit("error in close file\n");
    //}
    printf("return execute put\n");
    return shm->byte_written;
}