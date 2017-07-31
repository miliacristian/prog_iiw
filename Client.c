#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include <time.h>
#include "timer.h"


struct addr *addr = NULL;
struct itimerspec sett_timer, rst_timer;//timer e reset timer globali
int great_alarm = 0;//se diventa 1 è scattato il timer grande
timer_t timeout_timer_id; //id  del timer di timeout;
struct select_param param_client;
char *dir_client;

void timer_handler(int sig, siginfo_t *si, void *uc) {
    if (sig == SIGRTMIN) {
        (void) sig;
        (void) si;
        (void) uc;
        struct window_snd_buf *win_buffer = si->si_value.sival_ptr;
        struct temp_buffer temp_buf;
        strcpy(temp_buf.payload, win_buffer->payload);//dati del pacchetto da ritrasmettere
        temp_buf.ack = NOT_AN_ACK;
        temp_buf.seq = win_buffer->seq_numb;//numero di sequenza del pacchetto da ritrasmettere
        resend_message(addr->sockfd, &temp_buf, &(addr->dest_addr), sizeof(addr->dest_addr), param_client.loss_prob);//ritrasmetto il pacchetto di cui è scaduto il timer
        //printf("pacchetto ritrasmesso con ack %d seq %d dati %s:\n", temp_buf.ack, temp_buf.seq, temp_buf.payload);
        /*if (timer_settime((*win_buffer).time_id, 0, &sett_timer, NULL) == -1) {//avvio timer
            handle_error_with_exit("error in timer_settime\n");
        }*/
        start_timer((*win_buffer).time_id, &sett_timer);
    }
    else if(sig == SIGRTMIN+1){
        printf("great timeout expired\n");
        great_alarm = 1;
    }
    return;
}


