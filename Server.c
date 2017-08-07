#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include "timer.h"
#include "get_server.h"

//variabili globali
struct addr *addr = NULL;
struct itimerspec sett_timer, rst_timer;//timer e reset timer globali
int main_sockfd,msgid,child_mtx_id,mtx_prefork_id,great_alarm=0;//dopo le fork tutti i figli sanno quali sono gli id
struct select_param param_serv;
timer_t timeout_timer_id;
char*dir_server;

void timeout_handler(int sig, siginfo_t *si, void *uc){
    printf("tempo scaduto\n");
    great_alarm=1;
    return;
}
void timer_handler(int sig, siginfo_t *si, void *uc) {//ad ogni segnale è associata una struct che contiene i dati da ritrasmettere
        (void) sig;
        (void) si;
        (void) uc;
        struct window_snd_buf *win_buffer = si->si_value.sival_ptr;
        struct temp_buffer temp_buf;
        copy_buf1_in_buf2(temp_buf.payload, win_buffer->payload,MAXPKTSIZE-9);//dati del pacchetto da ritrasmettere,
        // si può evitare usando la struct win_buf_snd?
        temp_buf.ack = NOT_AN_ACK;
        temp_buf.seq = win_buffer->seq_numb;//numero di sequenza del pacchetto da ritrasmettere
        temp_buf.command=win_buffer->command;
        resend_message(addr->sockfd, &temp_buf, &(addr->dest_addr), sizeof(addr->dest_addr), param_serv.loss_prob);//ritrasmetto il pacchetto di cui è scaduto il timer
        start_timer((*win_buffer).time_id, &sett_timer);
    return;
}
void add_slash_to_dir_serv(char*argument){
    if((argument[strlen(argument)-1])!='/'){
        dir_server=malloc(strlen(argument)+2);//1 per "/" uno per terminatore
        if(dir_server==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(dir_server,'\0',strlen(argument)+2);
        strcpy(dir_server,argument);
        strcat(dir_server,"/");
        dir_server[strlen(argument)+2]='\0';
    }
    else {
        dir_server = argument;
    }
    return;
}

int execute_list(int sockfd,int *seq_to_send,struct temp_buffer temp_buff,int *window_base_rcv,int *window_base_snd,int *pkt_fly,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,struct sockaddr_in cli_addr){
    int byte_written=0,byte_readed,fd,byte_expected,W=param_serv.window;
    double timer=param_serv.timer_ms,loss_prob=param_serv.loss_prob;
    printf("server execute_list\n");
    return 0;
}

int execute_put(int sockfd,int *seq_to_send,struct temp_buffer temp_buff,int *window_base_rcv,int *window_base_snd,int *pkt_fly,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,struct sockaddr_in cli_addr){
    int byte_written=0,byte_readed,fd,byte_expected,W=param_serv.window;
    double timer=param_serv.timer_ms,loss_prob=param_serv.loss_prob;
    printf("server execute_put\n");
    return 0;
}

void initialize_mtx(sem_t*mtx){
    if(sem_init(mtx,1,1)==-1){
        handle_error_with_exit("error in sem_init\n");
    }
}
void initialize_mtx_prefork(struct mtx_prefork*mtx_prefork){
    if(sem_init(&(mtx_prefork->sem),1,1)==-1){
        handle_error_with_exit("error in sem_init\n");
    }
    mtx_prefork->free_process=0;
    return;
}

void reply_to_syn_and_execute_command(struct msgbuf request){//prendi dalla coda il messaggio di syn
    //e rispondi al client con syn_ack
    int sockfd;
    struct sockaddr_in serv_addr;
    socklen_t len=sizeof(request.addr);
    int seq_to_send =0,window_base_snd=0,window_base_rcv=0,W=param_serv.window, pkt_fly=0;//primo pacchetto della finestra->primo non riscontrato
    struct temp_buffer temp_buff;//pacchetto da inviare
    struct window_rcv_buf win_buf_rcv[2*W];
    struct window_snd_buf win_buf_snd[2 * W];
    struct addr temp_addr;

    memset((void *)&serv_addr, 0, sizeof(serv_addr));//inizializzo socket del processo ad ogni nuova richiesta
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(0);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(sockfd, (struct sockaddr *)&(serv_addr), sizeof(serv_addr)) < 0) {//bind con una porta scelta automataticam. dal SO
        handle_error_with_exit("error in bind\n");
    }

    memset(win_buf_rcv,0,sizeof(struct window_rcv_buf)*(2*W));//inizializza a zero
    memset(win_buf_snd,0,sizeof(struct window_snd_buf)*(2*W));//inizializza a zero
    for (int i = 0; i < 2 * W; i++) {
        win_buf_snd[i].seq_numb = i;
    }
    make_timers(win_buf_snd, W);//crea 2w timer
    set_timer(&sett_timer, param_serv.timer_ms);//inizializza struct necessaria per scegliere il timer

    temp_addr.sockfd = sockfd;
    temp_addr.dest_addr = request.addr;
    addr = &temp_addr;//inizializzo puntatore globale necessario per signal_handler

    send_syn_ack(sockfd, &request.addr, sizeof(request.addr),0 ); //ultimo parametro è param_serv.loss_prob!!!!
    start_timeout_timer(timeout_timer_id, 3000);
    if(recvfrom(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr *)&(request.addr),&len)!=-1){//ricevi il comando del client in finestra
        //bloccati finquando non ricevi il comando dal client
        stop_timeout_timer(timeout_timer_id);
        printf("pacchetto ricevuto con ack %d seq %d command %d dati %s:\n",temp_buff.ack,temp_buff.seq,temp_buff.command, temp_buff.payload);
        printf("comando %s ricevuto connessione instaurata\n",temp_buff.payload);
        great_alarm=0;
        window_base_rcv=(window_base_rcv+1)%(2*W);//pkt con num sequenza zero ricevuto
        if(temp_buff.command==LIST){
            execute_list(sockfd,&seq_to_send,temp_buff,&window_base_rcv,&window_base_snd,&pkt_fly,win_buf_rcv,win_buf_snd,request.addr);
            printf("comando list finito\n");
            if(close(sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
            return;
        }
        else if(temp_buff.command==PUT){
            execute_put(sockfd,&seq_to_send,temp_buff,&window_base_rcv,&window_base_snd,&pkt_fly,win_buf_rcv,win_buf_snd,request.addr);
            printf("comando put finito\n");
            if(close(sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
            return;
        }
        else if(temp_buff.command==GET){
            execute_get(sockfd, request.addr, len, &seq_to_send,&window_base_snd,&window_base_rcv,W, &pkt_fly,temp_buff, win_buf_rcv, win_buf_snd);
            printf("comando get finito\n");
            if(close(sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
            return;
        }
        else if(temp_buff.command==SYN_ACK || temp_buff.command==SYN){
            printf("pacchetto di connessione ricevuto e ignorato\n");
        }
        else{
            printf("invalid_command\n");
            if(close(sockfd)==-1){
                handle_error_with_exit("error in close socket child process\n");
            }
            return;
        }
    }
    else if(errno!=EINTR){
        handle_error_with_exit("error in send_syn_ack recvfrom\n");
    }
    if(great_alarm==1){
        great_alarm=0;
        printf("il client non è in ascolto\n");
        return ;
    }
    return;
}

void child_job(){//lavoro che deve svolgere il processo,loop infinito su get_request,satisfy request
    printf("pid %d\n",getpid());
    struct msgbuf request;//contiene comando e indirizzi del client
    int value;
    char done_jobs=0;
    struct sigaction sa,sa_timeout;

    struct mtx_prefork*mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);
    sem_t *mtx=(sem_t*)attach_shm(child_mtx_id);
    make_timeout_timer(&timeout_timer_id);
    if(close(main_sockfd)==-1){//chiudi il socket del padre
        handle_error_with_exit("error in close socket fd\n");
    }

    sa.sa_flags = SA_SIGINFO;//gestione segnali e sigrtmin
    sa.sa_sigaction = timer_handler;
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    sa_timeout.sa_sigaction = timeout_handler;
    if (sigemptyset(&sa_timeout.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGRTMIN+1, &sa_timeout, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    create_thread_signal_handler();
    for(;;){
        lock_sem(&(mtx_prefork->sem));//semaforo numero processi
        printf("mtx prefork\n");
        if(mtx_prefork->free_process>=5){
            printf("suicidio del pid %d\n",getpid());
            unlock_sem(&(mtx_prefork->sem));
            exit(EXIT_SUCCESS);
        }
        mtx_prefork->free_process+=1;
        unlock_sem(&(mtx_prefork->sem));
        lock_sem(mtx);
        printf("processo %d disponibile per una nuova richiesta\n",getpid());
        value=msgrcv(msgid,&request,sizeof(struct msgbuf)-sizeof(long),0,0);
        unlock_sem(mtx);//non è un problema prendere il mutex e bloccarsi in coda
        if(value==-1){//errore msgrcv
            lock_sem(&(mtx_prefork->sem));
            mtx_prefork->free_process-=1;
            unlock_sem(&(mtx_prefork->sem));
            handle_error_with_exit("an external error occured\n");
        }
        lock_sem(&(mtx_prefork->sem));
        printf("ho trovato una richiesta in coda\n");
        mtx_prefork->free_process-=1;
        unlock_sem(&(mtx_prefork->sem));
        reply_to_syn_and_execute_command(request);
        done_jobs++;
        if(done_jobs>10){
            printf("pid %d\n ha fatto molto lavoro!\n",getpid());
            exit(EXIT_SUCCESS);
        }
    }
    return;
}
void create_pool(int num_child){//crea il pool di processi,ogni processo ha il compito di gestire le richieste
    int pid;//da inizializzare num_child
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
    printf("pool handler\n");
    struct mtx_prefork*mtx_prefork=arg;
    int left_process;
    pid_t pid;
    for(;;){
        lock_sem(&(mtx_prefork->sem));
        if(mtx_prefork->free_process<5){
            left_process=5-mtx_prefork->free_process;
            printf("thread crea %d processi\n",left_process);
            unlock_sem(&(mtx_prefork->sem));
            create_pool(left_process);//se il thread è molto veloce nell'esecuzione
            // legge la variabile free process
            //piu di una volta e crea processi supplementari,non è un problema
        }
        else{
            unlock_sem(&(mtx_prefork->sem));
        }
        while((pid=waitpid(-1,NULL,WNOHANG))>0) {
            printf("thread libera risorse pid %d\n", pid);
        }
    }
    return NULL;
}


void create_thread_pool_handler(struct mtx_prefork*mtxPrefork){//funzione che crea il gestore(thread) della riserva
    // di processi
    pthread_t tid;
    if(pthread_create(&tid,NULL,pool_handler_job,mtxPrefork)!=0){
        handle_error_with_exit("error in create_pool_handler\n");
    }
    return;
}

int main(int argc,char*argv[]) {//i processi figli ereditano disposizione dei segnali,farla prima di crearli
    int fd,readed;
    size_t temp_len;
    socklen_t len;
    char commandBuffer[MAXCOMMANDLINE+1],*line,*command,localname[80];
    struct sockaddr_in addr,cliaddr;
    struct msgbuf msgbuf;

    struct mtx_prefork*mtx_prefork;//mutex tra processi e thread pool handler
    sem_t*mtx;//semaforo tra i processi che provano ad accedere alla coda di messaggi

    if(argc!=2){
        handle_error_with_exit("usage <directory>\n");
    }
    srand(time(NULL));
    check_if_dir_exist(argv[1]);
    add_slash_to_dir_serv(argv[1]);
    printf("%s\n",dir_server);
    strcpy(localname,"");
    strcpy(localname,getenv("HOME"));
    strcat(localname,"/parameter.txt");
    fd=open(localname,O_RDONLY);
    if(fd==-1){
        handle_error_with_exit("file parameter in /home/username/parameter.txt not found\n");
    }
    line=malloc(sizeof(char)*MAXLINE);
    if(line==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    command=line;
    memset(line,'\0',MAXLINE);
    readed=readline(fd,line,MAXLINE);
    /*if(count_word_in_buf(line)!=3){
        handle_error_with_exit("parameter.txt must contains 3 parameters <W><loss_prob><timer>\n");
    }*/
    if(readed<=0){
        handle_error_with_exit("error in read line\n");
    }
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
    line=command;
    free(line);
    command=NULL;

    mtx_prefork_id=get_id_shared_mem(sizeof(struct mtx_prefork));
    child_mtx_id=get_id_shared_mem(sizeof(sem_t));
    msgid=get_id_msg_queue();//crea coda di messaggi id globale

    mtx=(sem_t*)attach_shm(child_mtx_id);//mutex per accedere alla coda
    mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);//mutex tra processi e pool handler
    initialize_mtx(mtx);
    initialize_mtx_prefork(mtx_prefork);

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

    create_pool(1);//da cambiare
    //create_thread_pool_handler(mtx_prefork);//da decommentare

    while(1) {
        len=sizeof(cliaddr);
        if ((recvfrom(main_sockfd, commandBuffer, MAXCOMMANDLINE, 0, (struct sockaddr *) &cliaddr, &len)) < 0) {
            handle_error_with_exit("error in recvcommand");//scrive nella struct le info del client
            // e nel buffer il comando ricevuto dal client
        }
        printf("messaggio ricevuto server\n");
        msgbuf.addr=cliaddr;//inizializza la struct con addr e commandBuffer
        msgbuf.mtype=1;
        if(msgsnd(msgid,&msgbuf,sizeof(struct msgbuf)-sizeof(long),0)==-1){
            handle_error_with_exit("error in msgsnd\n");
        }
    }
    return EXIT_SUCCESS;
}