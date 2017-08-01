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
int msgid,child_mtx_id,mtx_prefork_id,great_alarm=0;//dopo le fork tutti i figli sanno quali sono gli id
struct select_param param_serv;
timer_t timeout_timer_id;
char*dir_server;

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
        resend_message(addr->sockfd, &temp_buf, &(addr->dest_addr), sizeof(addr->dest_addr), param_serv.loss_prob);//ritrasmetto il pacchetto di cui è scaduto il timer
        start_timer((*win_buffer).time_id, &sett_timer);
    }
    else if(sig == SIGRTMIN+1){
        printf("great timeout expired\n");
        great_alarm = 1;
    }
    return;
}

int execute_list(int sockfd,int seq_to_send,struct temp_buffer temp_buff,int window_base_rcv,int window_base_snd,int pkt_fly,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,struct sockaddr_in cli_addr){
    int byte_written=0,byte_readed,fd,byte_expected,W=param_serv.window;
    double timer=param_serv.timer_ms,loss_prob=param_serv.loss_prob;
    printf("server execute_list\n");
    return 0;
}

int execute_put(int sockfd,int seq_to_send,struct temp_buffer temp_buff,int window_base_rcv,int window_base_snd,int pkt_fly,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,struct sockaddr_in cli_addr){
    int byte_written=0,byte_readed,fd,byte_expected,W=param_serv.window;
    double timer=param_serv.timer_ms,loss_prob=param_serv.loss_prob;
    printf("server execute_put\n");
    return 0;
}

