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

int rtx_list_client = 0;

int
close_connection_list(struct temp_buffer temp_buff, int *seq_to_send, struct window_snd_buf *win_buf_snd, int sockfd,
                      struct sockaddr_in serv_addr, socklen_t len, int *window_base_snd, int *window_base_rcv,
                      int *pkt_fly, int W, int *byte_written, double loss_prob,
                      struct shm_snd *shm_snd) {
    printf("close connection\n");
    send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                           shm_snd->shm->win_buf_snd, "FIN", FIN, &shm_snd->shm->seq_to_send,
                           shm_snd->shm->param.loss_prob, shm_snd->shm->param.window, &shm_snd->shm->pkt_fly,
                           shm_snd->shm);//manda messaggio di fin
    alarm(TIMEOUT);
    errno = 0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//attendo fin_ack dal server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto close connection con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly,
                                      shm_snd->shm);
                } else {
                    printf("ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (temp_buff.command == FIN_ACK) {
                alarm(0);
                printf("return close connection\n");
                pthread_cancel(shm_snd->tid);
                printf("thread cancel close connection\n");
                pthread_exit(NULL);
            } else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                      shm_snd->shm->addr.len, temp_buff, shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato close connection pacchetto con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd, shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            printf("return close connection\n");
            pthread_cancel(shm_snd->tid);
            printf("thread cancel close connection\n");
            pthread_exit(NULL);
        }
    }
}

int
wait_for_fin_list(struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int sockfd,
                  struct sockaddr_in serv_addr, socklen_t len, int *window_base_snd, int *window_base_rcv, int *pkt_fly,
                  int W, int *byte_written, double loss_prob,
                  struct shm_snd *shm_snd) {
    printf("wait for fin list\n");
    alarm(TIMEOUT);//chiusura temporizzata
    errno = 0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto wait for fin_list con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.command == FIN) {
                alarm(0);
                printf("fin ricevuto\n");
                pthread_cancel(shm_snd->tid);
                printf("thread cancel wait for fin\n");
                printf("file list:\n%s",shm_snd->shm->list);
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly,
                                      shm_snd->shm);
                } else {
                    printf("wait for fin ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                      shm_snd->shm->addr.len, temp_buff, shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d payload %s\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command, temp_buff.payload);
                printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd, shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            printf("return wait_for_fin\n");
            pthread_cancel(shm_snd->tid);
            printf("thread cancel wait for fin\n");
            printf("file list:\n%s",shm_snd->shm->list);
            pthread_exit(NULL);
        }
    }
}

int rcv_list2(int sockfd, struct sockaddr_in serv_addr, socklen_t len, struct temp_buffer temp_buff,
          struct window_snd_buf *win_buf_snd, struct window_rcv_buf *win_buf_rcv, int *seq_to_send, int W, int *pkt_fly,
          char **list, int dimension, double loss_prob, int *window_base_snd, int *window_base_rcv, int *byte_written,
          struct shm_snd *shm_snd) {
    alarm(TIMEOUT);
    send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                           shm_snd->shm->win_buf_snd, "START", START, &shm_snd->shm->seq_to_send,
                           shm_snd->shm->param.loss_prob,
                           shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, shm_snd->shm);
    printf("messaggio start inviato\n");
    printf("rcv list\n");
    errno = 0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto rcv_list2 con ack %d seq %d command %d payload %s\n", temp_buff.ack,
                   temp_buff.seq, temp_buff.command, temp_buff.payload);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly,
                                      shm_snd->shm);
                } else {
                    printf("rcv_list2 ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                      shm_snd->shm->addr.len, temp_buff, shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else if (seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                if (temp_buff.command == DATA) {
                    rcv_list_send_ack_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->list,
                                                &shm_snd->shm->addr.dest_addr,
                                                shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_rcv,
                                                &shm_snd->shm->window_base_rcv,
                                                shm_snd->shm->param.loss_prob, shm_snd->shm->param.window,
                                                shm_snd->shm->dimension,
                                                &shm_snd->shm->byte_written,shm_snd->shm);
                    if (shm_snd->shm->byte_written == shm_snd->shm->dimension) {
                        wait_for_fin_list(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->addr.sockfd,
                                          shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len,
                                          &shm_snd->shm->window_base_snd,
                                          &shm_snd->shm->window_base_rcv, &shm_snd->shm->pkt_fly,
                                          shm_snd->shm->param.window, &shm_snd->shm->byte_written,
                                          shm_snd->shm->param.loss_prob, shm_snd);
                        return shm_snd->shm->byte_written;
                    }
                } else {
                    printf("errore rcv_list2\n");
                    printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd,
                           shm_snd->shm->window_base_rcv);
                    handle_error_with_exit("");
                }
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto rcv list2 con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd, shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            printf("return rcv list\n");
            pthread_cancel(shm_snd->tid);
            printf("thread cancel rcv list\n");
            pthread_exit(NULL);
        }
    }
}

