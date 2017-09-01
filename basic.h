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
#define MAXPKTSIZE 1468//no packet fragmentation //1468
#define MAXLINE 1024
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
#define TIMEOUT 5
#ifndef LINE_H
#define LINE_H
//pacchetto fuori finestra da mandare ack=not_an_ack seq=not_a_pkt
struct temp_buffer{
    int seq;
    int ack;
    char command;
    char payload[MAXPKTSIZE-9];// dati pacchetto
};
struct window_rcv_buf{
    char received;
    char command;
    char payload[MAXPKTSIZE-9];
};

struct window_snd_buf{//struttura per memorizzare info sui pacchetti da inviare
// se diventa pesante come memoria è meglio allocata nell'heap?
    char acked;
    char payload[MAXPKTSIZE-9];
    char command;
    struct timespec time;//usato per timer adattativo
    //int seq_numb;
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
struct shm_snd {
    struct shm_sel_repeat *shm;
    pthread_t tid;
};

struct node  {
    int seq;
    struct timespec tv;
    int timer_ms;
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
void copy_buf1_in_buf2(char*buf2,char*buf1,int dim);
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
