#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

//variabili globali
int msgid,mtx_list_id,child_mtx_id,mtx_prefork_id;//dopo le fork tutti i figli sanno quali sono gli id
struct select_param param_serv;
char*dir_server;

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
void initialize_mtx_list(struct mtx_list*mtx,char*path){
    if(sem_init(&(mtx->sem),1,1)==-1){
        handle_error_with_exit("error in sem_init\n");
    }
    mtx->lenght=count_char_dir(path)+5;//conta i char  in dir comprese newlines e terminatore stringa+strlen("list:")
    printf("%d caratteri\n",mtx->lenght);
    mtx->modified=1;
    return;
}

void send_list_to_client(char*list,int sockfd,struct sockaddr_in cliaddr,int lenght){
    if(sendto(sockfd,list,lenght,0,(struct sockaddr*)&cliaddr,sizeof(cliaddr))<0){
        handle_error_with_exit("error in send_list\n");
    }
    return;
}

void satisfy_request(int sockfd,struct msgbuf request,struct mtx_list*mtx_list){
    int filename_len=0,fd,byte;//lunghezza filename upload
    char*filename,*path;
    filename_len=strlen((request.command)-4);
    filename=alloca(sizeof(char)*filename_len);
    //strcpy(filename,request.command[3]);//sottostringa
    if(strncmp(request.command,"put",3)==0){//esegui comando put
        //leggi la dimensione del file passata dal client nel buffer filename e memorizzala in byte
        //metti in filename la sottostringa che contiene solo il nome del file
        //path=alloca(sizeof(char)*(strlen(parameter_server.directory)+strlen(filename)));// lo "/" è già presente

        strcat(filename,path);//?? farla che path ha la concatenazione di stringhe
        if(!check_if_file_exist(path)){//il file non esiste posso memorizzarlo,farlo con try_lock
            fd=open(filename,O_CREAT | O_WRONLY,0644);
            if(fd==-1){
                handle_error_with_exit("error in open file\n");
            }
            selective_repeat_receiver(sockfd,fd,byte,request.addr);
        }
        else{//il file esiste già,manda conferma per rinominarlo,prova a riaprirlo e a riverificare che esiste
        }

        lock_sem(&(mtx_list->sem));
        mtx_list->lenght+=filename_len;
        mtx_list->modified=1;
        unlock_sem(&(mtx_list->sem));
    }
    else{//comando get
        filename=alloca(sizeof(char)*filename_len);
        //creare path=direcotry+filename
        if(!check_if_file_exist(path)){
            //comunica al client che il file non esiste
        }
    }
    return;
}
void child_job(){//lavoro che deve svolgere il processo,loop infinito su get_request,satisfy request
    printf("pid %d\n",getpid());
    struct sockaddr_in serv_addr;//struttura per processo locale
    struct msgbuf request;//contiene comando e indirizzi del client
    int sockfd,value;
    char done_jobs=0;

    struct mtx_list*mtx_list=(struct mtx_list*)attach_shm(mtx_list_id);
    struct mtx_prefork*mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);
    sem_t *mtx=(sem_t*)attach_shm(child_mtx_id);


    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(0);
    serv_addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // crea il socket
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(sockfd, (struct sockaddr *)&(serv_addr), sizeof(serv_addr)) < 0) {//bind con una porta scelta automataticam. dal SO
        handle_error_with_exit("error in bind\n");
    }

    for(;;){
        lock_sem(&(mtx_prefork->sem));
        if(mtx_prefork->free_process>=5){
            printf("suicidio del pid %d\n",getpid());
            unlock_sem(&(mtx_prefork->sem));
            exit(EXIT_SUCCESS);
        }
        mtx_prefork->free_process+=1;
        unlock_sem(&(mtx_prefork->sem));
        lock_sem(mtx);
        value=msgrcv(msgid,&request,sizeof(struct msgbuf)-sizeof(long),2,0);
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
        satisfy_request(sockfd,request,mtx_list);
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
void*thread_list_job(void*arg){//thread che legge i messaggi di tipo 1 dalla coda di messaggi
    //e fa la list il thread non muore mai
    struct mtx_list *mtx=arg;//puntatore alla regione di memoria condivisa
    struct msgbuf msg;
    struct sockaddr_in servaddr;
    int sockfd,len_list;
    int dim_msg=sizeof(struct msgbuf)-sizeof(long);
    char*list=NULL;

    memset((void *)&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(0);
    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // crea il socket
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(sockfd,(struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }

    for(;;) {
        printf("provo a leggere dalla coda\n");
        if (msgrcv(msgid, &msg, dim_msg, 1, 0) == -1) {//type=1
            handle_error_with_exit("error in msgrcv\n");
        }
        printf("messaggio preso dalla coda\n");
        lock_sem(&(mtx->sem));
        if(mtx->modified==0){
            printf("lista non modificata\n");
            len_list=mtx->lenght;
            unlock_sem(&(mtx->sem));
            send_list_to_client(list,sockfd,msg.addr,len_list);
        }
        else{
            printf("lista modificata\n");
            mtx->modified=0;//ponila come non modificata
            len_list=mtx->lenght;
            unlock_sem(&(mtx->sem));
            if(list!=NULL) {
                free(list);//serve solo la prima volta
            }
            list=files_in_dir(dir_server,len_list);
            send_list_to_client(list,sockfd,msg.addr,len_list);//con selective repeat
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
    char commandBuffer[MAXCOMMANDLINE+1],*line;
    struct sockaddr_in addr,cliaddr;
    struct msgbuf msgbuf;

    struct mtx_prefork*mtx_prefork;//mutex tra processi e thread pool handler
    sem_t*mtx;//semaforo tra i processi che provano ad accedere alla coda di messaggi
    key_t msg_key,shm_key,mtx_key,mtx_fork_key;

    if(argc!=2){
       handle_error_with_exit("usage <directory>\n");
    }
    dir_server=argv[1];
    check_if_dir_exist(dir_server);
    fd=open("/home/cristian/Scrivania/parameter.txt",O_RDONLY);
    if(fd==-1){
        handle_error_with_exit("error in read parameters into file\n");
    }
    line=malloc(sizeof(char)*MAXLINE);
    memset(line,'\0',MAXLINE);
    readed=readline(fd,line,MAXLINE);
    if(readed<=0){
        handle_error_with_exit("error in read line\n");
    }
    param_serv.window=parse_integer_and_move(&line);
    skip_space(&line);
    param_serv.loss_prob=parse_double_and_move(&line);
    skip_space(&line);
    param_serv.timer_ms=parse_long_and_move(&line);
    if(close(fd)==-1){
	handle_error_with_exit("error in close file\n");
    }
    free(line);

    msg_key=create_key(".",'d');
    shm_key=create_key(".",'a');
    mtx_key=create_key(".",'b');
    mtx_fork_key=create_key(".",'c');

    mtx_prefork_id=get_id_shared_mem(mtx_fork_key,sizeof(struct mtx_prefork));
    child_mtx_id=get_id_shared_mem(mtx_key,sizeof(sem_t));
    msgid=get_id_msg_queue(msg_key);//crea coda di messaggi id global

    mtx=(sem_t*)attach_shm(child_mtx_id);
    mtx_prefork=(struct mtx_prefork*)attach_shm(mtx_prefork_id);
    initialize_mtx(mtx);
    initialize_mtx_prefork(mtx_prefork);


    memset((void *)&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(SERVER_PORT);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // crea il socket
        handle_error_with_exit("error in socket create\n");
    }
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }

    create_pool(0);//solo per ora num_child=0
    create_thread_pool_handler(mtx_prefork);
    //create_thread_list(mtx_list);

    while(1) {
        len=sizeof(cliaddr);
        if ((recvfrom(sockfd, commandBuffer, MAXCOMMANDLINE, 0, (struct sockaddr *) &cliaddr, &len)) < 0) {
            handle_error_with_exit("error in recvcommand");//scrive nella struct le info del client
            // e nel buffer il comando ricevuto dal client
        }
        printf("messaggio ricevuto server\n");
        msgbuf.addr=cliaddr;//inizializza la struct con addr e commandBuffer
        strcpy(msgbuf.command,commandBuffer);
        msgbuf.mtype=0;
        if(msgsnd(msgid,&msgbuf,sizeof(struct msgbuf)-sizeof(long),0)==-1){
            handle_error_with_exit("error in msgsnd\n");
        }
    }
    return EXIT_SUCCESS;
}
