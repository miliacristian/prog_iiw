#include "basic.h"
#include "io.h"
#include "parser.h"
#include "timer.h"
#include "Server.h"
#include "list_server.h"
#include "get_server.h"
#include "communication.h"
#include "put_server.h"


int main_sockfd,msgid,queue_mtx_id,mtx_prefork_id,great_alarm_serv=0;//dopo le fork tutti i figli
// sanno quali sono gli id
struct select_param param_serv;
char*dir_server;

void timeout_handler_serv(int sig, siginfo_t *si, void *uc){//gestione del segnale alarm
    (void)sig;
    (void)si;
    (void)uc;
    great_alarm_serv=1;
}

void initialize_mtx_prefork(struct mtx_prefork*mtx_prefork){//inizializza memoria condivisa contentente semaforo
    //impostando numero di processi liberi a 0
    if(mtx_prefork==NULL){
        handle_error_with_exit("error in initialize_mtx_prefork\n");
    }
    if(sem_init(&(mtx_prefork->sem),1,1)==-1){
        handle_error_with_exit("error in sem_init\n");
    }
    mtx_prefork->free_process=0;
    return;
}

void reply_to_syn_and_execute_command(struct msgbuf request){//prendi dalla coda il messaggio di syn
    struct sockaddr_in serv_addr;
    struct temp_buffer temp_buff;//pacchetto da inviare
    struct shm_sel_repeat *shm=malloc(sizeof(struct shm_sel_repeat));
    if(shm==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    initialize_mtx(&(shm->mtx));
    initialize_cond(&(shm->list_not_empty));
    shm->fd=-1;
    shm->dimension=-1;
    shm->filename=NULL;
    shm->list=NULL;
    shm->byte_readed=0;
    shm->byte_written=0;
    shm->byte_sent=0;
    shm->addr.dest_addr=request.addr;
    shm->pkt_fly=0;
    shm->window_base_rcv=0;
    shm->window_base_snd=0;
    shm->win_buf_snd=0;
    shm->seq_to_send=0;
    shm->addr.len=sizeof(request.addr);
    shm->param.window=param_serv.window;//primo pacchetto della finestra->primo non riscontrato
    if(param_serv.timer_ms !=0 ) {
        shm->param.timer_ms = param_serv.timer_ms;
        shm->adaptive = 0;
    }
    else{
        shm->param.timer_ms = TIMER_BASE_ADAPTIVE;
        shm->adaptive = 1;
        shm->dev_RTT_ms=0;
        shm->est_RTT_ms=TIMER_BASE_ADAPTIVE;
    }
    shm->param.loss_prob=param_serv.loss_prob;
    shm->head=NULL;
    shm->tail=NULL;
    shm->win_buf_rcv=malloc(sizeof(struct window_rcv_buf)*(2*(param_serv.window)));
    if(shm->win_buf_rcv==NULL){
        handle_error_with_exit("error in malloc win buf rcv\n");
    }
    shm->win_buf_snd=malloc(sizeof(struct window_snd_buf)*(2*(param_serv.window)));
    if(shm->win_buf_snd==NULL){
        handle_error_with_exit("error in malloc win buf snd\n");
    }
    memset(shm->win_buf_rcv,0,sizeof(struct window_rcv_buf)*(2*(param_serv.window)));//inizializza a zero
    memset(shm->win_buf_snd,0,sizeof(struct window_snd_buf)*(2*(param_serv.window)));//inizializza a zero
    for (int i = 0; i < 2 *(param_serv.window); i++) {
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
    for (int i = 0; i < 2 *(param_serv.window); i++) {
        shm->win_buf_snd[i].lap = -1;
        shm->win_buf_snd[i].acked=2;
    }
    for (int i = 0; i < 2 *(param_serv.window); i++) {
        shm->win_buf_rcv[i].lap = -1;
    }
    memset((void *)&serv_addr, 0, sizeof(serv_addr));//inizializzo socket del processo ad ogni nuova richiesta
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(0);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((shm->addr.sockfd= socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(shm->addr.sockfd, (struct sockaddr *)&(serv_addr), sizeof(serv_addr)) < 0) {//bind con una porta scelta automataticam. dal SO
        handle_error_with_exit("error in bind\n");
    }
    //manda syn ack dopo aver ricevuto il syn e aspetta il comando del client
    send_syn_ack(shm->addr.sockfd, &request.addr, sizeof(request.addr),0 ); //ultimo parametro è param_serv.loss_prob!!!!
    alarm(TIMEOUT);
    if(recvfrom(shm->addr.sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr *)&(shm->addr.dest_addr),&(shm->addr.len))!=-1){//ricevi il comando del client in finestra
        //bloccati finquando non ricevi il comando dal client
        alarm(0);
        print_rcv_message(temp_buff);
        printf(GREEN"comando %s ricevuto connessione instaurata\n"RESET,temp_buff.payload);
        great_alarm_serv=0;
        //in base al comando ricevuto il processo figlio server esegue uno dei 3 comandi
        if(temp_buff.command==LIST){
            execute_list(temp_buff,shm);
        }
        else if(temp_buff.command==PUT){
            set_max_buff_rcv_size(shm->addr.sockfd);
            execute_put(temp_buff,shm);
            if(close(shm->addr.sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
        }
        else if(temp_buff.command==GET){
            execute_get(temp_buff,shm);
            if(close(shm->addr.sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
        }
        else if(temp_buff.command==SYN_ACK || temp_buff.command==SYN){
            printf("pacchetto di connessione ricevuto e ignorato\n");
        }
        else{
            printf("invalid_command\n");
            if(close(shm->addr.sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
        }
    }
    else if(errno!=EINTR && errno!=0){
        handle_error_with_exit("error in send_syn_ack recvfrom\n");
    }
    if(great_alarm_serv==1){
        great_alarm_serv=0;
        printf("il client non è in ascolto\n");
        return ;
    }
    //libera la memoria della shared memory a fine lavoro
    for (int i = 0; i < 2 *(param_serv.window); i++) {
        free(shm->win_buf_snd[i].payload);
        shm->win_buf_snd[i].payload=NULL;
        free(shm->win_buf_rcv[i].payload);
        shm->win_buf_rcv[i].payload=NULL;
    }
    free(shm->win_buf_rcv);
    free(shm->win_buf_snd);
    shm->win_buf_snd=NULL;
    shm->win_buf_rcv=NULL;
    free(shm);
    shm=NULL;
    return;
}

void child_job() {//lavoro che deve svolgere il processo.
    //for(;;){
    //prende la richiesta dalla coda;
    // risponde al client;
    // soddisfala richiesta;
    //}
    struct msgbuf request;//contiene indirizzo del client da servire
    int value;
    char done_jobs=0;
    struct sigaction sa_timeout;
    struct mtx_prefork*mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);//ottieni puntatore
    // alla regione di memoria condivisa dei processi
    memset(&sa_timeout,0,sizeof(struct sigaction));
    unlock_signal(SIGALRM);
    sem_t *mtx=(sem_t*)attach_shm(queue_mtx_id);//ottieni puntatore
    // alla regione di memoria condivisa dei processi
    if(close(main_sockfd)==-1){//chiudi il socket del padre
        handle_error_with_exit("error in close socket fd\n");
    }

    sa_timeout.sa_sigaction = timeout_handler_serv;//disposizione per segnale alarm
    if (sigemptyset(&sa_timeout.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGALRM, &sa_timeout, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }

    for(;;){
        lock_sem(&(mtx_prefork->sem));//semaforo numero processi
        if(mtx_prefork->free_process>=NUM_FREE_PROCESS){
            printf("troppi processi liberi,suicidio del processo %d\n",getpid());
            unlock_sem(&(mtx_prefork->sem));
            exit(EXIT_SUCCESS);
        }
        mtx_prefork->free_process+=1;
        unlock_sem(&(mtx_prefork->sem));
        lock_sem(mtx);
        printf("processo %d disponibile e in attesa\n",getpid());
        value=(int)msgrcv(msgid,&request,sizeof(struct msgbuf)-sizeof(long),0,0);
        unlock_sem(mtx);//non è un problema prendere il mutex e bloccarsi in coda
        if(value==-1){//errore msgrcv
            lock_sem(&(mtx_prefork->sem));
            mtx_prefork->free_process-=1;
            unlock_sem(&(mtx_prefork->sem));
            handle_error_with_exit("errore in msgrcv\n");
        }
        lock_sem(&(mtx_prefork->sem));
        printf("processo %d svolge la richiesta presa dalla coda\n",getpid());
        mtx_prefork->free_process-=1;
        unlock_sem(&(mtx_prefork->sem));
        reply_to_syn_and_execute_command(request);//soddisfa la richiesta
        done_jobs++;//incrementa numero di lavori svolti
        if(done_jobs>MAX_PROC_JOB){
            printf("processo %d ha fatto molto lavoro e non svolgerà più richieste\n",getpid());
            exit(EXIT_SUCCESS);
        }
    }
    return;
}
void create_pool(int num_child){//crea il pool di processi.
// Ogni processo ha il compito di gestire le richieste
    int pid;
    if(num_child<0){
        handle_error_with_exit("num_child must be greater than 0\n");
    }
    for(int i=0;i<num_child;i++) {
        if ((pid = fork()) == -1) {
            handle_error_with_exit("error in fork\n");
        }
        if (pid == 0) {
            child_job();//i figli non ritorna mai
        }
    }
    return;//il padre ritorna dopo aver creato i processi
}
void*pool_handler_job(void*arg){//thread che gestisce il pool dei processi del client
    printf("thread pool handler creato\n");
    struct mtx_prefork*mtx_prefork=arg;
    int left_process;
    pid_t pid;
    block_signal(SIGALRM);
    for(;;){
        lock_sem(&(mtx_prefork->sem));
        if(mtx_prefork->free_process<NUM_FREE_PROCESS){
            left_process=NUM_FREE_PROCESS-mtx_prefork->free_process;
            //printf("thread crea %d processi\n",left_process);
            unlock_sem(&(mtx_prefork->sem));
            create_pool(left_process);//crea i processi rimanenti per arrivare a NUM_FREE_PROCESS
        }
        else{
            unlock_sem(&(mtx_prefork->sem));
        }
        while((pid=waitpid(-1,NULL,WNOHANG))>0) {
            //printf("thread libera risorse del processo %d\n", pid);
        }
    }
    return NULL;
}

void create_thread_pool_handler(struct mtx_prefork*mtx_prefork){
//crea il gestore(thread) della riserva di processi
    if(mtx_prefork==NULL){
        handle_error_with_exit("error in create thread_pool_handler\n");
    }
    pthread_t tid;
    if(pthread_create(&tid,NULL,pool_handler_job,mtx_prefork)!=0){
        handle_error_with_exit("error in create_pool_handler\n");
    }
    block_signal(SIGALRM);
    return;
}

int main(int argc,char*argv[]) {//funzione principale processo server
    int fd,readed;
    socklen_t len;
    char commandBuffer[MAXCOMMANDLINE+1],*line,*command,localname[80];
    struct sockaddr_in addr,cliaddr;
    struct msgbuf msgbuf;//struttura del messaggio della coda

    struct mtx_prefork*mtx_prefork;//mutex tra processi e thread pool handler
    sem_t*mtx_queue;//semaforo tra i processi che provano ad accedere alla coda di messaggi

    if(argc!=2){
        handle_error_with_exit("usage <directory>\n");
    }
    srand(time(NULL));
    check_if_dir_exist(argv[1]);//verifica che directory passata come parametro esiste
    dir_server=add_slash_to_dir(argv[1]);
    //verifica che il file parameter.txt
    better_strcpy(localname,"./parameter.txt");
    fd=open(localname,O_RDONLY);
    if(fd==-1){
        handle_error_with_exit("parameter.txt in ./ not found\n");
    }
    line=malloc(sizeof(char)*MAXLINE);
    if(line==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    command=line;
    memset(line,'\0',MAXLINE);
    readed=readline(fd,line,MAXLINE);//leggi parametri di esecuzione dal file parameter
    if(count_word_in_buf(line)!=3){
        handle_error_with_exit("parameter.txt must contains 3 parameters <window><loss_prob><timer>\n");
    }
    if(readed<=0){
        handle_error_with_exit("error in read line\n");
    }
    //inizializza parametri di esecuzione
    param_serv.window=parse_integer_and_move(&line);
    if(param_serv.window<1){
        handle_error_with_exit("window must be greater than 0\n");
    }
    skip_space(&line);
    param_serv.loss_prob=parse_double_and_move(&line);
    if(param_serv.loss_prob<0 || param_serv.loss_prob>100){
        handle_error_with_exit("invalid loss prob\n");
    }
    skip_space(&line);
    param_serv.timer_ms=parse_integer_and_move(&line);
    if(param_serv.timer_ms<0){
        handle_error_with_exit("timer must be positive or 0\n");
    }
    if(close(fd)==-1){
        handle_error_with_exit("error in close file\n");
    }
    free(command);//liberazione memoria della linea retta dal file
    line=NULL;

    //inizializza memorie condivise contenenti semafori e inizializza coda
    mtx_prefork_id=get_id_shared_mem(sizeof(struct mtx_prefork));
    queue_mtx_id=get_id_shared_mem(sizeof(sem_t));
    msgid=get_id_msg_queue();//crea coda di messaggi id globale

    mtx_queue=(sem_t*)attach_shm(queue_mtx_id);//mutex per accedere alla coda
    mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);//mutex tra processi e pool handler
    initialize_sem(mtx_queue);//inizializza memoria condivisa
    initialize_mtx_prefork(mtx_prefork);//inizializza memoria condivisa

    memset((void *)&addr, 0, sizeof(addr));//inizializza socket processo principale
    addr.sin_family=AF_INET;
    addr.sin_port=htons(SERVER_PORT);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((main_sockfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) {
        handle_error_with_exit("error in socket create\n");
    }

    if (bind(main_sockfd,(struct sockaddr*)&addr,sizeof(addr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }

    create_pool(NUM_FREE_PROCESS);//crea il pool di NUM_FREE_PROCESS processi
    create_thread_pool_handler(mtx_prefork);//crea il thread che gestisce la riserva di processi
    while(1) {
        len=sizeof(cliaddr);
        if ((recvfrom(main_sockfd, commandBuffer, MAXCOMMANDLINE, 0, (struct sockaddr *) &cliaddr, &len)) < 0) {
            handle_error_with_exit("error in recvcommand");//memorizza  l'indirizzo del client e lo scrive in coda
        }
        printf(GREEN"è stata inviata una richiesta al processo centrale\n"RESET);
        msgbuf.addr=cliaddr;//inizializza la struct con addr
        msgbuf.mtype=1;
        if(msgsnd(msgid,&msgbuf,sizeof(struct msgbuf)-sizeof(long),0)==-1){//inserisce nella coda l'indirizzo del client
            handle_error_with_exit("error in msgsnd\n");
        }
    }
    return EXIT_SUCCESS;
}