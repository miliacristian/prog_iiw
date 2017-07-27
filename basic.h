#include <semaphore.h>

#define MAXCOMMANDLINE 320
#define MAXFILENAME 255
#define MAXPKTSIZE 1468//no packet fragmentation
#define MAXLINE 1024
#define SERVER_PORT 5193


struct mtx_list{//definizione di struct condivise ad accesso esclusivo
    sem_t sem;
    int lenght;
    char modified;
};
struct mtx_prefork{
    sem_t sem;
    int free_process;
};
struct msgbuf{
    long mtype;
    struct sockaddr_in addr;
    char command[MAXCOMMANDLINE];//ancora da definire
};

struct select_param{
    int window;
    double loss_prob;
    long timer_ms;
}param_serv,param_client;

char*dir_server;
char*dir_client;

void handle_error_with_exit(char*errorString);
void get_file_list(char*path);
char check_if_file_exist(char*path);
int get_file_size(char*path);
char count_words_into_line(char*line);
void lock_sem(sem_t*sem);
void unlock_sem(sem_t*sem);
int get_id_msg_queue(key_t key);
int get_id_shared_mem(key_t shm_key,int size);
void*attach_shm(int shmid);
key_t create_key(char*k,char k1);
int count_char_dir(char*path);
char* files_in_dir(char* path,int lenght);
char seq_is_in_window(int start_win,int end_win,int window,int seq);
char check_if_dir_exist(char*dir_path);

