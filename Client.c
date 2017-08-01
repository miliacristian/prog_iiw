#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include <time.h>
#include "timer.h"
#include "Client.h"
#include "get_client.h"

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
        temp_buf.command=win_buffer->command;
        resend_message(addr->sockfd, &temp_buf, &(addr->dest_addr), sizeof(addr->dest_addr), param_client.loss_prob);//ritrasmetto il pacchetto di cui è scaduto il timer
        start_timer((*win_buffer).time_id, &sett_timer);
    }
    else if(sig == SIGRTMIN+1){
        printf("great timeout expired\n");
        great_alarm = 1;
    }
    return;
}
int get_command(int sockfd, struct sockaddr_in serv_addr, char *filename) {//svolgi la get con connessione già instaurata
    int byte_written = 0,seq_to_send = 0, window_base_snd = 0, window_base_rcv = 0, W = param_client.window, pkt_fly = 0;;//primo pacchetto della finestra->primo non riscontrato
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
    byte_written= wait_for_dimension(sockfd, serv_addr, len, filename, byte_written , seq_to_send , window_base_snd , window_base_rcv, W, pkt_fly ,temp_buff ,win_buf_rcv,win_buf_snd);
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
