#include "basic.h"


void send_syn_ack(int sockfd,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob);
void resend_message(int sockfd,struct temp_buffer*temp_buff,struct sockaddr_in *serv_addr,socklen_t len, double loss_prob);
void send_syn(int sockfd,struct sockaddr_in *serv_addr, socklen_t len, double loss_prob);
void send_message(int sockfd, struct sockaddr_in *cli_addr, socklen_t len, struct temp_buffer temp_buff, char *data, char command, double loss_prob);
void rcv_data_send_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_ack_file_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void send_data_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm) ;
void send_message_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm,char command,char*message);
void rcv_list_send_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void send_list_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_ack_list_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_msg_send_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void rcv_msg_re_send_ack_in_window(struct temp_buffer temp_buff,struct shm_sel_repeat *shm);
void print_rcv_message(struct temp_buffer temp_buff);
void *rtx_job(void *arg);