/*int execute_get(int sockfd,int seq_to_send,struct temp_buffer temp_buff,int window_base_rcv,int window_base_snd,int pkt_fly,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,struct sockaddr_in cli_addr){
    //verifica prima che il file con nome dentro temp_buffer esiste ,manda la dimensione, aspetta lo start e inizia a mandare il file,temp_buff contiene il pacchetto con comando get
    int byte_readed=0,fd,W=param_serv.window,byte_left;
    double timer=param_serv.timer_ms,loss_prob=param_serv.loss_prob;
    printf("server execute_get\n");
    char *command,*path,dim[11],first_pkt;
    socklen_t len=sizeof(cli_addr);
    path=generate_full_pathname(temp_buff.payload+4, dir_server);
    printf("%s\n",path);
    if(check_if_file_exist(path)){
        byte_left=get_file_size(path);
        printf("dimensione del file %d\n",byte_left);
        sprintf(dim, "%d",byte_left);
        strcpy(temp_buff.payload,dim);//scrivo dentro tem_buff la dimensione del file
        temp_buff.ack=temp_buff.seq;
        temp_buff.seq=seq_to_send;

        strcpy(win_buf_snd[seq_to_send].payload,temp_buff.payload);
        win_buf_snd[seq_to_send].seq_numb=temp_buff.seq;
        //copio dentro la finestra temp buffer
        //if(sendto(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr*)&cli_addr,sizeof(struct sockaddr_in))==-1){//manda richiesta del client al server
          //  handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        //}
        send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr),param_serv.loss_prob );
        //if(timer_settime(win_buf_snd[seq_to_send].time_id, 0, &sett_timer,NULL)==-1){
          //  handle_error_with_exit("error in timer_settime\n");
        //}
        start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
        //printf("pacchetto inviato con ack %d seq %d dati %s:\n",temp_buff.ack,temp_buff.seq,temp_buff.payload);
        seq_to_send=(seq_to_send+1)%(2*W);
        //wait for start send file
        start_timeout_timer(timeout_timer_id, 5000);
        while(1){
            if(recvfrom(sockfd,&temp_buff,sizeof(struct temp_buffer),0,(struct sockaddr*)&cli_addr, &len)!=-1){
                if(&temp_buff!=NULL) {//start ricevuto
                    stop_timer(timeout_timer_id);
                    printf("pacchetto ricevuto con ack %d seq %d dati %s:\n", temp_buff.ack, temp_buff.seq, temp_buff.payload);
                    if (timer_settime(win_buf_snd[temp_buff.ack].time_id, 0, &rst_timer, NULL) == -1) {//resetta timer
                        handle_error_with_exit("error in timer_settime\n");
                    }
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    window_base_snd = (window_base_snd + 1) % (2 * W);
                    break;//inizia trasmissione file
                }
            }
            else if(great_alarm==1){
                great_alarm=0;
                printf("il client non è in ascolto\n");
                stop_all_timers(win_buf_snd, W);
                return byte_readed;
            }
        }
    }
    else{//il file non esiste,mando un messaggio di errore
        printf("il file non esiste\n");
        //temp_buff.ack=ACK_ERROR;
        temp_buff.seq=seq_to_send;
        strcpy(temp_buff.payload,"il file non esiste\n");
        strcpy(win_buf_snd[seq_to_send].payload,temp_buff.payload);
        //if(sendto(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr*)&cli_addr,sizeof(struct sockaddr_in))==-1){//manda msg di errore
          //  handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        //}
        send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr),param_serv.loss_prob );
        if(timer_settime(win_buf_snd[seq_to_send].time_id, 0, &sett_timer,NULL)==-1){
            handle_error_with_exit("error in timer_settime\n");
        }
        start_timer(win_buf_snd[seq_to_send].time_id, &sett_timer);
        seq_to_send=(seq_to_send+1)%(2*W);
        start_timeout_timer(timeout_timer_id, 5000);
        while(1){//mi metto in ricezione dell'ultimo ack
            if(recvfrom(sockfd,&temp_buff,sizeof(struct temp_buffer),0,(struct sockaddr*)&cli_addr, &len)!=-1){
                stop_timer(timeout_timer_id);
                printf("pacchetto ricevuto con ack %d seq %d dati %s:\n",temp_buff.ack,temp_buff.seq,temp_buff.payload);
                if(temp_buff.ack==window_base_snd && temp_buff.seq==NOT_A_PKT){
                    window_base_snd = (window_base_snd + 1) % (2 * W);
                    if (timer_settime(win_buf_snd[temp_buff.ack].time_id, 0, &rst_timer, NULL) == -1) {//resetta timer
                        handle_error_with_exit("error in timer_settime\n");
                    }
                    stop_timer(win_buf_snd[temp_buff.ack].time_id);
                    window_base_snd = (window_base_snd + 1) % (2 * W);

                    temp_buff.ack=NOT_AN_ACK;
                    strcpy(temp_buff.payload,"FIN");

                    send_message(sockfd, &temp_buff, &cli_addr, sizeof(cli_addr),param_serv.loss_prob );
                    //printf("pacchetto inviato con ack %d seq %d dati %s:\n",temp_buff.ack,temp_buff.seq,temp_buff.payload);
                    stop_all_timers(win_buf_snd, W);
                    return byte_readed;
                }
            }
            else if(great_alarm==1){
                printf("il client non risponde\n");
                great_alarm=0;
                stop_all_timers(win_buf_snd, W);
                return byte_readed;
            }
        }
    }
    printf("inizio invio file\n");
    fd = open(path, O_RDONLY);
    if(fd == -1){
        handle_error_with_exit("error in open");
    }
    free(path);
    //se sono qui tempbuff contiene ack 0,seq 1, dati start
    start_timeout_timer(timeout_timer_id,5000);
    while (byte_left > 0) {
        if (pkt_fly < W) {//finquando i pacchetti inviati e non ancora riscontrati sono minori di W
            readn(fd, win_buf_snd[seq_to_send].payload,(MAXPKTSIZE - 4));//metto nel buffer il pacchetto proveniente dal file senza terminatore di stringa
            //controllo su readn??
            if(temp_buff.seq==1) {
                readn(fd, win_buf_snd[seq_to_send].payload,(MAXPKTSIZE - 4));
                temp_buff.ack=1;
                temp_buff.seq=seq_to_send;
                strcpy(temp_buff.payload, (win_buf_snd[seq_to_send].payload));//copio dentro temp_buf i dati
            }
            strcpy(temp_buff.payload, (win_buf_snd[seq_to_send].payload));//copio dentro temp_buf i dati del pacchetto con eventuali terminatori di stringa
            temp_buff.seq = seq_to_send;//memorizzo il numero di sequenza
            send_message(sockfd, &(temp_buff),&cli_addr, sizeof(cli_addr),param_serv.loss_prob);
            start_timer(win_buf_snd[seq_to_send].time_id,500);//avvio timer
            pkt_fly++;//segno che ho inviato un pacchetto
            seq_to_send = (seq_to_send + 1) % (2 * W);//incremento ultimo inviato
        }
        while (recvfrom(sockfd, &temp_buff, sizeof(int), MSG_DONTWAIT, (struct sockaddr * )&cli_addr, &len) != -1) {//flag non bloccante,
            //finquando ci sono ack prendili
            if (seq_is_in_window(window_base_snd, window_base_snd+ W - 1, W,temp_buff.ack)) {
                //se è dentro la finestra->ack con numero di sequenza dentro finestra ricevuto
                stop_timer(win_buf_snd[temp_buff.ack].time_id);
                stop_timer(timeout_timer_id);
                start_timeout_timer(timeout_timer_id,5000);
                win_buf_snd[temp_buff.ack].acked = 1;//segna pkt come riscontrato
                if (temp_buff.ack == window_base_snd) {//ricevuto ack del primo pacchetto non riscontrato->avanzo finestra
                    while (win_buf_snd[window_base_snd].acked == 1) {//finquando ho pacchetti riscontrati
                        //avanzo la finestra
                        byte_readed += strlen(win_buf_snd[window_base_snd].payload);//segno come letti
                        win_buf_snd[window_base_snd].acked = 0;//resetta quando scorri finestra
                        window_base_snd = (window_base_snd + 1) % (2 * W);//avanza la finestra
                        pkt_fly--;
                    }
                }
            }
            else {//se è fuori la finestra ignoralo
            }
        }
        if(great_alarm==1){
            great_alarm=0;
            printf("il client non risponde\n");
            return byte_readed;
        }
    }
    //fine trasmissione
    temp_buff.ack=NOT_AN_ACK;
    strcpy(temp_buff.payload,"FIN");
    //temp_buff.seq=FIN_SEQ;//fin number
    send_message(sockfd, &temp_buff,&cli_addr,sizeof(cli_addr),param_serv.loss_prob);
    return byte_readed;
}*/

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

