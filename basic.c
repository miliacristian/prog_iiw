#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

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
char seq_is_in_window(int win_base,int end_win,int window,int seq){
    //verifica che se un numero di sequenza è dentro la finestra 1 si 0 no
    if(seq<0){
        return 0;
    }
    if(win_base<=window){
        if(seq>=win_base || seq<=end_win){
            return 1;
        }
        return 0;
    }
    else{//window base>window
        if(seq<win_base && seq>end_win){
            return 0;
        }
        return 1;
    }
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
    memset(list,'\0',lenght);
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
    return st.st_size;
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
int get_id_msg_queue(key_t key){//funzione che crea la coda di messaggi
    int msgid;
    if((msgid=msgget(key,IPC_CREAT | 0666))==-1){
        handle_error_with_exit("error in msgget\n");
    };
    return msgid;
}
int get_id_shared_mem(key_t shm_key,int size){
    int shmid;
    if((shmid=shmget(shm_key,size,IPC_CREAT | 0666))==-1){
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

key_t create_key(char*k,char k1){//funzione che crea la chiave per la msgqueue
    key_t key=ftok(k,k1);
    if(key==-1){
        handle_error_with_exit("error in ftok\n");
    }
    return key;
}

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
