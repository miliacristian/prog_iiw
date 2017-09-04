#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "timer.h"

#ifndef PROG_IIW_GET_CLIENT_H
#define PROG_IIW_GET_CLIENT_H

#endif //PROG_IIW_GET_CLIENT_H

int wait_for_get_dimension(int sockfd, struct sockaddr_in serv_addr, socklen_t  len, char *filename, int *byte_written , int *seq_to_send , int *window_base_snd , int *window_base_rcv, int W, int *pkt_fly , struct temp_buffer temp_buff ,struct window_rcv_buf *win_buf_rcv,struct window_snd_buf *win_buf_snd);
void get_client(struct shm_sel_repeat *shm);