void reply_to_syn_and_execute_command(int sockfd,struct msgbuf request){//prendi dalla coda il messaggio di syn
    //e rispondi al client con syn_ack
    socklen_t len=sizeof(request.addr);
    int seq_to_send =0,window_base_snd=0,window_base_rcv=0,W=param_serv.window, pkt_fly=0;//primo pacchetto della finestra->primo non riscontrato
    struct temp_buffer temp_buff;//pacchetto da inviare
    struct window_rcv_buf win_buf_rcv[2*W];
    struct window_snd_buf win_buf_snd[2 * W];
    struct addr temp_addr;


    memset(win_buf_rcv,0,sizeof(struct window_rcv_buf)*(2*W));//inizializza a zero
    memset(win_buf_snd,0,sizeof(struct window_snd_buf)*(2*W));//inizializza a zero

    make_timers(win_buf_snd, W);//crea 2w timer
    set_timer(&sett_timer, param_serv.timer_ms);//inizializza struct necessaria per scegliere il timer

    temp_addr.sockfd = sockfd;
    temp_addr.dest_addr = request.addr;
    addr = &temp_addr;//inizializzo puntatore globale necessario per signal_handler

    //if (sendto(sockfd,NULL,0,0,(struct sockaddr *)&(request.addr),sizeof(struct sockaddr_in))==-1) {
      //  handle_error_with_exit("error in sendto SIN_ACK");
    //}//rispondo al syn con il syn_ack non in finestra
    send_syn_ack(sockfd, &request.addr, sizeof(request.addr),0 ); //param_serv.loss_prob
    start_timeout_timer(timeout_timer_id, 3000);
    if(recvfrom(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr *)&(request.addr),&len)!=-1){//ricevi il comando del client in finestra
        stop_timer(timeout_timer_id);
        printf("pacchetto ricevuto con ack %d seq %d command %d dati %s:\n",temp_buff.ack,temp_buff.seq,temp_buff.command, temp_buff.payload);
        printf("connessione instaurata\n");
        great_alarm=0;
        window_base_rcv=(window_base_rcv+1)%(2*W);//pkt con num sequenza zero ricevuto
        if(strncmp(temp_buff.payload,"list",4)==0){
            execute_list(sockfd,seq_to_send,temp_buff,window_base_rcv,window_base_snd,pkt_fly,win_buf_rcv,win_buf_snd,request.addr);
            return;
        }
        else if(strncmp(temp_buff.payload,"put",3)==0){
            execute_put(sockfd,seq_to_send,temp_buff,window_base_rcv,window_base_snd,pkt_fly,win_buf_rcv,win_buf_snd,request.addr);
            return;
        }
        else if(strncmp(temp_buff.payload,"get",3)==0){
            execute_get(sockfd, request.addr, len, seq_to_send,window_base_snd,window_base_rcv,W, pkt_fly,temp_buff, win_buf_rcv, win_buf_snd);
            return;
        }
        else{
            printf("invalid_command\n");
            return;
        }
    }
    else if(great_alarm==1){
        great_alarm=0;
        printf("il client non è in ascolto\n");
        return ;
    }
    return;
}

