#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>
#include <netinet/in.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <unistd.h>
#include <zconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wait.h>
#include <zconf.h>
#include <wchar.h>
#include <signal.h>
#include <sys/time.h>

#define MAXCOMMANDLINE 320
#define MAXFILENAME 255
#define MAXPKTSIZE 120//1468==no packet fragmentation //1468
#define MAXLINE 1024
#define BUFF_RCV_SIZE (208*1024)//(208*1024)//208*1024 max buff_size_without root
#define SERVER_PORT 5195
#define ERROR 5
#define START 4
#define DIMENSION 6
#define NOT_A_PKT (-5)
#define NOT_AN_ACK (-5)
#define FIN 2
#define FIN_ACK 3
#define GET 1
#define PUT 7
#define LIST 8
#define DATA 0
#define SYN 9
#define SYN_ACK 10
#define TIMEOUT 2
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"
#define STR_VALUE(val) #val
#define STR(name) STR_VALUE(name)
#define PATH_LEN 256
#define MD5_LEN 32
#define OVERHEAD (sizeof(int)*3+sizeof(char))
#define FREE_PROCESS 1
#define MAX_PROC_JOB 4
#define NO_LAP (-1)
#ifndef LINE_H
#define LINE_H
//pacchetto fuori finestra da mandare ack=not_an_ack seq=not_a_pkt
struct temp_buffer{
    int seq;
    int ack;
    int lap;
    char command;
    char payload[MAXPKTSIZE-OVERHEAD];// dati pacchetto
};
struct window_rcv_buf{
    char received;
    char command;
    int lap;
    char*payload;
};

struct window_snd_buf{//struttura per memorizzare info sui pacchetti da inviare
// se diventa pesante come memoria è meglio allocata nell'heap?
    char acked;
    char command;
    struct timespec time;//usato per timer adattativo
    int lap;
    char*payload;
};


struct addr{
    int sockfd;
    struct sockaddr_in dest_addr;
    socklen_t len;
};

struct select_param{
    int window;
    double loss_prob;
    int timer_ms;
};
struct shm_sel_repeat{//variabili inutilizzate da togliere
    struct timeval time;//
    struct addr addr;
    int pkt_fly;//
    pthread_mutex_t mtx;//
    struct window_snd_buf *win_buf_snd;//
    struct window_rcv_buf *win_buf_rcv;//
    char md5_sent[MD5_LEN + 1];//dopo l'inizializzazione contiene terminatore di stringa
    int dimension;
    char*filename;
    int window_base_rcv;//
    int window_base_snd;//
    int seq_to_send;//prossima posizione libera per mandare pacchetto
    pthread_cond_t list_not_empty;
    int byte_readed;
    int byte_written;
    int byte_sent;
    char*list;
    struct select_param param;
    int fd;
    pthread_t tid;
    struct node* head;
    struct node *tail;
};

struct mtx_prefork{
    sem_t sem;
    int free_process;
};
struct msgbuf{
    long mtype;
    struct sockaddr_in addr;
};

struct node  {
    int seq;
    struct timespec tv;
    int timer_ms;
    int lap;
    struct node* next;
    struct node* prev;
};
#endif

void handle_error_with_exit(char*errorString);
void get_file_list(char*path);
char check_if_file_exist(char*path);
int get_file_size(char*path);
char count_words_into_line(char*line);
void lock_sem(sem_t*sem);
void unlock_sem(sem_t*sem);
int get_id_msg_queue();
int get_id_shared_mem(int size);
void*attach_shm(int shmid);
key_t create_key(char*k,char k1);
int count_char_dir(char*path);
char* files_in_dir(char* path,int lenght);
char seq_is_in_window(int start_win,int window,int seq);
char check_if_dir_exist(char*dir_path);
char flip_coin(double loss_prob);
char* generate_full_pathname(char* filename, char* dir_server);
void copy_buf2_in_buf1(char*buf1,char*buf2,int dim);
void*try_to_sleep(void*arg);
pthread_t create_thread_signal_handler();
char* generate_multi_copy(char*path_to_filename,char*filename);
int count_word_in_buf(char*buf);
void block_signal(int signal);
void initialize_sem(sem_t*mtx);
void initialize_mtx(pthread_mutex_t *mtx);
void destroy_mtx(pthread_mutex_t *mtx);
void lock_mtx(pthread_mutex_t *mtx);
void unlock_mtx(pthread_mutex_t *mtx);
void initialize_cond(pthread_cond_t*cond);
void destroy_cond(pthread_cond_t*cond);
void wait_on_a_condition(pthread_cond_t*cond,pthread_mutex_t *mtx);
void unlock_thread_on_a_condition(pthread_cond_t*cond);
char to_resend(struct shm_sel_repeat *shm, struct node node);
char to_resend2(struct shm_sel_repeat *shm, struct node node);
char calc_file_MD5(char *file_name, char *md5_sum);
void check_md5(char*filename,char*md5_to_check);
void print_double_buff_rcv_size(int sockfd);
void set_buff_rcv_size(int sockfd,int size);
void set_max_buff_rcv_size(int sockfd);
char*make_list(char*path);
void better_strcpy(char*buf1,char*buf2);
void better_strcat(char*str1,char*str2);
void better_strncpy(char*buf1,char*buf2,int lenght);
void unlock_signal(int signal);