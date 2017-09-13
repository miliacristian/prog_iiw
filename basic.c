#include "basic.h"
#include "timer.h"

char calc_file_MD5(char *filename, char *md5_sum, long dimension){//calcola md5 di un file
    if(filename==NULL || md5_sum==NULL){
        handle_error_with_exit("error in calc md5\n");
    }
    if(dimension>=MAX_MD5_SIZE){
        printf(YELLOW "Md5 not available due to file size\n"RESET);
        memset(md5_sum, '0', 32);
        return 1;
    };
#define MD5SUM_CMD_FMT "md5sum %." STR(PATH_LEN) "s 2>/dev/null"
    char cmd[PATH_LEN + sizeof (MD5SUM_CMD_FMT)];
    sprintf(cmd, MD5SUM_CMD_FMT, filename);
#undef MD5SUM_CMD_FMT
    FILE *p = popen(cmd, "r");
    if (p == NULL) return 0;
    int i, ch;
    for (i = 0; i < MD5_LEN && isxdigit(ch = fgetc(p)); i++) {
        *md5_sum++ = ch;
    }
    *md5_sum = '\0';
    pclose(p);
    return i == MD5_LEN;
}
void check_md5(char*filename,char*md5_to_check, long dimension) {//verifica che 2 md5 sono uguali
    if (dimension >=MAX_MD5_SIZE){
        printf(YELLOW "File %s received,md5 not available due to file size\n"RESET, filename);
    }else {
        char md5[MD5_LEN + 1];
        if (filename == NULL || md5_to_check == NULL) {
            handle_error_with_exit("error in check md5\n");
        }
        if (!calc_file_MD5(filename, md5, dimension)) {
            handle_error_with_exit("error in calculate md5\n");
        }
        if (strcmp(md5_to_check, md5) != 0) {
            printf(RED "File %s corrupted\n"RESET, filename);
        } else {
            printf(GREEN "File %s correctly received\n"RESET, filename);
        }
    }
    return;
}
char* add_slash_to_dir(char*argument){
    char*dir;
    if(argument==NULL){
        handle_error_with_exit("error in add_slash argument is NULL\n");
    }
    if((argument[strlen(argument)-1])!='/'){
        dir=malloc(sizeof(char)*(strlen(argument)+2));//1 per "/" uno per terminatore
        if(dir==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(dir,'\0',strlen(argument)+2);
        better_strcpy(dir,argument);
        better_strcat(dir,"/");
        dir[strlen(argument)+1]='\0';
    }
    else {
        dir=malloc(strlen(argument)+1);//1 per "/" uno per terminatore
        if(dir==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        memset(dir,'\0',strlen(argument)+1);
        better_strcpy(dir,argument);
        dir[strlen(argument)]='\0';
    }
    return dir;
}
double absolute(double value){
    if (value<0){
        return ((value)*(-1));
    }
    return value;
}

void initialize_timeval(struct timespec *tv,int timer_ms){//funzione che somma i tempi di una struct e di un timer
// mettendo il risultato dentro la struct
    if(tv==NULL){
        handle_error_with_exit("error in initialize timeval\n");
    }
    long temp;
    temp=tv->tv_nsec+(timer_ms*1000000);
    if(temp>=1000000000){
        tv->tv_nsec=temp%1000000000;
        tv->tv_sec+=(temp-tv->tv_nsec)/1000000000;
    }else{
        tv->tv_nsec = temp;
    }
    return;
}
void better_strcpy(char*buf1,char*buf2){//strcpy+controllo buffer
    if(buf1==NULL){
        handle_error_with_exit("error better strcpy buf1 is NULL\n");
    }
    if(buf2==NULL){
        handle_error_with_exit("error better strcpy buf2 is NULL\n");
    }
    strcpy(buf1,buf2);
    return;
}
void better_strncpy(char*buf1,char*buf2,int lenght){//strncpy+controllo buffer
    if(buf1==NULL){
        handle_error_with_exit("error better strcpy buf1 is NULL\n");
    }
    if(buf2==NULL){
        handle_error_with_exit("error better strcpy buf2 is NULL\n");
    }
    strncpy(buf1,buf2,lenght);
    return;
}
void better_strcat(char*str1,char*str2){//strcat+controllo buffer
    if(str1==NULL){
        handle_error_with_exit("error better strcat str1 is NULL\n");
    }
    if(str2==NULL){
        handle_error_with_exit("error better strcat str2 is NULL\n");
    }
    strcat(str1,str2);
    return;
}
void set_max_buff_rcv_size(int sockfd){//imposta il buffer ricezione della socket al massimo possibile(senza root)
    int buff_size=BUFF_RCV_SIZE;
    if(setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&buff_size,sizeof(buff_size))!=0){
        handle_error_with_exit("error in setsockopt\n");
    }
    return;
}

char to_resend(struct shm_sel_repeat *shm, struct node node){//verifica se un pacchetto è da ritrasmettere
    if(shm==NULL){
        handle_error_with_exit("error in to_resend shm is NULL\n");
    }
    if(node.lap == (shm->win_buf_snd[node.seq]).lap){
        if((shm->win_buf_snd[node.seq].acked)!= 0){
            return 0;
        }
        return 1;
    }
    else{
        return 0;
    }
}
key_t get_key(char c){
    key_t key;
    key=ftok(".",c);
    if(key==-1){
        handle_error_with_exit("error in get_key\n");
    }
    return key;
}
void block_signal(int signal){//non fa ricevere il segnale definito al thread chiamante
    sigset_t set;
    if(sigemptyset(&set)==-1){
        handle_error_with_exit("error in sigemptyset\n");
    }
    if(sigaddset(&set,signal)==-1){//aggiungi segnale al sigset
        handle_error_with_exit("error in sigaddset\n");
    }
    if(pthread_sigmask(SIG_BLOCK,&set,NULL)!=0){//blocca i segnali presenti nel sig_set
        handle_error_with_exit("error in pthread_sigmask\n");
    }
    return;
}
void unlock_signal(int signal){//fa ricevere il segnale definito al thread chiamante
    sigset_t set;
    if(sigemptyset(&set)==-1){
        handle_error_with_exit("error in sigemptyset\n");
    }
    if(sigaddset(&set,signal)==-1){//aggiungi segnale al sigset
        handle_error_with_exit("error in sigaddset\n");
    }
    if(pthread_sigmask(SIG_UNBLOCK,&set,NULL)!=0){//blocca i segnali presenti nel sig_set
        handle_error_with_exit("error in pthread_sigmask\n");
    }
    return;
}
void initialize_sem(sem_t*mtx){//inizializza semaforo
    if(mtx==NULL){
        handle_error_with_exit("error in initialize_sem mtx is NULL\n");
    }
    if(sem_init(mtx,1,1)==-1){
        handle_error_with_exit("error in sem_init\n");
    }
    return;
}
void initialize_mtx(pthread_mutex_t *mtx){//inizializza mutex
    if(mtx==NULL){
        handle_error_with_exit("error in initialize_mtx mtx is NULL\n");
    }
    if(pthread_mutex_init(mtx,NULL)!=0){
        handle_error_with_exit("error in initialize mtx\n");
    }
    return;
}
void destroy_mtx(pthread_mutex_t *mtx){//distrugge mutex
    if(mtx==NULL){
        handle_error_with_exit("error in destroy_mtx mtx is NULL\n");
    }
    if(pthread_mutex_destroy(mtx)!=0){
        handle_error_with_exit("error in destroy mtx\n");
    }
    return;
}
void lock_mtx(pthread_mutex_t *mtx){//lock mutex
    if(mtx==NULL){
        handle_error_with_exit("error in lock_mtx mtx is NULL\n");
    }
    if(pthread_mutex_lock(mtx)!=0){
        handle_error_with_exit("error in pthread_mutex_lock\n");
    }
    return;
}
void unlock_mtx(pthread_mutex_t *mtx){//unlock mutex
    if(mtx==NULL){
        handle_error_with_exit("error in unlock_mtx mtx is NULL\n");
    }
    if(pthread_mutex_unlock(mtx)!=0){
        handle_error_with_exit("error in pthread_mutex_unlock\n");
    }
    return;
}
void initialize_cond(pthread_cond_t*cond){//inizializza variabile condizione
    if(cond==NULL){
        handle_error_with_exit("error in initialize_cond cond is NULL\n");
    }
    if(pthread_cond_init(cond,NULL)!=0){
        handle_error_with_exit("error in initialize cond\n");
    }
}
void destroy_cond(pthread_cond_t*cond){//distrugge variabile condizione
    if(cond==NULL){
        handle_error_with_exit("error in destroy_cond cond is NULL\n");
    }
    if(pthread_cond_destroy(cond)!=0){
        handle_error_with_exit("error in destroy cond\n");
    }
    return;
}
void wait_on_a_condition(pthread_cond_t*cond,pthread_mutex_t *mtx){//il thread chiamante aspetta che la condizione diventi vera
    if(mtx==NULL){
        handle_error_with_exit("error in wait condition mtx is NULL\n");
    }
    if(cond==NULL){
        handle_error_with_exit("error in wait condition cond is NULL\n");
    }
    if(pthread_cond_wait(cond,mtx)!=0){
        handle_error_with_exit("error in wait on a condition\n");
    }
    return;
}
void unlock_thread_on_a_condition(pthread_cond_t*cond){//sblocca un thread bloccato sulla condizione specificata
    if(cond==NULL){
        handle_error_with_exit("error in unlock_thread cond is NULL\n");
    }
    if(pthread_cond_signal(cond)!=0){
        handle_error_with_exit("error in unlock_thread_on_a_condition\n");
    }
    return;
}
char check_if_dir_exist(char*dir_path){//verifica se una directory esiste
    if(dir_path==NULL){
        handle_error_with_exit("error in check if dir exist path is NULL\n");
    }
    DIR *dir;
    if((dir=opendir(dir_path))==NULL){
        handle_error_with_exit("error in open directory\n");
    }
    closedir(dir);
    return 1;
}
void handle_error_with_exit(char*error_string){//uccide il processo dopo essersi accorto di un errore
    printf(RED "%s" RESET, error_string);
    perror("");
    exit(EXIT_FAILURE);
}
void copy_buf2_in_buf1(char*buf1,char*buf2,long dim){//copia dim byte da buf 2 a buf1
    if(buf2==NULL){
        handle_error_with_exit("buff 2 null\n");
    }
    if(buf1==NULL){
        handle_error_with_exit("buff 1 null\n");
    }
    if(dim<0){
        handle_error_with_exit("error in copy_buf2 in buf1\n");
    }
    for(int i=0;i<dim;i++){
        *buf1=*buf2;
        buf1++;
        buf2++;
    }
    return;
}

int count_word_in_buf(char*buf){//conta le parole dentro una stringa
    if(buf==NULL){
        handle_error_with_exit("error in count_word buf is NULL\n");
    }
    int word=0;
    size_t lun;
    lun=strlen(buf);
    for(int i=0;i<(int)lun;){
        if(*buf==' '){
            buf++;
            i++;
            while(*buf==' '){
                buf++;
                i++;
            }
        }
        if(*buf!='\0' && *buf!='\n'){
            word++;
        }
        else{
            break;
        }
        while(*buf!=' ' && *buf!='\n'){
            buf++;
            i++;
        }
    }
    return word;
}

void generate_branches_and_number(char*temp,char copy_number){//fa diventare temp una stringa con underscore e numero dentro
    char num_format_string[4];//3 per le cifre +1 terminatore
    if(temp==NULL){
        handle_error_with_exit("error in generate branches temp is NULL\n");
    }
    memset(num_format_string,'\0',4);
    better_strcpy(temp,"_");
    sprintf(num_format_string, "%d",copy_number);
    better_strcat(temp,num_format_string);
    return;
}

char* generate_multi_copy(char*path_to_filename,char*filename){//ritorna path assoluti tutti diversi tra loro partendo da un file che esiste,o ritorna null se ci sono troppe copie del file(255)
// fare la free di absolut path nella funzione chiamante dopo aver creato il file
    unsigned char copy_number=1;
    char temp[5],*occurence,*absolute_path,*first_of_the_dot;//temp=="_number"
    memset(temp,'\0',5);
    if(path_to_filename==NULL){
        handle_error_with_exit("error generate multi copy path_to_filename is NULL\n");
    }
    if(filename==NULL){
        handle_error_with_exit("error generate multi copy filename is NULL\n");
    }
    absolute_path=generate_full_pathname(filename,path_to_filename);
    if(absolute_path==NULL){
        handle_error_with_exit("error in generate full path\n");
    }
    if(!check_if_file_exist(absolute_path)){
        return absolute_path;
    }
    first_of_the_dot=malloc(sizeof(char)*(strlen(filename)+5));;//5==terminatore+ "_" + +3 cifre per il numero
    if(first_of_the_dot==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    memset(first_of_the_dot,'\0',strlen(filename)+5);
    generate_branches_and_number(temp,copy_number);//scrive dentro temp la stringa da concatenare
    occurence=strchr(filename,'.');
    if(occurence==NULL){//non esiste un punto nel filename
        better_strcpy(first_of_the_dot,filename);
        better_strcat(first_of_the_dot,temp);
    }
    else{//esiste un punto nel filename
        better_strncpy(first_of_the_dot,filename,strlen(filename)-strlen(occurence));
        better_strcat(first_of_the_dot,temp);
        better_strcat(first_of_the_dot,occurence);
    }
    absolute_path=generate_full_pathname(first_of_the_dot,path_to_filename);
    if(absolute_path==NULL){
        handle_error_with_exit("error in generate full path\n");
    }
    while(check_if_file_exist(absolute_path)){
        copy_number+=1;
        if(copy_number>=255){
            return NULL;
        }
        if(occurence==NULL) {//aggiungi alla fine del filename le parentesi e il numero
            generate_branches_and_number(temp, copy_number);
            better_strcpy(first_of_the_dot,filename);
            better_strcat(first_of_the_dot,temp);
            absolute_path=generate_full_pathname(first_of_the_dot,path_to_filename);
            if(absolute_path==NULL){
                handle_error_with_exit("error in generate full path\n");
            }
        }
        else{//esiste un punto nel filename
            generate_branches_and_number(temp, copy_number);
            memset(first_of_the_dot,'\0',strlen(filename)+5);
            better_strncpy(first_of_the_dot,filename,strlen(filename)-strlen(occurence));
            better_strcat(first_of_the_dot,temp);
            better_strcat(first_of_the_dot,occurence);
            absolute_path=generate_full_pathname(first_of_the_dot,path_to_filename);
            if(absolute_path==NULL){
                handle_error_with_exit("error in generate full path\n");
            }
        }
    }
    free(first_of_the_dot);
    return absolute_path;
}

char seq_is_in_window(int win_base,int window,int seq){
    //verifica che se un numero di sequenza è dentro la finestra:1 si; 0 no
    if(win_base<0 || window<1 || seq<0){
        handle_error_with_exit("invalid win_base,window or seq in seq_is_in_window\n");
    }
    int end_win=(win_base+window-1)%(2*window);
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

char flip_coin(double loss_prob){//genera un numero casuale tra 0 e 100 e ritorna vero se devo trasmettere,falso altrimenti
    if(loss_prob<0 || loss_prob>100){
        handle_error_with_exit("error in flip_coin invalid loss_prob\n");
    }
    int random_num=rand()%101;
    if(loss_prob>(random_num)){
        return 0;
    }
    return 1;
}

int count_char_dir(char*path){
    if(path==NULL){
        handle_error_with_exit("error in count_char_dir path is NULL\n");
    }
    DIR *d;
    struct dirent *dir;
    int lenght=0;
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
            lenght++;// newline
        }
    }
    if(lenght!=0){
        lenght+=1;//per il terminatore di stringa
    }
    closedir(d);//chiudendo la directory una volta riaperta ripunta al primo file della directory
    return lenght;

}
char* files_in_dir(char* path,long lenght) {
    //funzione che, dato un path, crea una stringa (file1\nfile2\nfile3\n...) contenente il nome di tutti file in quella directory
    //fare la free nel chiamante del char* ritornato
    if(path==NULL || lenght<0){
        handle_error_with_exit("error in files_in_dir\n");
    }
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
    if (d!=NULL) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_DIR) {
                better_strcat(list, dir->d_name);
                better_strcat(list, "\n");
            }
        }
        closedir(d);
    }
    else{
        handle_error_with_exit("error in scan directory\n");
    }
    return (list);
}
char*make_list(char*path){//ritorna la lista di file presenti in path
    char*list;
    int lenght;
    if(path==NULL){
        handle_error_with_exit("error in make list path is NULL\n");
    }
    lenght=count_char_dir(path);
    list=files_in_dir(path,lenght);
    return list;
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

long get_file_size(char*path){//ritorna la dimensione di un file dato un path
    if(path==NULL){
        handle_error_with_exit("error in get_file_size\n");
    }
    struct stat st;
    if(stat(path,&st)==-1){
        handle_error_with_exit("error get file_status\n");
    }
    return st.st_size;
}

void lock_sem(sem_t*sem){//lock semaforo
    if(sem==NULL){
        handle_error_with_exit("error in lock_sem sem is NULL\n");
    }
    if(sem_wait(sem)==-1){
        handle_error_with_exit("error in sem_wait\n");
    }
    return;
}
void unlock_sem(sem_t*sem){//unlock semaforo
    if(sem==NULL){
        handle_error_with_exit("error in unlock_sem sem is NULL\n");
    }
    if(sem_post(sem)==-1){
        handle_error_with_exit("error in sem_post\n");
    }
    return;
}
int get_id_msg_queue(){//funzione che crea la coda di messaggi
    int msgid;
    if((msgid=msgget(IPC_PRIVATE,IPC_CREAT | 0666))==-1){
        handle_error_with_exit("error in msgget\n");
    }
    return msgid;
}

int get_id_shared_mem_with_key(int size,key_t key){//funzione che crea la memoria condivisa tra processi
    int shmid;
    if((shmid=shmget(key,(size_t)size,IPC_CREAT | 0666))==-1){
        handle_error_with_exit("error in get_id_shm\n");
    }
    return shmid;
}
int get_id_shared_mem(int size){//funzione che crea la memoria condivisa tra processi
    int shmid;
    if((shmid=shmget(IPC_PRIVATE,(size_t)size,IPC_CREAT | 0666))==-1){
        handle_error_with_exit("error in get_id_shm\n");
    }
    return shmid;
}
void*attach_shm(int shmid){//funzione che ritorna puntatore all'area di memoria condivisa
    void*mtx=shmat(shmid,NULL,0);
    if(mtx==(void*)-1){
        handle_error_with_exit("errror in attach_shm\n");
    }
    return mtx;
}

char* generate_full_pathname(char* filename, char* dir){//funzione che crea il path assoluto di un file sapendo che sta nella directory dir
//ricordarsi di fare la free di path nella funzione chiamante
    char* path;
    path=malloc(sizeof(char)*MAXPKTSIZE);
    if(filename ==NULL || dir==NULL){
        return NULL;
    }
    if(path==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    memset(path,'\0',MAXPKTSIZE);
    better_strcpy(path,dir);
    better_strcat(path,filename);
    return path;
}