void child_job(){//lavoro che deve svolgere il processo,loop infinito su get_request,satisfy request
    printf("pid %d\n",getpid());
    struct sockaddr_in serv_addr;//struttura per processo locale
    struct msgbuf request;//contiene comando e indirizzi del client
    int sockfd,value;
    char done_jobs=0;
    struct sigaction sa,sa_timeout;

    struct mtx_prefork*mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);
    sem_t *mtx=(sem_t*)attach_shm(child_mtx_id);

    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(0);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    make_timeout_timer(&timeout_timer_id);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // crea il socket
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(sockfd, (struct sockaddr *)&(serv_addr), sizeof(serv_addr)) < 0) {//bind con una porta scelta automataticam. dal SO
        handle_error_with_exit("error in bind\n");
    }


    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    sa_timeout.sa_sigaction = timer_handler;//chiama handler quando scade un alarm
    if (sigemptyset(&sa_timeout.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGRTMIN+1, &sa_timeout, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }

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
        printf("msg queue\n");
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
        reply_to_syn_and_execute_command(sockfd,request);
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
    int sockfd,fd,readed;

    socklen_t len;
    char commandBuffer[MAXCOMMANDLINE+1],*line,*command,localname[80];
    struct sockaddr_in addr,cliaddr;
    struct msgbuf msgbuf;

    struct mtx_prefork*mtx_prefork;//mutex tra processi e thread pool handler
    sem_t*mtx;//semaforo tra i processi che provano ad accedere alla coda di messaggi
    key_t msg_key,shm_key,mtx_key,mtx_fork_key;

    if(argc!=2){
        handle_error_with_exit("usage <directory>\n");
    }
    srand(time(NULL));
    dir_server=argv[1];
    check_if_dir_exist(dir_server);
    strcpy(localname,"");
    strcpy(localname,getenv("HOME"));
    strcat(localname,"/parameter.txt");
    fd=open(localname,O_RDONLY);
    if(fd==-1){
        handle_error_with_exit("error in open file  parameter\n");
    }
    line=malloc(sizeof(char)*MAXLINE);
    command=line;
    memset(line,'\0',MAXLINE);
    readed=readline(fd,line,MAXLINE);
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
    param_serv.timer_ms=parse_long_and_move(&line);
    if(param_serv.timer_ms<0){
        handle_error_with_exit("timer must be positive or 0\n");
    }
    if(close(fd)==-1){
        handle_error_with_exit("error in close file\n");
    }
    line=command;
    free(line);
    command=NULL;

   /* msg_key=create_key(".",'d');
    mtx_key=create_key(".",'b');
    mtx_fork_key=create_key(".",'c');*/

    mtx_prefork_id=get_id_shared_mem(sizeof(struct mtx_prefork));
    child_mtx_id=get_id_shared_mem(sizeof(sem_t));
    msgid=get_id_msg_queue();//crea coda di messaggi id globale

    mtx=(sem_t*)attach_shm(child_mtx_id);//mutex per accedere alla coda
    mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);//mutex tra processi e pool handler
    initialize_mtx(mtx);
    initialize_mtx_prefork(mtx_prefork);


    memset((void *)&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(SERVER_PORT);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM,0)) < 0) { // crea il socket
        handle_error_with_exit("error in socket create\n");
    }

    if (bind(sockfd,(struct sockaddr*)&addr,sizeof(addr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }

    create_pool(1);//solo per ora num_child=0
    //create_thread_pool_handler(mtx_prefork);

    while(1) {
        len=sizeof(cliaddr);
        if ((recvfrom(sockfd, commandBuffer, MAXCOMMANDLINE, 0, (struct sockaddr *) &cliaddr, &len)) < 0) {
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