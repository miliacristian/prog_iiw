#include "basic.h"
#include "parser.h"
#include "timer.h"
#include "Client.h"
#include "get_client.h"
#include "communication.h"
#include "dynamic_list.h"
#include "file_lock.h"
//dopo aver ricevuto messaggio di errore aspetta messaggio di fin_ack
int close_connection_get(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    send_message_in_window(temp_buff, shm, FIN, "FIN");//manda messaggio di fin
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
            if (temp_buff.command == FIN_ACK) {//se ricevi fin_ack termina i 2 thread
                alarm(0);
                pthread_cancel(shm->tid);
                printf(RED "File %s not available\n"RESET,shm->filename);
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        handle_error_with_exit("impossibile ricevere dati dopo aver ricevuto messaggio errore\n");
                    } else {
                        rcv_ack_in_window(temp_buff, shm);
                    }
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se non è un ack e se non è in finestra
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
            printf(RED "File %s not available\n"RESET,shm->filename);
            pthread_exit(NULL);
        }
    }
}
//è stato ricevuto tutto il file aspetta il fin per chiudere la connessione
int wait_for_fin_get(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    char *path;
    path = generate_full_pathname(shm->filename, dir_client);
    if (path == NULL) {
        handle_error_with_exit("error in generate full pathname\n");
    }
    alarm(TIMEOUT);//chiusura temporizzata
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) != -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN) {//se ricevi fin termina i 2 thread
                alarm(0);
                check_md5(path, shm->md5_sent, shm->dimension);
                pthread_cancel(shm->tid);
                file_unlock(shm->fd);
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        handle_error_with_exit("errore ack wait for fin\n");
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
            } else {
                handle_error_with_exit("Internal error\n");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_client = 0;
            alarm(0);
            check_md5(path, shm->md5_sent,shm->dimension);
            pthread_cancel(shm->tid);
            file_unlock(shm->fd);
            pthread_exit(NULL);
        }
    }
}
//ricevuta dimensione del file,manda messaggio di start per far capire al sender che è pronto a ricevere i dati
int rcv_get_file(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    alarm(TIMEOUT);
    send_message_in_window(temp_buff, shm, START, "START");
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se non è ack e non è in finestra
                rcv_msg_re_send_ack_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else if (seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se nonè ack ed è in finestra
                if (temp_buff.command == DATA) {
                    rcv_data_send_ack_in_window(temp_buff, shm);
                    if (shm->byte_written == shm->dimension) {//dopo aver ricevuto tutto il file aspetta il fin
                        wait_for_fin_get(temp_buff, shm);
                        return shm->byte_written;
                    }
                } else {
                    handle_error_with_exit(RED "ricevuto messaggio speciale in finestra durante ricezione file\n"RESET);

                }
                alarm(TIMEOUT);
            } else {
                handle_error_with_exit("Internal error\n");
            }
        } else if (errno != EINTR && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_client = 0;
            alarm(0);
            printf(RED"Server is not available,request get %s\n"RESET, shm->filename);
            pthread_cancel(shm->tid);
            file_unlock(shm->fd);
            pthread_exit(NULL);
        }
    }
}
//manda messaggio get e aspetta messaggio contentente la dimensione del file
int wait_for_get_dimension(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    errno = 0;
    char *path, *first, *payload;
    better_strcpy(temp_buff.payload, "get ");
    better_strcat(temp_buff.payload, shm->filename);
    send_message_in_window(temp_buff, shm, GET, temp_buff.payload);//manda messaggio get
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo risposta del server
            //mi blocco sulla risposta del server
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == ERROR) {//se ricevi errore vai nello stato di chiusura connessione
                rcv_msg_send_ack_in_window(temp_buff, shm);
                close_connection_get(temp_buff, shm);
            } else if (temp_buff.command == DIMENSION) {//se ricevi dimensione del file vai nello stato di rcv_file
                lock_sem(shm->mtx_file);
                path = generate_multi_copy(dir_client, shm->filename);
                if (path == NULL) {
                    handle_error_with_exit("error:there are too much copies of the file\n");
                }
                shm->fd = open(path, O_WRONLY | O_CREAT, 0666);
                if (shm->fd == -1) {
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                file_lock_write(shm->fd);
                unlock_sem(shm->mtx_file);
                payload = malloc(sizeof(char) * (MAXPKTSIZE - OVERHEAD));
                if (payload == NULL) {
                    handle_error_with_exit("error in malloc\n");
                }
                copy_buf2_in_buf1(payload, temp_buff.payload, MAXPKTSIZE - OVERHEAD);
                first = payload;
                shm->dimension = parse_long_and_move(&payload);
                payload++;
                better_strncpy(shm->md5_sent, payload, MD5_LEN);
                shm->md5_sent[MD5_LEN] = '\0';
                rcv_msg_send_ack_in_window(temp_buff, shm);
                free(first);
                rcv_get_file(temp_buff, shm);
                if (close(shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                pthread_cancel(shm->tid);
                file_unlock(shm->fd);
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        handle_error_with_exit("error ack wait for get dimension\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    //ack duplicato
                }
                alarm(TIMEOUT);
            } else {
                handle_error_with_exit("Internal error\n");
            }
        } else if (errno != EINTR) {//se è scaduto il timer termina i 2 thread della trasmissione
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf(RED"Server is not available,request get %s\n"RESET, shm->filename);
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}

//thread trasmettitore e ricevitore
void *get_client_job(void *arg) {
    struct shm_sel_repeat *shm = arg;
    struct temp_buffer temp_buff;
    wait_for_get_dimension(temp_buff, shm);
    return NULL;
}


void get_client(struct shm_sel_repeat *shm) {//crea i 2 thread:
    //trasmettitore,ricevitore;
    //ritrasmettitore
    pthread_t tid_snd, tid_rtx;
    if (pthread_create(&tid_rtx, NULL, rtx_job, shm) != 0) {
        handle_error_with_exit("error in create thread get_client_rtx\n");
    }
    shm->tid = tid_rtx;
    //shm_snd.shm=shm;
    if (pthread_create(&tid_snd, NULL, get_client_job, shm) != 0) {
        handle_error_with_exit("error in create thread get_client\n");
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
