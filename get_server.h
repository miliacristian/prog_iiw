#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
#include "timer.h"
#include "Server.h"

#ifndef PROG_IIW_GET_SERVER_H
#define PROG_IIW_GET_SERVER_H

#endif //PROG_IIW_GET_SERVER_H

int execute_get(int sockfd, struct sockaddr_in cli_addr, socklen_t len, int seq_to_send, int window_base_snd,int window_base_rcv, int W,int pkt_fly,struct temp_buffer temp_buff, struct window_rcv_buf *win_buf_rcv, struct window_snd_buf *win_buf_snd);