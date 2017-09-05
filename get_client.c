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

int rtx_get_client = 0;

int close_connection_get(struct temp_buffer temp_buff, int *seq_to_send, struct window_snd_buf *win_buf_snd, int sockfd,
                         struct sockaddr_in serv_addr, socklen_t len, int *window_base_snd, int *window_base_rcv,
                         int *pkt_fly, int W, int *byte_written, double loss_prob, struct shm_snd *shm_snd) {
    printf("close connection\n");
    send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                           shm_snd->shm->win_buf_snd, "FIN", FIN, &shm_snd->shm->seq_to_send,
                           shm_snd->shm->param.loss_prob, shm_snd->shm->param.window, &shm_snd->shm->pkt_fly,
                           shm_snd->shm);//manda messaggio di fin
    alarm(TIMEOUT);
    errno = 0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//attendo fin_ack dal server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {//
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto close_conn get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly, shm_snd->shm);
                } else {
                    printf("close connect get ack duplicato\n");
                }
                alarm(0);
            } else if (temp_buff.command == FIN_ACK) {
                alarm(0);
                printf("return close connection 1\n");
                return shm_snd->shm->byte_written;
            } else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                      shm_snd->shm->addr.len, temp_buff, shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato close connect pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
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
            pthread_cancel(shm_snd->tid);
            printf("thread cancel close connection\n");
            pthread_exit(NULL);
        }
    }
}

int wait_for_fin_get(struct temp_buffer temp_buff, struct window_snd_buf *win_buf_snd, int sockfd,
                     struct sockaddr_in serv_addr, socklen_t len, int *window_base_snd, int *window_base_rcv,
                     int *pkt_fly, int W, int *byte_written, double loss_prob, struct shm_snd *shm_snd) {
    char *path;
    printf("wait for fin\n");
    path = generate_full_pathname(shm_snd->shm->filename, dir_client);
    if (path == NULL) {
        handle_error_with_exit("error in generate full pathname\n");
    }
    alarm(TIMEOUT);;//chiusura temporizzata
    errno = 0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//attendo messaggio di fin,
            // aspetto finquando non lo ricevo
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto wait for fin_get con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.command == FIN) {
                alarm(0);
                printf("return wait_for_fin 1\n");
                check_md5(path, shm_snd->shm->md5_sent);
                pthread_cancel(shm_snd->tid);
                printf("thread cancel \n");
                pthread_exit(NULL);
            } else if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly, shm_snd->shm);
                } else {
                    printf("wait for fin get ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                      shm_snd->shm->addr.len, temp_buff, shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else {
                printf("ignorato wait for fin pacchetto con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
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
            check_md5(path, shm_snd->shm->md5_sent);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel wait for fin\n");
            pthread_exit(NULL);
        }
    }
}

int rcv_get_file(int sockfd, struct sockaddr_in serv_addr, socklen_t len, struct temp_buffer temp_buff,
                 struct window_snd_buf *win_buf_snd, struct window_rcv_buf *win_buf_rcv, int *seq_to_send, int W,
                 int *pkt_fly, int fd, int dimension, double loss_prob, int *window_base_snd, int *window_base_rcv,
                 int *byte_written, struct shm_snd *shm_snd) {
    alarm(TIMEOUT);
    printf("sto ancora qui\n");
    send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                           shm_snd->shm->win_buf_snd, "START", START, &shm_snd->shm->seq_to_send,
                           shm_snd->shm->param.loss_prob, shm_snd->shm->param.window, &shm_snd->shm->pkt_fly,
                           shm_snd->shm);
    printf("messaggio start inviato\n");
    printf("rcv file\n");
    errno = 0;
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//bloccati finquando non ricevi file
            // o altri messaggi
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto rcv_get_file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly, shm_snd->shm);
                } else {
                    printf("rcv_get_file ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (!seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_msg_re_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                      shm_snd->shm->addr.len, temp_buff, shm_snd->shm->param.loss_prob);
                alarm(TIMEOUT);
            } else if (seq_is_in_window(shm_snd->shm->window_base_rcv, shm_snd->shm->param.window, temp_buff.seq)) {
                rcv_data_send_ack_in_window(shm_snd->shm->addr.sockfd, shm_snd->shm->fd, &shm_snd->shm->addr.dest_addr,
                                            shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_rcv,
                                            &shm_snd->shm->window_base_rcv, shm_snd->shm->param.loss_prob,
                                            shm_snd->shm->param.window, shm_snd->shm->dimension,
                                            &shm_snd->shm->byte_written);
                if (shm_snd->shm->byte_written == shm_snd->shm->dimension) {
                    wait_for_fin_get(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->addr.sockfd,
                                     shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, &shm_snd->shm->window_base_snd,
                                     &shm_snd->shm->window_base_rcv, &shm_snd->shm->pkt_fly, shm_snd->shm->param.window, &shm_snd->shm->byte_written,
                                     shm_snd->shm->param.loss_prob, shm_snd);
                    printf("return rcv file 1\n");
                    return shm_snd->shm->byte_written;
                }
                alarm(TIMEOUT);
            } else {
                printf("ignorato pacchetto rcv get file con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                       temp_buff.command);
                printf("winbase snd %d winbase rcv %d\n", shm_snd->shm->window_base_snd, shm_snd->shm->window_base_rcv);
                handle_error_with_exit("");
                alarm(TIMEOUT);
            }
        } else if (errno != EINTR && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_client == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm_client = 0;
            alarm(0);
            pthread_cancel(shm_snd->tid);
            printf("thread cancel rcv get file\n");
            pthread_exit(NULL);
        }
    }
}

