#include "basic.h"
#include "parser.h"
#include "timer.h"
#include "Client.h"
#include "get_client.h"
#include "communication.h"
#include "dynamic_list.h"
//dopo aver ricevuto messaggio di errore aspetta messaggio di fin_ack
int close_connection_get(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    printf("close connection_get\n");
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
                printf("thread cancel close connection\n");
                printf(RED "file not exist\n" RESET);
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        handle_error_with_exit("impossibile ricevere dati dopo aver ricevuto messaggio errore\n");
                    } else {
                        rcv_ack_in_window(temp_buff, shm);
                    }
                } else {
                    printf("close_connect_get ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se non è un ack e se non è in finestra
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                printf("ignorato close_connect_get pacchetto con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command, temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            printf("il server non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel close connection_get\n");
            printf(RED "file not exist\n" RESET);
            pthread_exit(NULL);
        }
    }
}

int wait_for_fin_get(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    char *path;
    printf("wait for fin\n");
    path = generate_full_pathname(shm->filename, dir_client);
    if (path == NULL) {
        handle_error_with_exit("error in generate full pathname\n");
    }
    alarm(TIMEOUT);//chiusura temporizzata
    errno = 0;
    while (1) {
        if (recvfrom(shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm->addr.dest_addr, &shm->addr.len) !=
            -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            print_rcv_message(temp_buff);
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            if (temp_buff.command == FIN) {//se ricevi fin termina i 2 thread
                alarm(0);
                check_md5(path, shm->md5_sent);
                pthread_cancel(shm->tid);
                printf("thread cancel wait for fin\n");
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è un ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        handle_error_with_exit("errore ack wait for fin\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    printf("wait for fin get ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se non è ack e non è in finestra
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command, temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            printf(BLUE"FIN non ricevuto\n"RESET);
            great_alarm_client = 0;
            alarm(0);
            check_md5(path, shm->md5_sent);
            pthread_cancel(shm->tid);
            printf("thread cancel wait for fin\n");
            pthread_exit(NULL);
        }
    }
}

int rcv_get_file(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    alarm(TIMEOUT);
    send_message_in_window(temp_buff, shm, START, "START");
    printf("messaggio start inviato\n");
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
                    printf("rcv_get_file ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se non è ack e non è in finestra
                rcv_msg_re_send_ack_command_in_window(temp_buff, shm);
                alarm(TIMEOUT);
            } else if (seq_is_in_window(shm->window_base_rcv, shm->param.window, temp_buff.seq)) {
                //se nonè ack ed è in finestra
                if (temp_buff.command == DATA) {
                    rcv_data_send_ack_in_window(temp_buff, shm);
                    if (shm->byte_written == shm->dimension) {
                        wait_for_fin_get(temp_buff, shm);
                        printf("return rcv file 1\n");
                        return shm->byte_written;
                    }
                } else {
                    printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                    handle_error_with_exit(RED "ricevuto messaggio speciale in finestra durante ricezione file\n"RESET);

                }
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto rcv get file con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command, temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            printf("il server non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel rcv get file\n");
            pthread_exit(NULL);
        }
    }
}

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
                rcv_msg_send_ack_command_in_window(temp_buff, shm);
                close_connection_get(temp_buff, shm);
            } else if (temp_buff.command == DIMENSION) {//se ricevi dimensione del file vai nello stato di rcv_file
                path = generate_multi_copy(dir_client, shm->filename);
                if (path == NULL) {
                    handle_error_with_exit("error:there are too much copies of the file\n");
                }
                shm->fd = open(path, O_WRONLY | O_CREAT, 0666);
                if (shm->fd == -1) {
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                payload = malloc(sizeof(char) * (MAXPKTSIZE - OVERHEAD));
                if (payload == NULL) {
                    handle_error_with_exit("error in malloc\n");
                }
                copy_buf2_in_buf1(payload, temp_buff.payload, MAXPKTSIZE - OVERHEAD);
                first = payload;
                shm->dimension = parse_integer_and_move(&payload);
                payload++;
                better_strncpy(shm->md5_sent, payload, MD5_LEN);
                shm->md5_sent[MD5_LEN] = '\0';
                rcv_msg_send_ack_command_in_window(temp_buff, shm);
                free(first);
                rcv_get_file(temp_buff, shm);
                if (close(shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                pthread_cancel(shm->tid);
                printf("thread cancel wait_for_get_dimension\n");
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {//se è ack
                if (seq_is_in_window(shm->window_base_snd, shm->param.window, temp_buff.ack)) {//se è in finestra
                    if (temp_buff.command == DATA) {
                        handle_error_with_exit("error ack wait for get dimension\n");
                    }
                    rcv_ack_in_window(temp_buff, shm);
                } else {
                    printf("wait get_dim ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto wait_for_get_dimension con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command, temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {//se è scaduto il timer termina i 2 thread della trasmissione
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il server non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm->tid);
            printf("thread cancel wait for get dimension\n");
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
//thread ritrasmettitore
void *get_client_rtx_job(void *arg) {
    printf("thread rtx creato\n");
    struct shm_sel_repeat *shm=arg;
    struct temp_buffer temp_buff;
    struct node*node=NULL;
    long timer_ns_left;
    char to_rtx;
    struct timespec sleep_time, rtx_time;
    block_signal(SIGALRM);//il thread rtx non viene bloccato dal segnale di timeout
    node = alloca(sizeof(struct node));
    for(;;) {
        lock_mtx(&(shm->mtx));
        while (1) {
            if(delete_head(&shm->head,node)==-1){//rimuovi nodo dalla lista,
                // se lista vuota aspetta sulla condizione
                wait_on_a_condition(&(shm->list_not_empty),&shm->mtx);
            }
            else{
                if(!to_resend(shm, *node)){//se non è da ritrasmettere rimuovi un altro nodo dalla lista
                    continue;
                }
                else{
                    //se è da ritrasmettere memorizza il nodo e continua dopo il while
                    break;
                }
            }
        }
        unlock_mtx(&(shm->mtx));
        timer_ns_left=calculate_time_left(*node);//calcola quanto tempo manca per far scadere il timeout
        if(timer_ns_left<=0){//tempo già scaduto ,verifica ancora se è da ritrasmettere
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                //se non è da ritrasmettere togli un altro nodo dalla lista
                continue;
            }
            else{
                //è da ritrasmettere immediatamente
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap=node->lap;
                lock_mtx(&(shm->mtx));
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command=shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd,&temp_buff,&shm->addr.dest_addr,shm->addr.len,shm->param.loss_prob);
                if(clock_gettime(CLOCK_MONOTONIC, &rtx_time)!=0){
                    handle_error_with_exit("error in get_time\n");
                }
                //dopo averlo ritrasmesso viene riaggiunto alla lista dei pacchetti inviati
                insert_ordered(node->seq,node->lap,rtx_time,shm->param.timer_ms,&shm->head,&shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
        else{
            //se timer>0 dormi per tempo rimanente poi riverifica se è da mandare
            sleep_struct(&sleep_time, timer_ns_left);
            nanosleep(&sleep_time , NULL);
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend(shm, *node);
            unlock_mtx(&(shm->mtx));
            if(!to_rtx){
                continue;
            }
            else{
                //pacchetto è da ritrasmettere,ritrasmettilo e rinseriscilo nella lista dinamica
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
    }
    return NULL;
}

void get_client(struct shm_sel_repeat *shm) {//crea i 2 thread:
    //trasmettitore,ricevitore;
    //ritrasmettitore
    pthread_t tid_snd, tid_rtx;
    if (pthread_create(&tid_rtx, NULL, get_client_rtx_job, shm) != 0) {
        handle_error_with_exit("error in create thread get_client_rtx\n");
    }
    printf("%lu tid_rtx\n", tid_rtx);
    shm->tid = tid_rtx;
    //shm_snd.shm=shm;
    if (pthread_create(&tid_snd, NULL, get_client_job, shm) != 0) {
        handle_error_with_exit("error in create thread get_client\n");
    }
    printf("%lu tid_snd\n", tid_snd);
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
