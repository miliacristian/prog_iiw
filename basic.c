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

void*try_to_sleep(void*arg){//thread che invoca il timer_handler e che quindi gestisce le ritrasmissioni
    (void)arg;
    block_signal(SIGRTMIN+1);
    while(1){
        pause();
    }
}

char to_resend(struct shm_sel_repeat *shm, struct Node node){
    if( shm->win_buf_snd[node.seq].time.tv_sec == node.tv.tv_sec && shm->win_buf_snd[node.seq].time.tv_usec == node.tv.tv_usec ){
        if(shm->win_buf_snd[node.seq].acked){
            return 0;
        }
        return 1;
    }
    return 0;
}

pthread_t create_thread_signal_handler(){
    pthread_t tid;
    if(pthread_create(&tid,NULL,try_to_sleep,NULL)!=0){
        handle_error_with_exit("error in pthread_create\n");
    }
    block_signal(SIGRTMIN);
    return tid;

}
void block_signal(int signal){
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
void initialize_sem(sem_t*mtx){
    if(sem_init(mtx,1,1)==-1){
        handle_error_with_exit("error in sem_init\n");
    }
    return;
}
void initialize_mtx(pthread_mutex_t *mtx){
    if(pthread_mutex_init(mtx,NULL)!=0){
        handle_error_with_exit("error in initialize mtx\n");
    }
    return;
}
void destroy_mtx(pthread_mutex_t *mtx){
    if(pthread_mutex_destroy(mtx)!=0){
        handle_error_with_exit("error in destroy mtx\n");
    }
    return;
}
void lock_mtx(pthread_mutex_t *mtx){
    if(pthread_mutex_lock(mtx)!=0){
        handle_error_with_exit("error in pthread_mutex_lock\n");
    }
    return;
}
void unlock_mtx(pthread_mutex_t *mtx){
    if(pthread_mutex_unlock(mtx)!=0){
        handle_error_with_exit("error in pthread_mutex_unlock\n");
    }
    return;
}
void initialize_cond(pthread_cond_t*cond){
    if(pthread_cond_init(cond,NULL)!=0){
        handle_error_with_exit("error in initialize cond\n");
    }
}
void destroy_cond(pthread_cond_t*cond){
    if(pthread_cond_destroy(cond)!=0){
        handle_error_with_exit("error in destroy cond\n");
    }
    return;
}
void wait_on_a_condition(pthread_cond_t*cond,pthread_mutex_t *mtx){
    if(pthread_cond_wait(cond,mtx)!=0){
        handle_error_with_exit("error in wait on a condition\n");
    }
    return;
}
void unlock_thread_on_a_condition(pthread_cond_t*cond){
    if(pthread_cond_signal(cond)!=0){
        handle_error_with_exit("error in unlock_thread_on_a_condition\n");
    }
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

int count_word_in_buf(char*buf){
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

void generate_branches_and_number(char*temp,char copy_number){//fa diventare temp una stringa con parentesi e numero dentro
    char num_format_string[4];
    memset(num_format_string,'\0',4);
    strcpy(temp,"(");
    sprintf(num_format_string, "%d",copy_number);
    strcat(temp,num_format_string);
    strcat(temp,")");
    return;
}

char* generate_multi_copy(char*path_to_filename,char*filename){//ritorna path assoluti tutti diversi tra loro partendo da un file che esiste,o ritorna null se ci sono troppe copie del file(255)
// fare la free di absolut path nella funzione chiamante dopo aver creato il file
    unsigned char copy_number=1;
    char temp[6],*occurence,*absolute_path,*first_of_the_dot;//temp=="(number)"
    memset(temp,'\0',6);
    absolute_path=generate_full_pathname(filename,path_to_filename);
    if(!check_if_file_exist(absolute_path)){
        return absolute_path;
    }
    first_of_the_dot=malloc(sizeof(char)*(strlen(filename)+6));;//6==terminatore+ "(" + ")" +3 cifre per il numero
    if(first_of_the_dot==NULL){
        handle_error_with_exit("error in malloc\n");
    }
    memset(first_of_the_dot,'\0',strlen(filename)+6);
    generate_branches_and_number(temp,copy_number);//scrive dentro temp la stringa da concatenare
    occurence=strchr(filename,'.');
    if(occurence==NULL){//non esiste un punto nel filename
        strcpy(first_of_the_dot,filename);
        strcat(first_of_the_dot,temp);
    }
    else{//esiste un punto nel filename
        strncpy(first_of_the_dot,filename,strlen(filename)-strlen(occurence));
        strcat(first_of_the_dot,temp);
        strcat(first_of_the_dot,occurence);
    }
    absolute_path=generate_full_pathname(first_of_the_dot,path_to_filename);
    while(check_if_file_exist(absolute_path)){
        copy_number+=1;
        if(copy_number>=255){
            return NULL;
        }
        if(occurence==NULL) {//aggiungi alla fine del filename le parentesi e il numero
            generate_branches_and_number(temp, copy_number);
            strcpy(first_of_the_dot,filename);
            strcat(first_of_the_dot,temp);
            absolute_path=generate_full_pathname(first_of_the_dot,path_to_filename);
        }
        else{//esiste un punto nel filename
            generate_branches_and_number(temp, copy_number);
            memset(first_of_the_dot,'\0',strlen(filename)+6);
            strncpy(first_of_the_dot,filename,strlen(filename)-strlen(occurence));
            strcat(first_of_the_dot,temp);
            strcat(first_of_the_dot,occurence);
            absolute_path=generate_full_pathname(first_of_the_dot,path_to_filename);
        }
    }
    free(first_of_the_dot);
    return absolute_path;
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

int count_char_dir(char*path){
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