int wait_for_get_dimension2(int sockfd, struct sockaddr_in serv_addr, socklen_t len, char *filename, int *byte_written,
                            int *seq_to_send, int *window_base_snd, int *window_base_rcv, int W, int *pkt_fly,
                            struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv,
                            struct window_snd_buf *win_buf_snd, struct shm_snd *shm_snd) {
    errno = 0;
    char *path, *first, *payload;
    strcpy(temp_buff.payload, "get ");
    strcat(temp_buff.payload, filename);
    send_message_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                           shm_snd->shm->win_buf_snd, temp_buff.payload, GET, &shm_snd->shm->seq_to_send,
                           shm_snd->shm->param.loss_prob, shm_snd->shm->param.window, &shm_snd->shm->pkt_fly,
                           shm_snd->shm);//manda messaggio get
    alarm(TIMEOUT);
    while (1) {
        if (recvfrom(shm_snd->shm->addr.sockfd, &temp_buff, MAXPKTSIZE, 0,
                     (struct sockaddr *) &shm_snd->shm->addr.dest_addr, &shm_snd->shm->addr.len) !=
            -1) {//attendo risposta del server
            //mi blocco sulla risposta del server
            if (temp_buff.command == SYN || temp_buff.command == SYN_ACK) {
                continue;//ignora pacchetto
            } else {
                alarm(0);
            }
            printf("pacchetto ricevuto wait_get_dim con ack %d seq %d command %d\n", temp_buff.ack, temp_buff.seq,
                   temp_buff.command);
            if (temp_buff.seq == NOT_A_PKT && temp_buff.ack != NOT_AN_ACK) {
                if (seq_is_in_window(shm_snd->shm->window_base_snd, shm_snd->shm->param.window, temp_buff.ack)) {
                    rcv_ack_in_window(temp_buff, shm_snd->shm->win_buf_snd, shm_snd->shm->param.window,
                                      &shm_snd->shm->window_base_snd, &shm_snd->shm->pkt_fly, shm_snd->shm);
                } else {
                    printf("wait get_dim ack duplicato\n");
                }
                alarm(TIMEOUT);
            } else if (temp_buff.command == ERROR) {
                rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                   shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_rcv,
                                                   &shm_snd->shm->window_base_rcv, shm_snd->shm->param.loss_prob,
                                                   shm_snd->shm->param.window);
                close_connection_get(temp_buff, &shm_snd->shm->seq_to_send, shm_snd->shm->win_buf_snd,
                                     shm_snd->shm->addr.sockfd, shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len,
                                     &shm_snd->shm->window_base_snd, &shm_snd->shm->window_base_rcv,
                                     &shm_snd->shm->pkt_fly, shm_snd->shm->param.window, &shm_snd->shm->byte_written,
                                     shm_snd->shm->param.loss_prob, shm_snd);
                printf("return wait for dimension 1\n");
                return shm_snd->shm->byte_written;
            } else if (temp_buff.command == DIMENSION) {
                path = generate_multi_copy(dir_client, shm_snd->shm->filename);
                if (path == NULL) {
                    handle_error_with_exit("error:there are too much copies of the file");
                }
                shm_snd->shm->fd = open(path, O_WRONLY | O_CREAT, 0666);
                if (shm_snd->shm->fd == -1) {
                    handle_error_with_exit("error in open file\n");
                }
                free(path);
                payload = malloc(sizeof(char) * (MAXPKTSIZE - OVERHEAD));
                if (payload == NULL) {
                    handle_error_with_exit("error in malloc\n");
                }
                copy_buf1_in_buf2(payload, temp_buff.payload, MAXPKTSIZE - OVERHEAD);
                //strcpy(payload,temp_buff.payload);
                //printf("payload %s\n",temp_buff.payload);
                //printf("payload %s\n",payload);
                first = payload;
                shm_snd->shm->dimension = parse_integer_and_move(&payload);
                payload++;
                strncpy(shm_snd->shm->md5_sent, payload, MD5_LEN);
                shm_snd->shm->md5_sent[MD5_LEN] = '\0';
                printf("md5 %s\n", shm_snd->shm->md5_sent);
                rcv_msg_send_ack_command_in_window(shm_snd->shm->addr.sockfd, &shm_snd->shm->addr.dest_addr,
                                                   shm_snd->shm->addr.len, temp_buff, shm_snd->shm->win_buf_rcv,
                                                   &shm_snd->shm->window_base_rcv, shm_snd->shm->param.loss_prob,
                                                   shm_snd->shm->param.window);
                free(first);
                printf("sto qui\n");
                rcv_get_file(shm_snd->shm->addr.sockfd, shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len, temp_buff,
                             shm_snd->shm->win_buf_snd, shm_snd->shm->win_buf_rcv, &shm_snd->shm->seq_to_send,
                             shm_snd->shm->param.window, &shm_snd->shm->pkt_fly, shm_snd->shm->fd,
                             shm_snd->shm->dimension, shm_snd->shm->param.loss_prob, &shm_snd->shm->window_base_snd,
                             &shm_snd->shm->window_base_rcv, &shm_snd->shm->byte_written, shm_snd);
                if (close(shm_snd->shm->fd) == -1) {
                    handle_error_with_exit("error in close file\n");
                }
                pthread_cancel(shm_snd->tid);
                printf("thread cancel put client\n");
                pthread_exit(NULL);
            } else {
                printf("ignorato pacchetto wait get dimension con ack %d seq %d command %d\n", temp_buff.ack,
                       temp_buff.seq,
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
            printf("thread cancel wait for get dimension\n");
            pthread_exit(NULL);
        }
    }
}

