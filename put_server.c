#include "basic.h"
#include "parser.h"
#include "timer.h"
#include "Server.h"
#include "communication.h"
#include "put_server.h"
#include "dynamic_list.h"
#include "file_lock.h"
//dopo aver ricevuto tutto il file mettiti in ricezione del fin,manda fin_ack e termina i 2 thread
void wait_for_fin_put(struct shm_sel_repeat *shm) {
    struct temp_buffer temp_buff;
    alarm(2);//chiusura temporizzata
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE,0, (struct sockaddr *) &shm->addr.dest_addr,
                     &shm->addr.len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo,bloccante o non bloccante??
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN) {//se ricevi fin manda fin_ack solo una volta e termina sia i thread sia la trasmissione
                alarm(0);
                send_message(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len, temp_buff, "FIN_ACK", FIN_ACK,
                             shm->param.loss_prob);
                check_md5(shm->filename, shm->md5_sent, shm->dimension);
                pthread_cancel(shm->tid);
                file_unlock(shm->fd);
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if(temp_buff.command==DATA){
                        handle_error_with_exit("errore in ack wait for fin\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {//non ack non in finestra
                rcv_msg_re_send_ack_in_window(temp_buff,shm);
                alarm(TIMEOUT);
            } else {
               handle_error_with_exit("Internal error\n");
            }
        }
        if (errno != EINTR && errno != 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_serv = 0;
            alarm(0);
            check_md5(shm->filename, shm->md5_sent, shm->dimension);
            pthread_cancel(shm->tid);
            file_unlock(shm->fd);
            pthread_exit(NULL);
        }
    }
}

void rcv_put_file(struct shm_sel_repeat *shm) {
    //dopo aver ricevuto messaggio di put manda messaggio di start e si mette in ricezione dello start
    struct temp_buffer temp_buff;
    alarm(TIMEOUT);
    if (shm->fd != -1) {
        send_message_in_window(temp_buff,shm, START,"START");//invia start
    } else {
        send_message_in_window(temp_buff,shm, ERROR,"ERROR" );//invia errore
    }
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) &shm->addr.dest_addr,
                     &shm->addr.len) != -1) {
            //bloccante o non bloccante??
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN) {//se ricevi fin manda fin_ack solo una volta
                // e termina sia thread sia trasmissione
                send_message(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len, temp_buff,
                             "FIN_ACK", FIN_ACK, shm->param.loss_prob);
                alarm(0);
                printf(GREEN "Request completed\n"RESET);
                pthread_cancel(shm->tid);
                file_unlock(shm->fd);
                pthread_exit(NULL);
            }
            else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if(temp_buff.command==DATA){
                        handle_error_with_exit("errore in ack rcv_put_file\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se non è ack e non è in finestra
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else if (seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {//se non è ack ed è in finestra
                if (temp_buff.command == DATA) {
                    rcv_data_send_ack_in_window(temp_buff, shm);
                    if ((shm->byte_written) == (shm->dimension)) {//dopo aver ricevuto tutto il file aspetta il fin
                        wait_for_fin_put(shm);
                        return ;
                    }
                } else {
                   handle_error_with_exit(RED "ricevuto messaggio speciale in finestra durante ricezione file\n"RESET);
                }
                alarm(TIMEOUT);
            } else {
               handle_error_with_exit("Internal error\n");
            }
        }
        if (errno != EINTR && errno != 0) {//aggiungere altri controlli
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            printf(RED"Client is not available,request put %s\n"RESET,shm->filename);
            great_alarm_serv = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            file_unlock(shm->fd);
            pthread_exit(NULL);
        }
    }
}

//thread trasmettitore e ricevitore
void *put_server_job(void *arg) {
    struct shm_sel_repeat *shm = arg;
    rcv_put_file(shm);
    return NULL;
}

void put_server(struct shm_sel_repeat *shm) {//crea i 2 thread:
    //trasmettitore,ricevitore;
    //ritrasmettitore
    pthread_t tid_snd, tid_rtx;
    if (pthread_create(&tid_rtx, NULL, rtx_job, shm) != 0) {
        handle_error_with_exit("error in create thread put_server_rtx\n");
    }
    shm->tid = tid_rtx;
    if (pthread_create(&tid_snd, NULL, put_server_job, shm) != 0) {
        handle_error_with_exit("error in create thread put_server\n");
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

//ricevuto pacchetto con put dimensione e filename
void execute_put(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
    //verifica prima che il file esiste(con filename dentro temp_buffer)
    // ,manda start e si mette in ricezione del file,
    char *path, *first, *payload;
    payload = malloc(sizeof(char) * (MAXPKTSIZE - OVERHEAD));
    if (payload == NULL) {
        handle_error_with_exit("error in payload\n");
    }
    //estrai dal pacchetto filename e dimensione
    better_strcpy(payload, temp_buff.payload);
    first = payload;
    shm->dimension = parse_long_and_move(&payload);
    payload++;
    better_strncpy(shm->md5_sent, payload, MD5_LEN);
    shm->md5_sent[MD5_LEN] = '\0';
    payload += MD5_LEN;
    payload++;
    lock_sem(shm->mtx_file);
    path = generate_multi_copy(dir_server, payload);
    shm->filename = malloc(sizeof(char) * MAXFILENAME);
    if (shm->filename == NULL) {
        handle_error_with_exit("error in malloc\n");
    }
    if (path != NULL) {
        better_strcpy(shm->filename, path);
        shm->fd = open(path, O_WRONLY | O_CREAT, 0666);
        if (shm->fd == -1) {
            handle_error_with_exit("error in open\n");
        }
        file_lock_write(shm->fd);
        free(path);
    } else {
        shm->fd = -1;
    }
    unlock_sem(shm->mtx_file);
    free(first);
    payload = NULL;
    rcv_msg_send_ack_in_window(temp_buff, shm);//invio ack della put
    put_server(shm);
    if (shm->fd != -1) {
        file_unlock(shm->fd);
        if (close(shm->fd) == -1) {
            handle_error_with_exit("error in close file\n");
        }
    }
    return;
}