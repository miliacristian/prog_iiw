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
#include "basic.h"

int close_connection_list(struct temp_buffer temp_buff,int *seq_to_send,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob,struct shm_sel_repeat *shm);
int  wait_for_fin_list(struct temp_buffer temp_buff,struct window_snd_buf*win_buf_snd,int sockfd,struct sockaddr_in serv_addr,socklen_t len,int *window_base_snd,int *window_base_rcv,int *pkt_fly,int W,int *byte_written,double loss_prob,struct shm_sel_repeat *shm);
int rcv_list(int sockfd,struct sockaddr_in serv_addr,socklen_t len,struct temp_buffer temp_buff,struct window_snd_buf *win_buf_snd,struct window_rcv_buf *win_buf_rcv,int *seq_to_send,int W,int *pkt_fly,char*list,int dimension,double loss_prob,int *window_base_snd,int *window_base_rcv,int *byte_written,struct shm_sel_repeat *shm);
void list_client(struct shm_sel_repeat *shm);
int wait_for_list_dimension(int sockfd, struct sockaddr_in serv_addr, socklen_t  len, int *byte_written , int *seq_to_send , int *window_base_snd , int *window_base_rcv, int W, int *pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd,struct shm_sel_repeat *shm);