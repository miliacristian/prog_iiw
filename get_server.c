#include "basic.h"
#include "timer.h"
#include "Server.h"
#include "get_server.h"
#include "communication.h"
#include "file_lock.h"

//è stato riscontrato tutto manda il fin e termina trasmissione
void close_get_send_file( struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {//manda fin non in finestra senza sequenza e ack e chiudi
    alarm(0);
    //manda fin
    send_message(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len, temp_buff, "FIN",
                 FIN, shm->param.loss_prob);
    pthread_cancel(shm->tid);
    printf(GREEN"File %s correctly sent\n"RESET, shm->filename);
    pthread_exit(NULL);
}
//dopo aver ricevuto start inizia a mandare il file
void send_file( struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    alarm(TIMEOUT);
    while (1) {
        if (shm->pkt_fly < shm->param.window && (shm->byte_sent) < shm->dimension) {
            send_data_in_window(temp_buff, shm);
        }
        while (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, MSG_DONTWAIT, (struct sockaddr *) &shm->addr.dest_addr,
                        &shm->addr.len) !=
               -1) {//non devo bloccarmi sulla ricezione,se ne trovo uno leggo finquando posso
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        rcv_ack_file_in_window(temp_buff, shm);
                        if (shm->byte_readed == shm->dimension) {
                            //se è stato riscontrato tutto vai nello stato di chiusura connessione
                            close_get_send_file(temp_buff, shm);
                            pthread_cancel(shm->tid);
                            pthread_exit(NULL);
                        }
                    } else {
                        rcv_ack_in_window(temp_buff, shm);
                        if (shm->byte_readed == shm->dimension) {
                            //se tutti i byte sono stati riscontrati vai in chiusura
                            close_get_send_file(temp_buff, shm);
                            pthread_cancel(shm->tid);
                            pthread_exit(NULL);
                        }
                    }
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {//non ack non in finestra
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                handle_error_with_exit("Internal error\n");
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_serv = 0;
            printf(RED"Client not available,get %s\n"RESET, shm->filename);
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}
//dopo aver ricevuto il comando get manda dimensione del file e aspetta start
void wait_for_start_get(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    char *path, dim_string[15];
    int file_try_lock;
    path = generate_full_pathname(shm->filename, dir_server);
    if (path == NULL) {
        handle_error_with_exit("error in generate full path\n");
    }
    //ottieni la dimensione del file da inviare e inseriscila insieme all'MD5 dentro il pacchetto da inviare al client
    if (check_if_file_exist(path)) {
        shm->dimension = get_file_size(path);
        sprintf(dim_string, "%ld", shm->dimension);
        shm->fd = open(path, O_RDONLY);
        if (shm->fd == -1) {
            handle_error_with_exit("error in open\n");
        }
        errno=0;
        file_try_lock=file_try_lock_write(shm->fd);
        if(file_try_lock==-1){
            if(errno==EWOULDBLOCK){
                send_message_in_window(temp_buff,shm, ERROR,"not available at the moment");
                errno=0;
            }
            else {
                handle_error_with_exit("error in try_lock\n");
            }
        }
        else {
            file_unlock(shm->fd);
            calc_file_MD5(path, shm->md5_sent, shm->dimension);
            better_strcpy(temp_buff.payload, dim_string);
            better_strcat(temp_buff.payload, " ");
            better_strcat(temp_buff.payload, shm->md5_sent);
            free(path);
            send_message_in_window(temp_buff, shm, DIMENSION, dim_string);//manda pacchetto dimension
        }
    }
    else {
        send_message_in_window(temp_buff,shm, ERROR,"doesn't exist");//manda pacchetto errore
    }
    errno = 0;
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *)
                &shm->addr.dest_addr, &shm->addr.len) != -1) {//attendo risposta del client,
            // aspetto finquando non arriva la risposta o scade il timeout
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN) {//se ricevi fin manda fin_ack e termina i 2 thread
                send_message(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len, temp_buff,
                             "FIN_ACK", FIN_ACK, shm->param.loss_prob);
                alarm(0);
                printf(GREEN "Request completed\n"RESET);
                pthread_cancel(shm->tid);
                pthread_exit(NULL);
            } else if (temp_buff.command == START) {//se ricevi start vai nello stato di send_file
                rcv_msg_send_ack_in_window(temp_buff, shm);
                send_file(temp_buff,shm);
                if (close(shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                pthread_cancel(shm->tid);
                pthread_exit(NULL);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {//se è non akc e non in finestra
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            }  else {
                handle_error_with_exit("Internal error\n");
            }
        } else if (errno != EINTR && errno != 0) {//se è scaduto il timer termina i 2 thread della trasmissione
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf(RED"Client not available,get %s\n"RESET, shm->filename);
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}
//thread trasmettitore e ricevitore
void *get_server_job(void *arg) {
    struct shm_sel_repeat *shm = arg;
    struct temp_buffer temp_buff;
    wait_for_start_get(temp_buff,shm);
    return NULL;
}


void get_server(struct shm_sel_repeat *shm) {//crea i 2 thread:
    //trasmettitore,ricevitore;
    //ritrasmettitore
    pthread_t tid_snd, tid_rtx;
    if (pthread_create(&tid_rtx, NULL, rtx_job, shm) != 0) {
        handle_error_with_exit("error in create thread get_server_rtx\n");
    }
    shm->tid = tid_rtx;
    if (pthread_create(&tid_snd, NULL, get_server_job, shm) != 0) {
        handle_error_with_exit("error in create thread get_server_\n");
    }
    block_signal(SIGALRM);//il thread principale non viene interrotto dal segnale di timeout
    //il thread principale aspetta che i 2 thread finiscano i compiti
    if (pthread_join(tid_snd, NULL) != 0) {
        handle_error_with_exit("error in pthread_join\n");
    }
    if (pthread_join(tid_rtx, NULL) != 0) {
        handle_error_with_exit("error in pthread_join\n");
    }
    unlock_signal(SIGALRM);
    return;
}
//ricevuto messaggio get filename
void execute_get(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    shm->filename = malloc(sizeof(char) * (MAXPKTSIZE - OVERHEAD));
    if (shm->filename == NULL) {
        handle_error_with_exit("error in malloc\n");
    }
    rcv_msg_send_ack_in_window(temp_buff, shm);
    better_strcpy(shm->filename, temp_buff.payload + 4);
    get_server(shm);
    if (shm->fd != -1) {
        if (close(shm->fd) == -1) {
            handle_error_with_exit("error in close file\n");
        }
    }
    return;
}