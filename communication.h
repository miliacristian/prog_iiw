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

#ifndef PROG_IIW_COMMUNICATION_H
#define PROG_IIW_COMMUNICATION_H

#endif //PROG_IIW_COMMUNICATION_H


void send_syn_ack(int sockfd,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob);
void resend_message(int sockfd,struct temp_buffer*temp_buff,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob);
void send_syn(int sockfd,struct sockaddr_in *serv_addr, socklen_t len, double loss_prob);
void send_message(int sockfd, struct sockaddr_in *cli_addr, socklen_t len, struct temp_buffer temp_buff, char *data, char command, double loss_prob);
void rcv_data_send_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_ack_file_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
//void send_fin(int sockfd, struct sockaddr_in *cli_addr, socklen_t len, struct temp_buffer temp_buff, double loss_prob);
//void send_fin_ack(int sockfd,struct sockaddr_in *serv_addr,socklen_t len,struct temp_buffer temp_buff,double loss_prob);
void send_data_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) ;
void send_message_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm,char command,char*message);
void rcv_list_send_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void send_list_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_ack_list_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_msg_send_ack_command_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_msg_re_send_ack_command_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
//void rcv_list_send_ack_command_in_window(int sockfd,char*list, struct sockaddr_in *serv_addr, socklen_t len, struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv, int *window_base_rcv, double loss_prob, int W, int dim, int *byte_written);