int
wait_for_list_dimension(int sockfd, struct sockaddr_in serv_addr, socklen_t len, int *byte_written, int *seq_to_send,
                        int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly, struct temp_buffer temp_buff,
                        struct window_rcv_buf *win_buf_rcv, struct window_snd_buf *win_buf_snd,
                        struct shm_snd *shm_snd) {
    errno = 0;
    char *list, *first;
    strcpy(temp_buff.payload, "list");
    send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                           shm_snd->shm->win_buf_snd, temp_buff.payload, LIST, &shm_snd->shm->seq_to_send,
                           shm_snd->shm->param.loss_prob,
                           shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, shm_snd->shm);//manda messaggio get
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff,MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//attendo risposta del server
            //mi blocco sulla risposta del server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto wait for list_dim con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly,
                                      shm_snd->shm);
                } else {
                    printf("ack duplicato non fare nulla\n");
                }
                alarm(TIMEOUT);
            } else if (temp_buff.command == ERROR) {
                rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                   shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_rcv,
                                                   &shm_snd->shm->window_base_rcv,
                                                   shm_snd->shm->param.loss_prob,
                                                   shm_snd->shm->param.window);
                close_connection_list(temp_buff, &shm_snd->shm->seq_to_send, shm_snd->shm->win_buf_snd,
                                      shm_snd->shm->addr.sockfd,
                                      shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len,
                                      &shm_snd->shm->window_base_snd,
                                      &shm_snd->shm->window_base_rcv, &shm_snd->shm->pkt_fly, shm_snd->shm->param.window,
                                      &shm_snd->shm->byte_written,
                                      shm_snd->shm->param.loss_prob, shm_snd);
                printf("lista vuota\n");
                pthread_cancel(shm_snd->tid);
                printf("thread cancel wait for list dimension\n");
                pthread_exit(NULL);
            } else if (temp_buff.command == DIMENSION) {
                rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                   shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_rcv,
                                                   &shm_snd->shm->window_base_rcv,
                                                   shm_snd->shm->param.loss_prob,
                                                   shm_snd->shm->param.window);
                shm_snd->shm->dimension = parse_integer(temp_buff.payload);
                printf("dimensione ricevuta %d\n", shm_snd->shm->dimension);
                shm_snd->shm->list = malloc(sizeof(char) * shm_snd->shm->dimension);
                if (shm_snd->shm->list == NULL) {
                    handle_error_with_exit("error in malloc\n");
                }
                memset(shm_snd->shm->list, '\0', shm_snd->shm->dimension);

                first = shm_snd->shm->list;
                rcv_list2(shm_snd->shm->addr.sockfd, shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                          shm_snd->shm->win_buf_snd, shm_snd->shm->win_buf_rcv, &shm_snd->shm->seq_to_send,
                          shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, &list,
                          shm_snd->shm->dimension, shm_snd->shm->param.loss_prob, &shm_snd->shm->window_base_snd,
                          &shm_snd->shm->window_base_rcv, &shm_snd->shm->byte_written,
                          shm_snd);
                printf("dimension %d\n", shm_snd->shm->dimension);
                printf("byte_written %d\n", shm_snd->shm->byte_written);
                printf("%d\n", (int) strlen(first));
                if (shm_snd->shm->byte_written == shm_snd->shm->dimension) {
                    printf("list:\n%s", first);//stampa della lista ottenuta
                } else {
                    printf("errore,lista non correttamente ricevuta\n");
                }
                free(first);//il puntatore di list è stato spostato per inviare la lista
                list = NULL;
                return shm_snd->shm->byte_written;
            } else {
                printf("ignorato pacchetto wait list dim con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd, shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel wait for list dimension\n");
            pthread_exit(NULL);
        }
    }
}

