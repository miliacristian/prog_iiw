#include "basic.h"
#include "timer.h"
#include "Server.h"
#include "get_server.h"
#include "communication.h"
#include "dynamic_list.h"
//è stato riscontrato tutto manda il fin e termina trasmissione
int close_get_send_file( struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {//manda fin non in finestra senza sequenza e ack e chiudi
    alarm(0);
    //manda fin
    send_message(shm->addr.sockfd, &shm->addr.dest_addr, shm->addr.len, temp_buff, "FIN",
                 FIN, shm->param.loss_prob);
    pthread_cancel(shm->tid);
    pthread_exit(NULL);
}
//dopo aver ricevuto start inizia a mandare il file
int send_file( struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
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
                        //printf("byte readed %d\n", shm->byte_readed);
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
                printf("ignorato pacchetto send_file con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command, temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        }
        if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK && errno != 0) {
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {//se è scaduto il timer termina i 2 thread della trasmissione
            great_alarm_serv = 0;
            printf("il client non è in ascolto send file\n");
            alarm(0);
            pthread_cancel(shm->tid);
            pthread_exit(NULL);
        }
    }
}
//dopo aver ricevuto il comando get manda dimensione del file e aspetta start
int wait_for_start_get(struct temp_buffer temp_buff, struct shm_sel_repeat *shm) {
    char *path, dim_string[11];
    path = generate_full_pathname(shm->filename, dir_server);
    if (path == NULL) {
        handle_error_with_exit("error in generate full path\n");
    }
    //ottieni la dimensione del file da inviare e inseriscila insieme all'MD5 dentro il pacchetto da inviare al client
    if (check_if_file_exist(path)) {
        shm->dimension = get_file_size(path);
        sprintf(dim_string, "%d", shm->dimension);
        shm->fd = open(path, O_RDONLY);
        if (shm->fd == -1) {
            handle_error_with_exit("error in open\n");
        }
        calc_file_MD5(path, shm->md5_sent);
        better_strcpy(temp_buff.payload, dim_string);//non dovrebbe essere strcat?
        better_strcat(temp_buff.payload, " ");
        better_strcat(temp_buff.payload, shm->md5_sent);
        free(path);
        send_message_in_window(temp_buff,shm, DIMENSION,dim_string);//manda pacchetto dimension
    } else {
        send_message_in_window(temp_buff,shm, ERROR,"il file non esiste");//manda pacchetto errore
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
                pthread_cancel(shm->tid);
                pthread_exit(NULL);
            } else if (temp_buff.command == START) {//se ricevi start vai nello stato di send_file
                printf("messaggio start ricevuto\n");
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
                printf("ignorato pacchetto wait_for_start_get con ack %d seq %d command %d lap %d\n", temp_buff.ack,
                       temp_buff.seq,
                       temp_buff.command, temp_buff.lap);
                printf("winbase snd %d winbase rcv %d\n", shm->window_base_snd, shm->window_base_rcv);
                handle_error_with_exit("");
            }
        } else if (errno != EINTR && errno != 0) {//se è scaduto il timer termina i 2 thread della trasmissione
            handle_error_with_exit("error in recvfrom\n");
        }
        if (great_alarm_serv == 1) {
            great_alarm_serv = 0;
            printf("il client non è in ascolto wait_for_start_get\n");
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
//thread ritrasmettitore
void *get_server_rtx_job(void *arg) {
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

void get_server(struct shm_sel_repeat *shm) {//crea i 2 thread:
    //trasmettitore,ricevitore;
    //ritrasmettitore
    pthread_t tid_snd, tid_rtx;
    if (pthread_create(&tid_rtx, NULL, get_server_rtx_job, shm) != 0) {
        handle_error_with_exit("error in create thread get_server_rtx\n");
    }
    shm->tid = tid_rtx;
    //shm_snd.shm=shm;
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
int execute_get(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) {
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
    return shm->byte_readed;
}