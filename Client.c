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
#include "put_client.h"

int great_alarm_client = 0;//se diventa 1 è scattato il timer globale
struct select_param param_client;
char *dir_client;

void add_slash_to_dir_client(char*argument){//aggiunge "/" ad un char*path se non presente
    if(argument==NULL){
        handle_error_with_exit("error in add_slah\n");
    }
    if ((argument[strlen(argument) - 1]) != '/') {
        dir_client = malloc(strlen(argument) + 2);//2== "/" +terminatore
        if (dir_client == NULL) {
            handle_error_with_exit("error in malloc\n");
        }
        memset(dir_client, '\0', strlen(argument) + 2);
        better_strcpy(dir_client, argument);
        better_strcat(dir_client, "/");
        dir_client[strlen(argument) + 2] = '\0';
    } else {
        dir_client = argument;
    }
    return;
}

void timeout_handler_client(int sig, siginfo_t *si, void *uc){//signal handler del timer globale
    (void)sig;
    (void)si;
    (void)uc;
    printf("%d tid\n",pthread_self());//da togliere
    great_alarm_client=1;
    return;
}

int get_command(int sockfd, struct sockaddr_in serv_addr, char *filename) {//svolgi la get con connessione già instaurata
    int byte_written=0;
    struct shm_sel_repeat *shm=malloc(sizeof(struct shm_sel_repeat));//alloca memoria condivisa thread
    if(filename==NULL){
        handle_error_with_exit("error in get_command\n");
    }
    if(shm==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    //inizializza memoria condivisa thread
    initialize_mtx(&(shm->mtx));
    initialize_cond(&(shm->list_not_empty));
    shm->fd=-1;
    shm->byte_written=0;
    shm->byte_sent=0;
    shm->list=NULL;
    shm->pkt_fly=0;
    shm->byte_readed=0;
    shm->window_base_rcv=0;
    shm->window_base_snd=0;
    shm->win_buf_snd=0;
    shm->seq_to_send=0;
    shm->param.window=param_client.window;
    shm->param.loss_prob=param_client.loss_prob;
    if(param_client.timer_ms !=0 ) {
        shm->param.timer_ms = param_client.timer_ms;
        shm->adaptive = 0;
    }
    else{
        shm->param.timer_ms = TIMER_BASE_ADAPTIVE;
        shm->adaptive = 1;
        shm->dev_RTT_ms=0;
        shm->est_RTT_ms=TIMER_BASE_ADAPTIVE;
    }
    printf("dev_RTT %f est_RTT %f\n", shm->dev_RTT_ms, shm->est_RTT_ms);
    printf("timer iniziale %d\n", shm->param.timer_ms);
    shm->addr.sockfd=sockfd;
    shm->addr.dest_addr=serv_addr;
    shm->dimension=-1;
    shm->filename=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD));
    shm->addr.len=sizeof(serv_addr);
    shm->head=NULL;
    shm->tail=NULL;
    if(shm->filename==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    better_strcpy(shm->filename,filename);
    shm->win_buf_rcv=malloc(sizeof(struct window_rcv_buf)*(2*param_client.window));
    if(shm->win_buf_rcv==NULL){
        handle_error_with_exit("error in malloc win buf rcv\n");
    }
    shm->win_buf_snd=malloc(sizeof(struct window_snd_buf)*(2*param_client.window));
    if(shm->win_buf_snd==NULL){
        handle_error_with_exit("error in malloc win buf snd\n");
    }
    memset(shm->win_buf_rcv, 0, sizeof(struct window_rcv_buf) * (2 * param_client.window));//inizializza a zero
    memset(shm->win_buf_snd, 0, sizeof(struct window_snd_buf) * (2 * param_client.window));//inizializza a zero
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].payload =malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_snd[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(shm->win_buf_snd[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
        shm->win_buf_rcv[i].payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_rcv[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(shm->win_buf_rcv[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
    }
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].lap = -1;
        shm->win_buf_snd[i].acked=2;
    }
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_rcv[i].lap = -1;
    }
    set_max_buff_rcv_size(shm->addr.sockfd);
    get_client(shm);
    byte_written=shm->byte_written;
    //libera memoria
    for (int i = 0; i < 2 *(param_client.window); i++) {
        free(shm->win_buf_snd[i].payload);
        shm->win_buf_snd[i].payload=NULL;
        free(shm->win_buf_rcv[i].payload);
        shm->win_buf_rcv[i].payload=NULL;
    }
    free(shm->win_buf_rcv);
    free(shm->win_buf_snd);
    shm->win_buf_rcv=NULL;
    shm->win_buf_snd=NULL;
    free(shm);
    shm=NULL;
    return byte_written;
}

int list_command(int sockfd, struct sockaddr_in serv_addr) {//svolgi la list con connessione già instaurata
    int byte_readed=0;
    struct shm_sel_repeat *shm=malloc(sizeof(struct shm_sel_repeat));//alloca memoria condivisa thread
    if(shm==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    //inizializza memoria condivisa thread
    initialize_mtx(&(shm->mtx));
    initialize_cond(&(shm->list_not_empty));
    shm->fd=-1;
    shm->byte_written=0;
    shm->byte_sent=0;
    shm->list=NULL;
    shm->pkt_fly=0;
    shm->byte_readed=0;
    shm->window_base_rcv=0;
    shm->window_base_snd=0;
    shm->win_buf_snd=0;
    shm->seq_to_send=0;
    shm->param.window=param_client.window;
    shm->param.loss_prob=param_client.loss_prob;
    if(param_client.timer_ms !=0 ) {
        shm->param.timer_ms = param_client.timer_ms;
        shm->adaptive = 0;
    }
    else{
        shm->param.timer_ms = TIMER_BASE_ADAPTIVE;
        shm->adaptive = 1;
        shm->dev_RTT_ms=0;
        shm->est_RTT_ms=TIMER_BASE_ADAPTIVE;
    }
    printf("dev_RTT %f est_RTT %f", shm->dev_RTT_ms, shm->est_RTT_ms);
    printf("timer iniziale %d\n", shm->param.timer_ms);
    shm->addr.sockfd=sockfd;
    shm->addr.dest_addr=serv_addr;
    shm->dimension=-1;
    shm->filename=NULL;
    shm->addr.len=sizeof(serv_addr);
    shm->head=NULL;
    shm->tail=NULL;
    shm->win_buf_rcv=malloc(sizeof(struct window_rcv_buf)*(2*param_client.window));
    if(shm->win_buf_rcv==NULL){
        handle_error_with_exit("error in malloc win buf rcv\n");
    }
    shm->win_buf_snd=malloc(sizeof(struct window_snd_buf)*(2*param_client.window));
    if(shm->win_buf_snd==NULL){
        handle_error_with_exit("error in malloc win buf snd\n");
    }
    memset(shm->win_buf_rcv, 0, sizeof(struct window_rcv_buf) * (2 * param_client.window));//inizializza a zero
    memset(shm->win_buf_snd, 0, sizeof(struct window_snd_buf) * (2 * param_client.window));//inizializza a zero
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].payload =malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_snd[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(shm->win_buf_snd[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
        shm->win_buf_rcv[i].payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_rcv[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(shm->win_buf_rcv[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
    }
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].lap = -1;
        shm->win_buf_snd[i].acked=2;
    }
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_rcv[i].lap = -1;
    }
    set_max_buff_rcv_size(shm->addr.sockfd);
    list_client(shm);
    byte_readed=shm->byte_readed;
    //libera memoria
    for (int i = 0; i < 2 *(param_client.window); i++) {
        free(shm->win_buf_snd[i].payload);
        shm->win_buf_snd[i].payload=NULL;
        free(shm->win_buf_rcv[i].payload);
        shm->win_buf_rcv[i].payload=NULL;
    }
    free(shm->win_buf_rcv);
    free(shm->win_buf_snd);
    shm->win_buf_rcv=NULL;
    shm->win_buf_snd=NULL;
    free(shm);
    shm=NULL;
    return byte_readed;
}

int put_command(int sockfd, struct sockaddr_in serv_addr, char *filename,int dimension) {//svolgi la put con connessione già instaurata
    int byte_readed=0;
    char*path;
    if(filename==NULL){
        handle_error_with_exit("error in put command\n");
    }
    path=generate_full_pathname(filename,dir_client);
    if(path==NULL){
        handle_error_with_exit("error in generate full path\n");
    }
    struct shm_sel_repeat *shm=malloc(sizeof(struct shm_sel_repeat));//alloca memoria condivisa thread
    if(shm==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    //inizializza memoria condivisa thread
    initialize_mtx(&(shm->mtx));
    initialize_cond(&(shm->list_not_empty));
    shm->fd=-1;
    shm->byte_written=0;
    shm->byte_sent=0;
    shm->list=NULL;
    shm->pkt_fly=0;
    shm->byte_readed=0;
    shm->window_base_rcv=0;
    shm->window_base_snd=0;
    shm->win_buf_snd=0;
    shm->seq_to_send=0;
    shm->param.window=param_client.window;
    shm->param.loss_prob=param_client.loss_prob;
    if(param_client.timer_ms !=0 ) {
        shm->param.timer_ms = param_client.timer_ms;
        shm->adaptive = 0;
    }
    else{
        shm->param.timer_ms = TIMER_BASE_ADAPTIVE;
        shm->adaptive = 1;
        shm->dev_RTT_ms=0;
        shm->est_RTT_ms=TIMER_BASE_ADAPTIVE;
    }
    printf("dev_RTT %f est_RTT %f", shm->dev_RTT_ms, shm->est_RTT_ms);
    printf("timer iniziale %d\n", shm->param.timer_ms);
    shm->addr.sockfd=sockfd;
    shm->addr.dest_addr=serv_addr;
    shm->dimension=dimension;
    shm->filename=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD));
    shm->addr.len=sizeof(serv_addr);
    shm->head=NULL;
    shm->tail=NULL;
    if(shm->filename==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    better_strcpy(shm->filename,filename);
    if(!calc_file_MD5(path,shm->md5_sent)){
        handle_error_with_exit("error in calculate md5\n");
    }
    free(path);
    path=NULL;
    printf("md5 %s\n",shm->md5_sent);
    shm->win_buf_rcv=malloc(sizeof(struct window_rcv_buf)*(2*param_client.window));
    if(shm->win_buf_rcv==NULL){
        handle_error_with_exit("error in malloc win buf rcv\n");
    }
    shm->win_buf_snd=malloc(sizeof(struct window_snd_buf)*(2*param_client.window));
    if(shm->win_buf_snd==NULL){
        handle_error_with_exit("error in malloc win buf snd\n");
    }
    memset(shm->win_buf_rcv, 0, sizeof(struct window_rcv_buf) * (2 * param_client.window));//inizializza a zero
    memset(shm->win_buf_snd, 0, sizeof(struct window_snd_buf) * (2 * param_client.window));//inizializza a zero
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].payload =malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_snd[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(shm->win_buf_snd[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
        shm->win_buf_rcv[i].payload=malloc(sizeof(char)*(MAXPKTSIZE-OVERHEAD+1));
        if(shm->win_buf_rcv[i].payload==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(shm->win_buf_rcv[i].payload,'\0',MAXPKTSIZE-OVERHEAD+1);
    }
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_snd[i].lap = -1;
        shm->win_buf_snd[i].acked=2;
    }
    for (int i = 0; i < 2 *(param_client.window); i++) {
        shm->win_buf_rcv[i].lap = -1;
    }
    put_client(shm);
    byte_readed=shm->byte_readed;
    //libera memoria
    for (int i = 0; i < 2 *(param_client.window); i++) {
        free(shm->win_buf_snd[i].payload);
        shm->win_buf_snd[i].payload=NULL;
        free(shm->win_buf_rcv[i].payload);
        shm->win_buf_rcv[i].payload=NULL;
    }
    free(shm->win_buf_rcv);
    free(shm->win_buf_snd);
    shm->win_buf_rcv=NULL;
    shm->win_buf_snd=NULL;
    free(shm);
    shm=NULL;
    return byte_readed;
}

struct sockaddr_in send_syn_recv_ack(int sockfd, struct sockaddr_in main_servaddr) {//client manda messaggio syn
// e processo server figlio risponde con ack cosi il client sa chi contattare per eseguire i comandi put,list e get
    char rtx = 0;
    struct sigaction sa;
    socklen_t len = sizeof(main_servaddr);
    struct temp_buffer temp_buff;
    sa.sa_flags =SA_SIGINFO;
    sa.sa_sigaction = timeout_handler_client;
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    errno=0;
    while (rtx < 500000 ) {//parametro da cambiare
        send_syn(sockfd, &main_servaddr, sizeof(main_servaddr), 0);  //mando syn al processo server principale  //da cambiare loss prob
        printf("mi metto in ricezione del syn_ack\n");
        alarm(2);
        if (recvfrom(sockfd,&temp_buff,MAXPKTSIZE, 0, (struct sockaddr *) &main_servaddr, &len) !=-1) {//ricevo il syn_ack del server,solo qui sovrascrivo la struct
            if (temp_buff.command == SYN_ACK) {
                alarm(0);
                printf(GREEN"pacchetto syn_ack ricevuto,connessione instaurata\n" RESET);
                great_alarm_client = 0;
                return main_servaddr;//ritorna l'indirizzo del processo figlio del server
            }
            else {
                printf("pacchetto con comando diverso da syn_ack ignorato\n");
            }
        }
        if(errno!=EINTR && errno!=0){
            handle_error_with_exit("error in recvfrom send_syn\n");
        }
        rtx++;
    }
    great_alarm_client = 0;
    handle_error_with_exit("il server non è in ascolto\n");
    return main_servaddr;
}

void client_list_job() {//inizializza socket ricevi indirizzo del processo server figlio e inizia comando list
    struct sockaddr_in serv_addr, cliaddr;
    int sockfd;
    printf("client list job\n");
    memset((void *) &serv_addr, 0, sizeof(serv_addr));//inizializza struct per contattare il server principale
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton\n");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {//inizializza socket del client
        handle_error_with_exit("error in socket\n");
    }
    memset((void *) &cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    set_max_buff_rcv_size(sockfd);
    serv_addr = send_syn_recv_ack(sockfd, serv_addr);//ottieni l'indirizzo per contattare un child_process_server
    list_command(sockfd, serv_addr);
    printf("finito comando list\n");
    close(sockfd);
    exit(EXIT_SUCCESS);
}

void client_get_job(char *filename) {//inizializza socket ricevi indirizzo del processo server figlio e inizia comando get
    printf("client get job\n");
    struct sockaddr_in serv_addr, cliaddr;
    int sockfd;
    if(filename==NULL){
        handle_error_with_exit("error in client_get_job\n");
    }
    memset((void *) &serv_addr, 0, sizeof(serv_addr));//inizializza struct per contattare il server principale
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton\n");
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {//inizializza socket del client
        handle_error_with_exit("error in socket\n");
    }
    memset((void *) &cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    set_max_buff_rcv_size(sockfd);
    serv_addr = send_syn_recv_ack(sockfd, serv_addr);//ottieni l'indirizzo per contattare un child_process_server
    get_command(sockfd, serv_addr, filename);
    printf("finito comando get\n");
    close(sockfd);
    exit(EXIT_SUCCESS);
}

void client_put_job(char *filename,int dimension) {//upload e filename già verificato,inizializza socket ricevi indirizzo del processo server figlio e inizia comando put
    printf("client put_job\n");
    struct sockaddr_in serv_addr, cliaddr;
    int sockfd;
    if(filename==NULL){
        handle_error_with_exit("error in client_put_job\n");
    }
    memset((void *) &serv_addr, 0, sizeof(serv_addr));//inizializza struct per contattare il server principale
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton\n");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {//inizializza socket del client
        handle_error_with_exit("error in socket\n");
    }
    memset((void *) &cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(0);
    if (bind(sockfd, (struct sockaddr *) &cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    serv_addr = send_syn_recv_ack(sockfd, serv_addr);//ottieni l'indirizzo per contattare un child_process_server
    put_command(sockfd, serv_addr, filename,dimension);
    printf("finito comando put\n");
    close(sockfd);
    exit(EXIT_SUCCESS);
}

void *thread_job(void *arg) {//thread che esegue waitpid dei processi del client
    (void) arg;

    pid_t pid;
    block_signal(SIGALRM);
    while (1) {
        while ((pid = waitpid(-1, NULL,WNOHANG)) > 0) {
            printf("pool handler libera risorse del processo %d\n", pid);
        }
    }
    return NULL;
}

void create_thread_waitpid() {//crea il thread che esegue waitpid
    pthread_t tid;
    pthread_create(&tid, 0, thread_job, NULL);
    return;
}


int main(int argc, char *argv[]) {//funzione principale client concorrente
    char *filename, *command, conf_upload[4], *line, localname[80],*my_list;
    //non fare mai la free su filename e command(servono ad ogni richiesta)
    int path_len, fd;
    pid_t pid;
    if (argc != 2) {
        handle_error_with_exit("usage <directory>\n");
    }
    srand(time(NULL));
    //verifica che il file parameter.txt esista
    check_if_dir_exist(argv[1]);
    add_slash_to_dir_client(argv[1]);
    better_strcpy(localname,"./parameter.txt");
    fd = open(localname, O_RDONLY);//apre il file parameter per leggere i parametri di input
    if (fd == -1) {
        handle_error_with_exit("parameter.txt in ./ not found\n");
    }
    line = malloc(sizeof(char)*MAXLINE);//alloca buffer per leggere dal file parameter.txt
    if(line==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    command = line;
    memset(line, '\0', MAXLINE);
    if (readline(fd, line, MAXLINE) <= 0) {
        handle_error_with_exit("error in read line\n");
    }
    //inizializza i parametri di esecuzione
    if(count_word_in_buf(line)!=3){
        handle_error_with_exit("parameter.txt must contains 3 parameters <W><loss_prob><timer>\n");
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
    param_client.timer_ms = parse_integer_and_move(&line);
    if (param_client.timer_ms < 0) {
        handle_error_with_exit("timer must be positive or zero\n");
    }
    path_len = (int)strlen(dir_client);
    if (close(fd) == -1) {
        handle_error_with_exit("error in close file\n");
    }
    free(command);
    line=NULL;
    create_thread_waitpid();

    if ((command = malloc(sizeof(char) * 8)) == NULL) {//contiene il comando digitato,8==lunghezza massima:my+" "+list\0,list\0,get\0 o put\0
        handle_error_with_exit("error in malloc buffercommand\n");
    }
    if ((filename = malloc(sizeof(char) * (MAXFILENAME))) == NULL) {//contiene il filename del comando digitato
        handle_error_with_exit("error in malloc filename\n");
    }
    printf(YELLOW "Choose one command:\n1)list\n2)get <filename>\n3)put <filename>\n4)my list\n5)exit\n" RESET);
    for (;;) {//ciclo infinito che associa ad ogni comando che digita l'utente un processo che esegue il comando

        check_and_parse_command(command, filename);//inizializza command,filename e size
        if (!is_blank(filename) && (strncmp(command, "put",3) == 0)) {
            char *path = alloca(sizeof(char) * (strlen(filename) + path_len + 1));
            better_strcpy(path, dir_client);
            move_pointer(&path, path_len);
            better_strcpy(path, filename);
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
                            client_put_job(filename,get_file_size(path));//i figli non ritornano mai
                        }
                        break;
                    } else if (strncmp(conf_upload, "n", 1) == 0) {
                        printf("not confirm\n");
                        break;//esci e riscansiona l'input
                    }
                }
            }
        } else if (!is_blank(filename) && strncmp(command,"get",3) == 0) {
            //fai richiesta al server del file
            if ((pid = fork()) == -1) {
                handle_error_with_exit("error in fork\n");
            }
            if (pid == 0) {
                client_get_job(filename);//i figli non ritornano mai
            }
        } else if (strncmp(command,"list",4) == 0) {//list
            if ((pid = fork()) == -1) {
                handle_error_with_exit("error in fork\n");
            }
            if (pid == 0) {
                client_list_job();//i figli non ritornano mai
            }
        }
        else if(strncmp(command,"my list",7) == 0){//comando my_list
            my_list=make_list(dir_client);
            printf(GREEN "%s" RESET,my_list);
            free(my_list);
        }
        else if(strncmp(command,"exit",4) == 0){
            exit(EXIT_SUCCESS);
        }
        else{
            handle_error_with_exit("comando errato\n");
        }
    }
    return EXIT_SUCCESS;
}
