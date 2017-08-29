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
int wait_for_fin_put2(struct shm_snd *shm_snd){
    printf("wait for fin\n");
    struct temp_buffer temp_buff;
    //start_timeout_timer(timeout_timer_id_serv,TIMEOUT-3000);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer), MSG_DONTWAIT, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                //stop_timeout_timer(timeout_timer_id_serv);
            }
            printf("pacchetto ricevuto wait for fin con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
            if (temp_buff.command==FIN){
                //stop_timeout_timer(timeout_timer_id_serv);
                send_message(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,"FIN_ACK",FIN_ACK,shm_snd->shm->param.loss_prob);
                printf("return wait_for_fin\n");
                return shm_snd->shm->byte_written;
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly);
                }
                else{
                    //stop_timer(shm_snd->shm->win_buf_snd[temp_buff.ack].time_id);
                    printf("wait for fin ack duplicato\n");
                }
                //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv,shm_snd->shm->param.window,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->param.loss_prob);
                //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->param.loss_prob);
                //handle_error_with_exit("");
                //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
        }
        else if(errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_serv = 0;
            //stop_timeout_timer(timeout_timer_id_serv);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
}
/*int  wait_for_fin_put(struct temp_buffer temp_buff,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in cli_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob){
    //in questo stato posso ricevere ack start in finesta,fin,parti di file già ricevute(fuori finestra)
    printf("wait for fin\n");
    start_timeout_timer(timeout_timer_id_serv,TIMEOUT-3000);//chiusura temporizzata
    errno=0;
    while(1){
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_serv);
            }
            printf("pacchetto ricevuto wait for fin con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,temp_buff.command);
            if (temp_buff.command==FIN){
                stop_timeout_timer(timeout_timer_id_serv);
                stop_all_timers(win_buf_snd, W);
                send_message(sockfd,&cli_addr,len,temp_buff,"FIN_ACK",FIN_ACK,loss_prob);
                printf("return wait_for_fin\n");
                return *byte_written;
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("wait for fin ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd,&cli_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",*window_base_snd,*window_base_rcv);
                rcv_msg_re_send_ack_command_in_window(sockfd,&cli_addr,len,temp_buff,loss_prob);
                //handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
        }
        else if(errno!=EINTR && errno!=0){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_serv = 0;
            stop_all_timers(win_buf_snd, W);
            stop_timeout_timer(timeout_timer_id_serv);
            return *byte_written;
        }
    }
}*/


int rcv_put_file2(struct shm_snd *shm_snd){
    //in questo stato posso ricevere put(fuori finestra),ack start(in finestra),parti di file
    struct temp_buffer temp_buff;
    //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
    send_message_in_window_serv(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_snd,"START",START,&shm_snd->shm->seq_to_send,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,&shm_snd->shm->pkt_fly);
    printf("messaggio start inviato\n");
    errno=0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, sizeof(struct temp_buffer),0, (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) != -1) {
            //bloccante o non bloccante??
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                //stop_timeout_timer(timeout_timer_id_serv);
            }
            printf("pacchetto ricevuto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(shm_snd->shm->window_base_snd,shm_snd->shm->param.window, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,shm_snd->shm->win_buf_snd,shm_snd->shm->param.window,&shm_snd->shm->window_base_snd,&shm_snd->shm->pkt_fly);
                }
                else{
                    printf("rcv put file ack duplicato\n");
                }
                //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->param.loss_prob);
                //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else if(seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window,temp_buff.seq)){
                if(temp_buff.command==DATA){
                    rcv_data_send_ack_in_window(shm_snd->shm->addr.sockfd,shm_snd->shm->fd,&shm_snd->shm->addr.dest_addr,shm_snd->shm->addr.len,temp_buff,shm_snd->shm->win_buf_rcv,&shm_snd->shm->window_base_rcv,shm_snd->shm->param.loss_prob,shm_snd->shm->param.window,shm_snd->shm->dimension,&shm_snd->shm->byte_written);
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
                //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n",shm_snd->shm->window_base_snd,shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                //start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
        }
        else if(errno != EINTR && errno != 0){//aggiungere altri controlli
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_serv = 0;
            //stop_timeout_timer(timeout_timer_id_serv);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel put client\n");
            pthread_exit(NULL);
        }
    }
}
/*int rcv_put_file(int sockfd,struct sockaddr_in cli_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,int fd,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written){
    //in questo stato posso ricevere put(fuori finestra),ack start(in finestra),parti di file
    start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
    send_message_in_window_serv(sockfd,&cli_addr,len,temp_buff,win_buf_snd,"START",START,seq_to_send,loss_prob,W,pkt_fly);
    printf("messaggio start inviato\n");
    errno=0;
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &cli_addr, &len) != -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                stop_timeout_timer(timeout_timer_id_serv);
            }
            printf("pacchetto ricevuto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq, temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack!=NOT_AN_ACK) {
                if(seq_is_in_window(*window_base_snd, W, temp_buff.ack)){
                    rcv_ack_in_window(temp_buff,win_buf_snd,W,window_base_snd,pkt_fly);
                }
                else{
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    printf("rcv put file ack duplicato\n");
                }
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else if (!seq_is_in_window(*window_base_rcv, W,temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(sockfd,&cli_addr,len,temp_buff,loss_prob);
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else if(seq_is_in_window(*window_base_rcv, W,temp_buff.seq)){
                if(temp_buff.command==DATA){
                    rcv_data_send_ack_in_window(sockfd,fd,&cli_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W,dimension,byte_written);
                    if(*byte_written==dimension){
                        wait_for_fin_put(temp_buff,win_buf_snd,sockfd,cli_addr,len,window_base_snd,window_base_rcv,pkt_fly,W,byte_written,loss_prob);
                        printf("return rcv file\n");
                        return *byte_written;
                    }
                }
                else{
                    printf("errore rcv put file\n");
                    printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                    handle_error_with_exit("");
                    rcv_msg_send_ack_command_in_window(sockfd,&cli_addr,len,temp_buff,win_buf_rcv,window_base_rcv,loss_prob,W);
                }
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
            else {
                printf("ignorato pacchetto rcv put file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", *window_base_snd, *window_base_rcv);
                handle_error_with_exit("");
                start_timeout_timer(timeout_timer_id_serv,TIMEOUT);
            }
        }
        else if(errno!=EINTR && errno!=0){
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_serv = 0;
            stop_timeout_timer(timeout_timer_id_serv);
            return *byte_written;
        }
    }
}*/
void*put_server_rtx_job(void*arg){
    struct shm_sel_repeat *shm=arg;
    block_signal(SIGRTMIN+1);
    while(1){}
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
    block_signal(SIGRTMIN+1);//il thread principale non viene interrotto dal segnale di timeout,ci sono altri thread?(waitpid ecc?)
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
    printf("dimensione del file put %d\n",shm->dimension);
    payload++;
    path=generate_multi_copy(dir_server,payload);
    shm->fd= open(path, O_WRONLY | O_CREAT,0666);
    if (shm->fd == -1) {
        handle_error_with_exit("error in open\n");
    }
    free(path);
    free(first);
    payload=NULL;
    rcv_msg_send_ack_command_in_window(shm->addr.sockfd,&shm->addr.dest_addr,shm->addr.len, temp_buff,shm->win_buf_rcv,&shm->window_base_rcv,shm->param.loss_prob,shm->param.window);//invio ack della put
    put_server(shm);
    if(close(shm->fd)==-1){
        handle_error_with_exit("error in close file\n");
    }
    printf("return execute put\n");
    return shm->byte_written;
}