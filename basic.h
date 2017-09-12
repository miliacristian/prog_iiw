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

#define MAXCOMMANDLINE 320//lunghezza massima comando che  inserisce il client
#define MAXFILENAME 255//lunghezza massima filename
#define MAXPKTSIZE 1468//1468==no packet fragmentation
#define MAXLINE 1024
#define BUFF_RCV_SIZE (208*1024)//buffer ricezione socket 208*1024 max buff_size_without root
#define OVERHEAD (sizeof(int)*3+sizeof(char))//payload-byte informativi
#define PATH_LEN 256//massima lunghezza del path

#define SERVER_PORT 5195//porta del server
#define IP "127.0.0.1"

#define NOT_AN_ACK (-5)//pacchetto non è un ack
#define NOT_A_PKT (-5) //pacchetto non è un pacchetto ma è un ack

#define DATA 0//comando da inserire nel pacchetto
#define GET 1//comando da inserire nel pacchetto
#define FIN 2//comando da inserire nel pacchetto
#define FIN_ACK 3//comando da inserire nel pacchetto
#define START 4 //comando da inserire nel pacchetto
#define ERROR 5//comando da inserire nel pacchetto
#define DIMENSION 6 //comando da inserire nel pacchetto
#define PUT 7//comando da inserire nel pacchetto
#define LIST 8//comando da inserire nel pacchetto
#define SYN 9//comando da inserire nel pacchetto
#define SYN_ACK 10//comando da inserire nel pacchetto

#define TIMEOUT 2//timeout,se non si riceve nulla per timeout secondi l'altro host non è in ascolto
#define TIMER_BASE_ADAPTIVE 10 //timer di partenza caso adattativo (in millisecondi)


#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

#define STR_VALUE(val) #val
#define STR(name) STR_VALUE(name)

#define MD5_LEN 32//lunghezza md5


#define NUM_FREE_PROCESS 10//numero di processi server che devono essere sempre disponibili per una richiesta
#define MAX_PROC_JOB 5//numero di richieste che un processo server può eseguire prima di morire
#define NO_LAP (-1)
#ifndef LINE_H
#define LINE_H

//pacchetto da mandare non selective repeat ack=not_an_ack seq=not_a_pkt

struct temp_buffer{//struttura del pacchetto da inviare
    int seq;//numero sequenza
    int ack;//numero ack
    int lap;//giri di finestra
    char command;//comando del pacchetto
    char payload[MAXPKTSIZE-OVERHEAD];// dati pacchetto
};
struct window_rcv_buf{//elemento della finestra di ricezione
    char received;//ricevuto 1==si 0==no
    char command;//comando
    int lap;
    char*payload;//dati pacchetto
};

struct window_snd_buf{//elemento della finestra di trasmissione
    char acked;//riscontrato 1==si 0==da riscontrare 2==vuoto
    char command;//comando
    struct timespec time;//usato per timer adattativo
    int lap;
    char*payload;//dati pacchetto
};


struct addr{//struttura contentente le informazioni per mandare un pacchetto ad un altro host
    int sockfd;
    struct sockaddr_in dest_addr;
    socklen_t len;
};

struct select_param{//parametri di esecuzione
    int window;//grandezza finestra
    double loss_prob;//prob perdita
    int timer_ms;//tempo di ritrasmissione in millisecondi ,se t==0 timer adattativo
};

struct shm_sel_repeat{//struttura condivisa tra i 2 thread necessaria sia per la sincronizzazione
// sia per svolgere la richiesta(put/get/list) vera e propria

    struct addr addr;//indirizzo dell'host
    int pkt_fly;//numero pacchetti in volo (va da 0 a w-1)
    pthread_mutex_t mtx;//mutex
    sem_t*mtx_file;
    struct window_snd_buf *win_buf_snd;//finestra trasmissione (va da 0 a 2w-1)
    struct window_rcv_buf *win_buf_rcv;//finestra ricezione (va da 0 a 2w-1)
    char md5_sent[MD5_LEN + 1];//md5
    long dimension;//dimensione del file/lista
    char*filename;
    int window_base_rcv;//sequenza di inizio finestra ricezione (va da 0 a 2w-1)
    int window_base_snd;//sequenza di inizio finestra trasmissione (va da 0 a 2w-1)
    int seq_to_send;//prossimo numero di sequenza da dare al pacchetto (va da 0 a 2w-1)
    pthread_cond_t list_not_empty;//variabile condizione lista dinamica non vuota
    long byte_readed;//byte letti e riscontrati lato sender
    long byte_written;//byte scritti e riscontrati lato receiver
    long byte_sent;//byte inviati con pacchetti senza contare quelli di ritrasmissione
    char*list;//puntatore alla lista dei file
    struct select_param param;//parametri
    double est_RTT_ms;//estimated RTT necessario per timer adattativo
    double dev_RTT_ms;
    char adaptive;//se 1 timer adattativo se 0 timer non adattativo
    int fd;//file descriptor
    pthread_t tid;//tid del thread di ritrasmissione,necessario per ucciderlo al termine della richiesta
    struct node* head;//puntatore alla testa della lista dinamica
    struct node *tail;//puntatore alla coda della lista dinamica
};

struct mtx_prefork{//mutex usato dai processi server e dal thread pool handler per tenere traccia dei processi liberi
    sem_t sem;
    int free_process;
};

struct msgbuf{//struttura della coda condivisa dei processi per leggere le richieste dei client
    long mtype;
    struct sockaddr_in addr;
};

struct node  {//struttura di un nodo della lista dinamica ordianta,la lista tiene traccia delle trasmissioni e delle ritrasmissioni
    int seq;
    struct timespec tv;//tempo di invio
    int timer_ms;//timeout attuale
    int lap;
    struct node* next;//puntatore al prossimo
    struct node* prev;//puntatore al precedente
};
#endif

void handle_error_with_exit(char*errorString);
void get_file_list(char*path);
char check_if_file_exist(char*path);
long get_file_size(char*path);
char count_words_into_line(char*line);
void lock_sem(sem_t*sem);
void unlock_sem(sem_t*sem);
int get_id_msg_queue();
int get_id_shared_mem(int size);
void*attach_shm(int shmid);
int count_char_dir(char*path);
char* files_in_dir(char* path,long lenght);
char seq_is_in_window(int start_win,int window,int seq);
char check_if_dir_exist(char*dir_path);
char flip_coin(double loss_prob);
char* generate_full_pathname(char* filename, char* dir_server);
void copy_buf2_in_buf1(char*buf1,char*buf2,long dim);
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
//char to_resend(struct shm_sel_repeat *shm, struct node node);
char to_resend(struct shm_sel_repeat *shm, struct node node);
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
void initialize_timeval(struct timespec *tv,int timer_ms);
double absolute(double value);
char* add_slash_to_dir(char*argument);
int get_id_shared_mem_with_key(int size,key_t key);
key_t get_key(char c);