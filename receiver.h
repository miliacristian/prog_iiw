
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
#include <wait.h>
#include <zconf.h>
#define MAXPKTSIZE 1468
#ifndef BUFF_H
#define BUFF_H
struct temp_buffer{
    int seq;
    int ack;
    char payload[MAXPKTSIZE-8];// dati pacchetto
};
struct window_rcv_buf{
    int received;
    char payload[MAXPKTSIZE-8];
};
#endif

int selective_repeat_receiver(int sockfd, int fd, int byte_expected, struct sockaddr_in dest_addr);