void *list_client_job(void *arg) {
    struct shm_snd *shm_snd = arg;
    struct temp_buffer temp_buff;
    wait_for_list_dimension(shm_snd->shm->addr.sockfd, shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len,
                            &shm_snd->shm->byte_written, &shm_snd->shm->seq_to_send,
                            &shm_snd->shm->window_base_snd, &shm_snd->shm->window_base_rcv, shm_snd->shm->param.window,
                            &shm_snd->shm->pkt_fly, temp_buff, shm_snd->shm->win_buf_rcv, shm_snd->shm->win_buf_snd,
                            shm_snd);
    return NULL;
}

void *list_client_rtx_job(void *arg) {
    printf("thread rtx creato\n");
    int byte_left;
    struct shm_sel_repeat *shm = arg;
    struct temp_buffer temp_buff;
    struct node *node = NULL;
    long timer_ns_left;
    char to_rtx;
    struct timespec sleep_time;
    block_signal(SIGALRM);//il thread receiver non viene bloccato dal segnale di timeout
    node = alloca(sizeof(struct node));
    for (;;) {
        lock_mtx(&(shm->mtx));
        while (1) {
            if (delete_head(&shm->head, node) == -1) {
                wait_on_a_condition(&(shm->list_not_empty), &shm->mtx);
            } else {
                if (!to_resend2(shm, *node)) {
                    //printf("pkt non da ritrasmettere\n");
                    continue;
                } else {
                    //printf("pkt da ritrasmettere\n");
                    break;
                }
            }
        }
        unlock_mtx(&(shm->mtx));
        timer_ns_left = calculate_time_left(*node);
        if (timer_ns_left <= 0) {
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend2(shm, *node);
            unlock_mtx(&(shm->mtx));
            if (!to_rtx) {
                //printf("no rtx immediata\n");
                continue;
            } else {
                //printf("rtx immediata\n");
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap = node->lap;
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command = shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd, &temp_buff, &shm->addr.dest_addr, shm->addr.len, shm->param.loss_prob);
                rtx_list_client++;
                lock_mtx(&(shm->mtx));
                if (clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[node->seq].time)) != 0) {
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq, node->lap, shm->win_buf_snd[node->seq].time, shm->param.timer_ms, &shm->head,
                               &shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        } else {
            sleep_struct(&sleep_time, timer_ns_left);
            nanosleep(&sleep_time, NULL);
            lock_mtx(&(shm->mtx));
            to_rtx = to_resend2(shm, *node);
            unlock_mtx(&(shm->mtx));
            if (!to_rtx) {
                continue;
            } else {
                //printf("rtx dopo sleep\n");
                temp_buff.ack = NOT_AN_ACK;
                temp_buff.seq = node->seq;
                temp_buff.lap = node->lap;
                copy_buf2_in_buf1(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command = shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd, &temp_buff, &shm->addr.dest_addr, shm->addr.len, shm->param.loss_prob);
                rtx_list_client++;
                lock_mtx(&(shm->mtx));
                if (clock_gettime(CLOCK_MONOTONIC, &(shm->win_buf_snd[node->seq].time)) != 0) {
                    handle_error_with_exit("error in get_time\n");
                }
                insert_ordered(node->seq, node->lap, shm->win_buf_snd[node->seq].time, shm->param.timer_ms, &shm->head,
                               &shm->tail);
                unlock_mtx(&(shm->mtx));
            }
        }
    }
    return NULL;
}

void list_client(struct shm_sel_repeat *shm) {
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd, tid_rtx;
    struct shm_snd shm_snd;
    if (pthread_create(&tid_rtx, NULL, list_client_rtx_job, shm) != 0) {
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rtx\n", tid_rtx);
    shm_snd.tid = tid_rtx;
    shm_snd.shm = shm;
    if (pthread_create(&tid_snd, NULL, list_client_job, &shm_snd) != 0) {
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_snd\n", tid_snd);
    block_signal(
            SIGALRM);//il thread principale non viene interrotto dal segnale di timeout,ci sono altri thread?(waitpid ecc?)
    if (pthread_join(tid_snd, NULL) != 0) {
        handle_error_with_exit("error in pthread_join\n");
    }
    if (pthread_join(tid_rtx, NULL) != 0) {
        handle_error_with_exit("error in pthread_join\n");
    }
    return;
}