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
#include "receiver.h"

#define MAXCOMMANDLINE 320
#define MAXFILENAME 255
#define MAXPKTSIZE 1468//no packet fragmentation //1468
#define MAXLINE 1024
#define SERVER_PORT 5194
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
#define TIMEOUT 5000
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
    int received;
    char command;
    char payload[MAXPKTSIZE-9];
};

struct window_snd_buf{//struttura per memorizzare info sui pacchetti da inviare
// se diventa pesante come memoria è meglio allocata nell'heap?
    int acked;
    char payload[MAXPKTSIZE-9];
    timer_t time_id;
    char command;
    int time_start;//usato per timer adattativo
    int seq_numb;
};


struct addr{
    int sockfd;
    struct sockaddr_in dest_addr;
};

struct mtx_prefork{
    sem_t sem;
    int free_process;
};
struct msgbuf{
    long mtype;
    struct sockaddr_in addr;
};

struct select_param{
    int window;
    double loss_prob;
    int timer_ms;
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
void send_message(int sockfd,struct temp_buffer*temp_buff,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob);
void send_syn(int sockfd,struct sockaddr_in *serv_addr, socklen_t len, double loss_prob);
void send_syn_ack(int sockfd,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob);
char* generate_full_pathname(char* filename, char* dir_server);
void resend_message(int sockfd,struct temp_buffer*temp_buff,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob);
void copy_buf1_in_buf2(char*buf2,char*buf1,int dim);
void*try_to_sleep(void*arg);
pthread_t create_thread_signal_handler();
char* generate_multi_copy(char*path_to_filename,char*filename);
int count_word_in_buf(char*buf);
void block_signal(int signal);