void *get_client_job(void *arg) {
    struct shm_snd *shm_snd = arg;
    struct temp_buffer temp_buff;
    wait_for_get_dimension2(shm_snd->shm->addr.sockfd, shm_snd->shm->addr.dest_addr, shm_snd->shm->addr.len,
                            shm_snd->shm->filename, &shm_snd->shm->byte_written, &shm_snd->shm->seq_to_send,
                            &shm_snd->shm->window_base_snd, &shm_snd->shm->window_base_rcv, shm_snd->shm->param.window,
                            &shm_snd->shm->pkt_fly, temp_buff, shm_snd->shm->win_buf_rcv, shm_snd->shm->win_buf_snd,
                            shm_snd);
    return NULL;
}

void *get_client_rtx_job(void *arg) {
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
                copy_buf1_in_buf2(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command = shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd, &temp_buff, &shm->addr.dest_addr, shm->addr.len, shm->param.loss_prob);
                rtx_get_client++;
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
                copy_buf1_in_buf2(temp_buff.payload, shm->win_buf_snd[node->seq].payload, MAXPKTSIZE - OVERHEAD);
                temp_buff.command = shm->win_buf_snd[node->seq].command;
                resend_message(shm->addr.sockfd, &temp_buff, &shm->addr.dest_addr, shm->addr.len, shm->param.loss_prob);
                rtx_get_client++;
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

void get_client(struct shm_sel_repeat *shm) {
    //initialize_cond();inizializza tutte le cond
    pthread_t tid_snd, tid_rtx;
    struct shm_snd shm_snd;
    if (pthread_create(&tid_rtx, NULL, get_client_rtx_job, shm) != 0) {
        handle_error_with_exit("error in create thread put client rcv\n");
    }
    printf("%d tid_rtx\n", tid_rtx);
    shm_snd.tid = tid_rtx;
    shm_snd.shm = shm;
    if (pthread_create(&tid_snd, NULL, get_client_job, &shm_snd) != 0) {
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