int get_command(int sockfd, struct sockaddr_in serv_addr, char *filename) {//svolgi la get con connessione già instaurata
    int byte_written = 0, fd, byte_expected, seq_to_send = 0, window_base_snd = 0, ack_numb = 0, window_base_rcv = 0, W = param_client.window;//primo pacchetto della finestra->primo non riscontrato
    int pkt_fly = 0;
    char value,*pathname;//fare la free di pathname
    double timer = param_client.timer_ms, loss_prob = param_client.loss_prob;
    struct temp_buffer temp_buff;//pacchetto da inviare
    struct window_rcv_buf win_buf_rcv[2 * W];
    struct window_snd_buf win_buf_snd[2 * W];
    struct addr temp_addr;
    struct sigaction sa, sa_timeout;
    memset(win_buf_rcv, 0, sizeof(struct window_rcv_buf) * (2 * W));//inizializza a zero
    memset(win_buf_snd, 0, sizeof(struct window_snd_buf) * (2 * W));//inizializza a zero
    //inizializzo numeri di sequenza nell'array di struct
    for (int i = 0; i < 2 * W; i++) {
        win_buf_snd[i].seq_numb = i;
    }

    socklen_t len = sizeof(serv_addr);

    make_timers(win_buf_snd, W);//crea 2w timer
    set_timer(&sett_timer, param_client.timer_ms);//inizializza struct necessaria per scegliere il timer

    temp_addr.sockfd = sockfd;
    temp_addr.dest_addr = serv_addr;
    addr = &temp_addr;//inizializzo puntatore globale necessario per signal_handler

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    strcpy(temp_buff.payload, "get ");
    strcat(temp_buff.payload, filename);
    temp_buff.seq = seq_to_send;
    temp_buff.ack =NOT_AN_ACK;//ack=-5 solo seq e payload
    strcpy(win_buf_snd[seq_to_send].payload, temp_buff.payload);//memorizzo pacchetto in finestra

    send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr),param_client.loss_prob ); //invio pacchetto con probabilità loss_prob

    //printf("pacchetto inviato con ack %d seq %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.payload);
    /*if (timer_settime(win_buf_snd[seq_to_send].time_id, 0, &sett_timer, NULL) == -1) {
        handle_error_with_exit("error in timer_settime\n");
    }*/
    start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
    seq_to_send = (seq_to_send + 1) % (2 * W);
    start_timeout_timer(timeout_timer_id, 5000);
    while (1) {
        if (recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len) != -1) {//risposta del server
            if (&temp_buff != NULL) {
                if (temp_buff.ack == ACK_ERROR) {
                    printf("pacchetto ricevuto con ack %d seq %d dati %s:\n", temp_buff.ack, temp_buff.seq,temp_buff.payload);
                    //reset_timeout_timer(timeout_timer_id, &rst_timer);
                    stop_timer(timeout_timer_id);
                    start_timeout_timer(timeout_timer_id, 5000);//chiusura temporizzata
                    stop_timer(win_buf_snd[0].time_id);

                    temp_buff.ack=temp_buff.seq;
                    temp_buff.seq = NOT_A_PKT;
                    //temp_buff.ack = 0;
                    //riscontro il messaggio di errore

                    send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr),param_client.loss_prob );

                    //printf("pacchetto inviato con ack %d seq %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.payload);
                    /*if (timer_settime(win_buf_snd[0].time_id, 0, &rst_timer, NULL) == -1) {//resetta timer del comando list
                        handle_error_with_exit("error in timer_settime\n");
                    }*/
                    window_base_snd = (window_base_snd + 1) % (2 * W);
                    while(1){
                        if(recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, (struct sockaddr *) &serv_addr, &len)!=-1){
                            if(temp_buff.ack==ACK_ERROR){
                                temp_buff.ack=temp_buff.seq;
                                temp_buff.seq=NOT_A_PKT;
                                send_message(sockfd, &temp_buff, &serv_addr, sizeof(serv_addr),param_client.loss_prob ); //riscontro il messaggio di errore
                                //ritrasmissioni del pkt precedente
                                //printf("pacchetto ritrasmesso con ack %d seq %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.payload);
                            }
                            else if(temp_buff.seq==FIN_SEQ){
                                //reset_timeout_timer(timeout_timer_id, &rst_timer);
                                stop_timer(timeout_timer_id);
                                printf("segmento di FIN ricevuto\n");
                                stop_all_timers(win_buf_snd, W);
                                return byte_written;
                            }
                        }
                        else if(great_alarm==1){
                            great_alarm=0;
                            printf("il server non risponde\n");
                            stop_all_timers(win_buf_snd, W);
                            return byte_written;
                        }
                    }
                }
                else {//pacchetto con dati:dimensione
                    //alarm(0);
                    //reset_timeout_timer(timeout_timer_id, &rst_timer);
                    stop_timer(timeout_timer_id);
                    start_timeout_timer(timeout_timer_id, 7000);
                    printf("pacchetto ricevuto con ack %d seq %d dati %s:\n", temp_buff.ack, temp_buff.seq,temp_buff.payload);
                    //pacchetto ack=0 seq=0 dati=dim ricevuto
                    byte_expected = parse_integer(temp_buff.payload);//segno la dimensione
                    printf("dimensione file ricevuta dal server %d\n", byte_expected);
                    /*if (timer_settime(win_buf_snd[temp_buff.ack].time_id, 0, &rst_timer, NULL) == -1) {//resetta timer
                        handle_error_with_exit("error in timer_settime\n");
                    }*/
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    window_base_snd = (window_base_snd + 1) % (2 * W);//avanzo finestra ora siamo a window base=1
                    //creo messaggio start
                    strcpy(temp_buff.payload, "START");//dati
                    temp_buff.ack = temp_buff.seq;//0
                    temp_buff.seq = seq_to_send;//1

                    strcpy(win_buf_snd[seq_to_send].payload, temp_buff.payload);
                    send_message(sockfd, &temp_buff, &serv_addr,sizeof(serv_addr),param_client.loss_prob ); //notifica il server che può iniziare a mandare il file
                    //pacchetto inviato con ack %d seq %d dati %s:\n", temp_buff.ack, temp_buff.seq,temp_buff.payload);
                    /*if (timer_settime(win_buf_snd[seq_to_send].time_id, 0, &sett_timer, NULL) == -1) {
                        handle_error_with_exit("error in timer_settime\n");
                    }*/
                    start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
                    seq_to_send = (seq_to_send + 1) % (2 * W);
                    break;
                }
            }
        }
        else if (great_alarm == 1) {//sono passati n secondi e senza risposta dal server
            great_alarm = 0;
            printf("server timeout & exit\n");
            stop_all_timers(win_buf_snd, W);
            return byte_written;
        }
    }//ho una dimensione del file

    printf("inizio ricezione file\n");
    pathname=malloc(sizeof(char)*MAXFILENAME);
    if(pathname==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    strcpy(pathname,dir_client);
    printf("%s\n",dir_client);
    strcat(pathname,filename);
    printf("%s\n",pathname);
    fd = open(pathname, O_WRONLY | O_CREAT, 0666);
    if (fd == -1) {
        handle_error_with_exit("error in open\n");
    }
    while (byte_written < byte_expected) {
        //alarm(5);//timeout cautelativo per capire se effettivamente il sender ha ricevuto command_ack
        if (recvfrom(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) &serv_addr, &len) != -1) {//bloccante
            if (&temp_buff != NULL) {
                //alarm(0);//resetto il timer perchè ho ricevuto un pacchetto
                //reset_timeout_timer(timeout_timer_id, &rst_timer);
                stop_timer(timeout_timer_id);
                start_timeout_timer(timeout_timer_id, 5000);
                if (temp_buff.seq > (2 * W - 1)) {//num sequenza imprevisto
                    //ignora
                } else if (!seq_is_in_window(window_base_rcv, window_base_rcv + W - 1, W, temp_buff.seq)) {
                    //se il numero  non è dentro la finestra
                    // un ack è stato smarrito->rinvialo
                    send_message(sockfd, &temp_buff, &serv_addr,sizeof(serv_addr),param_client.loss_prob ); //rinvio ack
                } else {//ricevuto numero sequenza in window
                    if (temp_buff.ack >= 0) {//ack trasmesso insieme al pacchetto
                        win_buf_snd[temp_buff.ack].acked = 1;
                        /*if (timer_settime(win_buf_snd[temp_buff.ack].time_id, 0, &rst_timer, NULL) == -1) {
                            handle_error_with_exit("error in timer_settime\n");
                        }*/
                        stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    }
                    win_buf_rcv[temp_buff.seq].received = 1;//segno pacchetto n-esimo come ricevuto
                    strcpy(win_buf_rcv[temp_buff.seq].payload, temp_buff.payload);//memorizzo il pacchetto n-esimo
                    if (temp_buff.seq == window_base_rcv) {//se pacchetto riempie un buco
                        // scorro la finestra fino al primo ancora non ricevuto
                        while (win_buf_rcv[window_base_rcv].received == 1) {
                            //dentro il buffer non deve esserci il terminatore
                            writen(fd, win_buf_rcv[window_base_rcv].payload,
                                   strlen(win_buf_rcv[window_base_rcv].payload));//necessario cosi non copia il terminatore
                            //controllo su writen
                            byte_written += strlen(win_buf_rcv[window_base_rcv].payload);
                            win_buf_rcv[window_base_rcv].received = 0;//segna pacchetto come non ricevuto
                            window_base_rcv = (window_base_rcv + 1) % (2 * W);//avanza la finestra con modulo di 2W
                        }
                        if (byte_written == byte_expected) {//fin_ack
                            while (1) {//chiusura di connessione
                                //alarm(5);
                                start_timeout_timer(timeout_timer_id, 5000);
                                if (recvfrom(sockfd, &temp_buff, MAXPKTSIZE, 0, (struct sockaddr *) &serv_addr, &len) !=
                                    -1) {//aspetta il fin
                                    //alarm(0);//resetto il timer perchè ho ricevuto un pacchetto
                                    //reset_timeout_timer(timeout_timer_id, &rst_timer);
                                    stop_timer(timeout_timer_id);
                                    if (temp_buff.seq > (2 * W - 1)) {//num sequenza imprevisto
                                        //ignora
                                    } else if (temp_buff.seq == -2) {//fine trasferimento
                                        temp_buff.ack = -2;
                                        strcpy(temp_buff.payload, "FIN_ACK");
                                        temp_buff.seq = -5;
                                        send_message(sockfd, &temp_buff, &serv_addr,sizeof(serv_addr),param_client.loss_prob );//mando fin_ack 1 sola volta;
                                        stop_all_timers(win_buf_snd, W);
                                        return byte_written;
                                    } else if (!seq_is_in_window(window_base_rcv, window_base_rcv + W - 1, W,
                                                                 temp_buff.seq)) {
                                        //se il numero  non è dentro la finestra
                                        // un ack è stato smarrito->rinvialo
                                        send_message(sockfd, &temp_buff, &serv_addr,sizeof(serv_addr),param_client.loss_prob );//rinvio ack
                                    }
                                } else if (great_alarm == 1) {
                                    great_alarm=0;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        } else if (great_alarm == 1) {
            printf("il sender non sta mandando più nulla o errore interno\n");
            great_alarm = 0;
            stop_all_timers(win_buf_snd, W);
            return byte_written;
        }

    }
    great_alarm = 0;
    stop_all_timers(win_buf_snd, W);
    return byte_written;
}

int list_command(int sockfd, struct sockaddr_in serv_addr) {//svolgi la list con connessione già instaurata
    return 0;
}

int put_command(int sockfd, struct sockaddr_in serv_addr, char *filename) {
    return 0;
}

struct sockaddr_in send_syn_recv_ack(int sockfd, struct sockaddr_in main_servaddr) {//client manda messaggio syn
// e server risponde con ack cosi il client sa chi contattare per mandare i messaggi di comando
    struct sigaction sa;
    socklen_t len = sizeof(main_servaddr);
    sa.sa_flags =SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    char rtx = 0;
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGRTMIN+1, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    while (rtx < 5000) {//scaduto 5 volte il timer il server non risponde
        /*if (sendto(sockfd, NULL, 0, 0, (struct sockaddr *) &main_servaddr, sizeof(main_servaddr)) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in syn sendto\n");//pkt num sequenza zero mandato
        }
        printf("syn mandato\n");*/
        send_syn(sockfd, &main_servaddr, sizeof(main_servaddr), param_client.loss_prob);  //mando syn al processo server principale
        printf("mi metto in ricezione del syn_ack\n");
        start_timeout_timer(timeout_timer_id, 3000);

        if (recvfrom(sockfd, NULL, 0, 0, (struct sockaddr *) &main_servaddr, &len) !=-1) {//ricevo il syn_ack del server,solo qui sovrascrivo la struct
            // so l'indirizzo di chi contattare
            //le recvfrom successive non sovrascrivo struct e ignoro messaggi vuoti
            //alarm(0);
            //reset_timeout_timer(timeout_timer_id, &rst_timer);7
            stop_timer(timeout_timer_id);
            printf("connessione instaurata\n");
            great_alarm = 0;
            return main_servaddr;//ritorna l'indirizzo del processo figlio del server
        }

        rtx++;
    }
    great_alarm = 0;
    handle_error_with_exit("il server non è in ascolto\n");
    return main_servaddr;
}


void client_list_job() {
    struct sockaddr_in serv_addr, cliaddr;
    int sockfd;
    printf("client list job\n");
    make_timeout_timer(&timeout_timer_id);
    memset((void *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket\n");
    }
    memset((void *) &cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    serv_addr = send_syn_recv_ack(sockfd, serv_addr);
    list_command(sockfd, serv_addr);
    exit(EXIT_SUCCESS);
}

void client_get_job(char *filename) {
    printf("client get job\n");
    struct sockaddr_in serv_addr, cliaddr;
    int sockfd;
    make_timeout_timer(&timeout_timer_id);
    memset((void *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket\n");
    }
    memset((void *) &cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    serv_addr = send_syn_recv_ack(sockfd, serv_addr);
    get_command(sockfd, serv_addr, filename);
    exit(EXIT_SUCCESS);
}

void client_put_job(char *filename) {//upload e filename già verificato
    printf("client put_job\n");
    struct sockaddr_in serv_addr, cliaddr;
    int sockfd;
    make_timeout_timer(&timeout_timer_id);
    memset((void *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket\n");
    }
    memset((void *) &cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    serv_addr = send_syn_recv_ack(sockfd, serv_addr);
    put_command(sockfd, serv_addr, filename);
    exit(EXIT_SUCCESS);
}
void *thread_job(void *arg) {
    (void) arg;
    //waitpid dei processi del client
    pid_t pid;
    while (1) {
        while ((pid = waitpid(-1, NULL, 0)) > 0) {
            printf("process %d\n", pid);
        }
    }
    return NULL;
}

void create_thread_waitpid() {
    pthread_t tid;
    pthread_create(&tid, 0, thread_job, NULL);
    return;
}


int main(int argc, char *argv[]) {
    char *filename, *command, conf_upload[4], buff[MAXPKTSIZE + 1], *line, localname[80];
    int path_len, fd;
    socklen_t len;
    pid_t pid;
    struct sockaddr_in servaddr, cliaddr, main_servaddr;

    if (argc != 2) {
        handle_error_with_exit("usage <directory>\n");
    }
    dir_client = argv[1];
    check_if_dir_exist(dir_client);
    strcpy(localname, "");
    strcpy(localname, getenv("HOME"));
    strcat(localname, "/parameter.txt");
    fd = open(localname, O_RDONLY);
    if (fd == -1) {
        handle_error_with_exit("error in read parameters into file\n");
    }
    line = malloc(sizeof(char) * MAXLINE);
    command = line;
    memset(line, '\0', MAXLINE);
    if (readline(fd, line, MAXLINE) <= 0) {
        handle_error_with_exit("error in read line\n");
    }
    param_client.window = parse_integer_and_move(&line);
    if (param_client.window < 1) {
        handle_error_with_exit("window must be greater than 0\n");
    }
    skip_space(&line);
    param_client.loss_prob = parse_double_and_move(&line);
    if (param_client.loss_prob < 0 || param_client.loss_prob > 100) {
        handle_error_with_exit("invalid loss prob\n");
    }
    skip_space(&line);
    param_client.timer_ms = parse_long_and_move(&line);
    if (param_client.timer_ms < 0) {
        handle_error_with_exit("timer must be positive or zero");
    }
    path_len = strlen(dir_client);
    if (close(fd) == -1) {
        handle_error_with_exit("error in close file\n");
    }
    line = command;//rimando indietro line spostato
    command = NULL;
    free(line);
    create_thread_waitpid();
    if ((command = malloc(sizeof(char) * 5)) == NULL) {
        handle_error_with_exit("error in malloc buffercommand\n");
    }
    if ((filename = malloc(sizeof(char) * (MAXFILENAME))) == NULL) {
        handle_error_with_exit("error in malloc filename\n");
    }
    for (;;) {
        check_and_parse_command(command, filename);//inizializza command,filename e size
        if (filename != NULL) {
        }
        if (!is_blank(filename) && (strcmp(command, "put") == 0)) {
            char *path = alloca(sizeof(char) * (strlen(filename) + path_len + 1));
            strcpy(path, dir_client);
            move_pointer(&path, path_len);
            strcpy(path, filename);
            path = path - path_len;
            printf("%s\n", path);
            if (!check_if_file_exist(path)) {
                printf("file not exist\n");
                continue;
            } else {
                printf("file size is %d bytes\n", get_file_size(path));
                while (1) {
                    printf("confirm upload file %s [y/n]\n", filename);
                    if (fgets(conf_upload, MAXLINE, stdin) == NULL) {
                        handle_error_with_exit("error in fgets\n");
                    }
                    if (strlen(conf_upload) != 2) {
                        printf("type y or n\n");
                        continue;
                    } else if (strncmp(conf_upload, "y", 1) == 0) {

                        printf("confirm\n");
                        if ((pid = fork()) == -1) {
                            handle_error_with_exit("error in fork\n");
                        }
                        if (pid == 0) {
                            client_put_job(filename);//i figli non ritorna mai
                        }
                        break;
                    } else if (strncmp(conf_upload, "n", 1) == 0) {
                        printf("not confirm\n");
                        break;//esci e riscansiona l'input
                    }
                }
            }
        } else if (!is_blank(filename) && strcmp(command, "get") == 0) {
            //fai richiesta al server del file
            if ((pid = fork()) == -1) {
                handle_error_with_exit("error in fork\n");
            }
            if (pid == 0) {
                client_get_job(filename);//i figli non ritorna mai
            }
        } else if (1) {//list
            if ((pid = fork()) == -1) {
                handle_error_with_exit("error in fork\n");
            }
            if (pid == 0) {
                client_list_job();//i figli non ritorna mai
            }
        }
    }
    return EXIT_SUCCESS;
}
