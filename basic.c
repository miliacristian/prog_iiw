#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include "timer.h"

void*try_to_sleep(void*arg){//thread che invoca il timer_handler e che quindi gestisce le ritrasmissioni
    printf("tid %d\n",(int)pthread_self());
    while(1){
        pause();
    }
}
pthread_t create_thread_signal_handler(){
    pthread_t tid;
    sigset_t set;
    if(pthread_create(&tid,NULL,try_to_sleep,NULL)!=0){

    }
    if(sigemptyset(&set)==-1){
        handle_error_with_exit("error in sigemptyset\n");
    }
    if(sigaddset(&set,SIGRTMIN+1)==-1){
        handle_error_with_exit("error in sigaddset\n");
    }
    if(sigaddset(&set,SIGRTMIN)==-1){
        handle_error_with_exit("error in sigaddset\n");
    }
    if(pthread_sigmask(SIG_BLOCK,&set,NULL)!=0){
        handle_error_with_exit("error in pthread_sigmask\n");
    }
    return tid;

}
void destroy_thread_signal_handler(pthread_t tid){
    pthread_kill(tid,SIGTERM);
    return;
}
char check_if_dir_exist(char*dir_path){
    DIR *dir;
    if((dir=opendir(dir_path))==NULL){
        handle_error_with_exit("error in open directory\n");
    }
    closedir(dir);
    return 1;
}
void handle_error_with_exit(char*errorString){
    perror(errorString);
    exit(EXIT_FAILURE);
}
void copy_buf1_in_buf2(char*buf2,char*buf1,int dim){
    for(int i=0;i<dim;i++){
        *buf2=*buf1;
        buf1++;
        buf2++;
    }
    return;
}
char seq_is_in_window(int win_base,int window,int seq){
    //verifica che se un numero di sequenza è dentro la finestra 1 si 0 no
    int end_win=(win_base+window-1)%(2*window);
    if(seq<0){
        return 0;
    }
    if(seq>((2*window)-1)){
        return 0;
    }
    if(win_base<window+1){
        if(seq>=win_base && seq<=end_win){
            return 1;
        }
        return 0;
    }
    else {//window base>=window+1
        if(seq<win_base && seq>end_win){
            return 0;
        }
        return 1;
    }

}
char flip_coin(double loss_prob){//ritorna vero se devo trasmettere falso altrimenti
    int random_num=rand()%101;
    if(loss_prob>(random_num)){
        return 0;
    }
    return 1;
}
void send_message(int sockfd,struct temp_buffer*temp_buff,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob) {
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto inviato con ack %d seq %d command %d \n", temp_buff->ack, temp_buff->seq,temp_buff->command);
    }
    else{
        printf("pacchetto con ack %d, seq %d command %d perso\n",temp_buff->ack,temp_buff->seq, temp_buff->command);
    }
    return;
}

void resend_message(int sockfd,struct temp_buffer*temp_buff,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob) {
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd, temp_buff, MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto ritrasmesso con ack %d seq %d command %d\n", temp_buff->ack, temp_buff->seq,temp_buff->command);
    }
    else{
        printf("pacchetto ritrasmesso con ack %d, seq %d perso\n",temp_buff->ack,temp_buff->seq);
    }
    return;
}

void send_syn(int sockfd,struct sockaddr_in *serv_addr, socklen_t len, double loss_prob) {
    struct temp_buffer temp_buff;
    temp_buff.seq=NOT_AN_ACK;
    temp_buff.command=SYN;
    temp_buff.ack=NOT_AN_ACK;
    strcpy(temp_buff.payload,"SYN");
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd,&temp_buff,MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in syn sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto syn mandato\n");
    }
    else{
        printf("pacchetto syn perso\n");
    }
    return;
}

