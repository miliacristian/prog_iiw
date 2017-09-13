#include "basic.h"
#include "timer.h"
#include "Client.h"
#include "communication.h"
#include "put_client.h"
#include "dynamic_list.h"
#include "file_lock.h"

//entra qui dopo aver ricevuto il messaggio di errore
int close_connection_put(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    send_message_in_window(temp_buff,
                           shm, FIN, "FIN");//manda messaggio di fin
    alarm(TIMEOUT);
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo fin_ack dal server
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {//
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN_ACK) {//se ricevi fin_ack termina thread e trasmissione
                alarm(0);
                pthread_cancel(shm->tid);
                printf(RED "Request put %s not available at moment\n"RESET,shm->filename);
                pthread_exit(NULL);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if(temp_buff.command==DATA){
                        handle_error_with_exit("error in close connection\n");//impossibile ricevere dati dopo aver ricevuto errore
                    }
                    else {
                        rcv_ack_in_window(temp_buff, shm);
                    }
                }
                else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            }  else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                handle_error_with_exit("Internal error\n");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf(RED "Request put %s not available at moment\n"RESET,shm->filename);
            pthread_exit(NULL);
        }
    }
}
//entra qui quando hai riscontrato tutti i pacchetti
int close_put_send_file(struct shm_sel_repeat *shm){
    //in questo stato ho ricevuto tutti gli ack (compreso l'ack della put),posso ricevere ack duplicati,FIN_ACK,start(fuori finestra)
    struct temp_buffer temp_buff;
    alarm(TIMEOUT);
    send_message_in_window(temp_buff, shm, FIN, "FIN");
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE,0, (struct sockaddr *) &(shm->addr.dest_addr), &shm->addr.len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            print_rcv_message(temp_buff);
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            if (temp_buff.command == FIN_ACK) {//se ricevi fin_ack termina i 2 thread e l'intera trasmissione
                alarm(0);
                printf(GREEN"File %s correctly sent\n"RESET,shm->filename);
                pthread_cancel(shm->tid);
                pthread_exit(NULL);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd,shm->param.window, temp_buff.ack)) {//se è in finestra
                    handle_error_with_exit("Internal error\n");
                }
                else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm->window_base_rcv,shm->param.window, temp_buff.seq)) {
                //se ènon ack non in finestra
                rcv_msg_re_send_ack_in_window( temp_buff,shm);
                alarm(TIMEOUT);
            }
            else {
                handle_error_with_exit("Internal error\n");
            }
        }
        if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_client = 0;
            printf(GREEN"File %s correctly sent\n"RESET,shm->filename);
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}
int send_put_file(struct shm_sel_repeat *shm) {//invia file con protocollo selective repeat,
// quando riesce a riscontrare tutto va nello stato di chiusura
    struct temp_buffer temp_buff;
    alarm(TIMEOUT);
    while (1) {
        //finquando pkt_fly <W e byte_sent <dimensione del file puoi mandare  un pacchetto file
        if (((shm->pkt_fly) < (shm->param.window)) && ((shm->byte_sent) < (shm->dimension))) {
            send_data_in_window(temp_buff, shm);
        }
        while(recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE, MSG_DONTWAIT, (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) != -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            print_rcv_message(temp_buff);
            if(temp_buff.command==SYN || temp_buff.command==SYN_ACK){
                continue;//ignora pacchetto
            }
            else{
                alarm(0);
            }
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd,shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        rcv_ack_file_in_window(temp_buff, shm);
                        if ((shm->byte_readed) ==(shm->dimension)) {
                            close_put_send_file(shm);
                            return shm->byte_readed;
                        }
                    }
                    else{
                        rcv_ack_in_window(temp_buff, shm);
                        if ((shm->byte_readed) ==(shm->dimension)) {
                            close_put_send_file(shm);
                            return shm->byte_readed;
                        }
                    }
                }
                else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            }
            else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se non è ack ed è fuori finestra
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
               handle_error_with_exit("Internal error\n");
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_client = 0;
            printf(RED"Server is not available,request put %s\n"RESET,shm->filename);
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}

//thread trasmettitore e ricevitore
void *put_client_job(void*arg){
    struct shm_sel_repeat *shm=arg;
    struct temp_buffer temp_buff;
    char dim_string[15];
    sprintf(dim_string, "%ld", shm->dimension);
    better_strcpy(temp_buff.payload,dim_string);
    better_strcat(temp_buff.payload," ");
    better_strcat(temp_buff.payload,shm->md5_sent);
    better_strcat(temp_buff.payload," ");
    better_strcat(temp_buff.payload,shm->filename);
    //invia messaggio put

    send_message_in_window(temp_buff, shm,PUT,temp_buff.payload);
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff,MAXPKTSIZE,0, (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) != -1) {//attendo risposta del server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            print_rcv_message(temp_buff);
            if (temp_buff.command == START) {//se riceve start va nello stato di send_file
                rcv_msg_send_ack_in_window( temp_buff,shm);
                send_put_file(shm);
                if (close(shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                return NULL;
            }
            if(temp_buff.command==ERROR) {//se riceve errore va nello stato di fine connessione
                rcv_msg_send_ack_in_window(temp_buff,shm);
                close_connection_put(temp_buff, shm);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm->window_base_snd,shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm);
                }
                else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else {
                handle_error_with_exit("Internal error\n");
            }
        }
        if (errno != EINTR && errno != 0 && errno!=EAGAIN && errno!=EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            printf(RED"Server is not available,request put %s\n"RESET,shm->filename);
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
    return NULL;
}

void put_client(struct shm_sel_repeat *shm){//crea i 2 thread:
    //trasmettitore,ricevitore;
    //ritrasmettitore
    pthread_t tid_snd,tid_rtx;
    if(pthread_create(&tid_rtx,NULL,rtx_job,shm)!=0){
        handle_error_with_exit("error in create thread put_client_rtx\n");
    }
    shm->tid=tid_rtx;
    if(pthread_create(&tid_snd,NULL,put_client_job,shm)!=0){
        handle_error_with_exit("error in create thread put_client\n");
    }
    block_signal(SIGALRM);//il thread principale non viene interrotto dal segnale di timeout
    //il thread principale aspetta che i 2 thread finiscano i compiti
    if(pthread_join(tid_snd,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    if(pthread_join(tid_rtx,NULL)!=0){
        handle_error_with_exit("error in pthread_join\n");
    }
    unlock_signal(SIGALRM);
    return;
}

