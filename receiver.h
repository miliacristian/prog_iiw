#include <netinet/in.h>
#include "basic.h"

struct temp_buffer{
    int seq;
    char payload[MAXPKTSIZE-4];// dati pacchetto
};
struct window_rcv_buf{
    int received;
    char payload[MAXPKTSIZE-4];
};

int selective_repeat_receiver(int sockfd, int fd, int byte_expected, struct sockaddr_in dest_addr);