void send_syn_ack(int sockfd,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob) {
    struct temp_buffer temp_buff;
    temp_buff.seq=NOT_AN_ACK;
    temp_buff.command=SYN_ACK;
    temp_buff.ack=NOT_AN_ACK;
    strcpy(temp_buff.payload,"SYN_ACK");
    if(flip_coin(loss_prob)) {
        if (sendto(sockfd,&temp_buff,MAXPKTSIZE,0, (struct sockaddr *) serv_addr, len) == -1) {//manda richiesta del client al server
            handle_error_with_exit("error in sendto\n");//pkt num sequenza zero mandato
        }
        printf("pacchetto syn ack mandato\n");
    }
    else{
        printf("pacchetto syn ack perso\n");
    }
    return;
}

char* files_in_dir(char* path,int lenght) {
    //funzione che, dato un path, crea una stringa (file1\nfile2\nfile3\n...) contenente il nome di tutti file in quella directory
    DIR *d;
    struct dirent *dir;
    char *list;
    list = malloc(sizeof(char)*lenght);
    if(list==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    if(path==NULL){
        handle_error_with_exit("error path NULL\n");
    }
    memset(list,'\0',(size_t)lenght);
    d = opendir(path);
    strcat(list,"list:");
    if (d!=NULL) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_DIR) {
                strcat(list, dir->d_name);
                strcat(list, "\n");
            }
        }
        closedir(d);
    }
    else{
        handle_error_with_exit("error in scan directory\n");
    }
    return (list);
}

char check_if_file_exist(char*path){//verifica che il file esiste in memoria
    if(path==NULL){
        handle_error_with_exit("error in check_if_file_exist\n");
    }
    if(access(path,F_OK)!=-1){
        return 1;
    }
    return 0;
}

int get_file_size(char*path){
    if(path==NULL){
        handle_error_with_exit("error in get_file_size\n");
    }
    struct stat st;
    if(stat(path,&st)==-1){
        handle_error_with_exit("error get file_status\n");
    }
    return (int)st.st_size;
}

void lock_sem(sem_t*sem){
    if(sem_wait(sem)==-1){
        handle_error_with_exit("error in sem_wait\n");
    }
    return;
}
void unlock_sem(sem_t*sem){
    if(sem_post(sem)==-1){
        handle_error_with_exit("error in sem_post\n");
    }
    return;
}
int get_id_msg_queue(){//funzione che crea la coda di messaggi
    int msgid;
    if((msgid=msgget(IPC_PRIVATE,IPC_CREAT | 0666))==-1){
        handle_error_with_exit("error in msgget\n");
    };
    return msgid;
}
int get_id_shared_mem(int size){
    int shmid;
    if((shmid=shmget(IPC_PRIVATE,(size_t)size,IPC_CREAT | 0666))==-1){
        handle_error_with_exit("error in get_id_shm\n");
    }
    return shmid;
}
void*attach_shm(int shmid){
    void*mtx=shmat(shmid,NULL,0);//VEDI TERZO PARAMETRO MAN_PAGE
    if(mtx==(void*)-1){
        handle_error_with_exit("errror in attach_shm\n");
    }
    return mtx;
}

/*key_t create_key(char*k,char k1){//funzione che crea la chiave per la msgqueue
    key_t key=ftok(k,k1);
    if(key==-1){
        handle_error_with_exit("error in ftok\n");
    }
    return key;
}*/

int count_char_dir(char*path){
    DIR *d;
    struct dirent *dir;
    int lenght=1;
    if(path==NULL){
        handle_error_with_exit("path is NULL\n");
    }
    d = opendir(path);
    if (d==NULL){
        handle_error_with_exit("error in scan directory\n");
    }
    while ((dir = readdir(d)) != NULL)
    {
        if (dir->d_type != DT_DIR) {
            lenght += strlen(dir->d_name);
            lenght++;// newline o terminatore stringa
        }
    }
    closedir(d);//chiudendo la directory una volta riaperta ripunta al primo file della directory
    return lenght;

}

char* generate_full_pathname(char* filename, char* dir_server){//ricordarsi di fare la free di path nella funzione chiamante
    char* path;
    path=malloc(sizeof(char)*MAXPKTSIZE);
    if(path==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    memset(path,'\0',MAXPKTSIZE);
    strcpy(path,dir_server);
    strcat(path,filename);
    return path;
}