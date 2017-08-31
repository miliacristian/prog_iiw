#include <time.h>
#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "timer.h"
#include "Client.h"
#include "Server.h"
#include "list_client.h"
#include "list_server.h"
#include "get_client.h"
#include "get_server.h"
#include "communication.h"
#include "list.h"

int  calculate_time_left(struct Node node){
    //ritorna il numero di millisecondi(tv-getttimeofday)
    struct timeval time_current;
    long time_ms_cur;
    long time_ms_timeval;
    int time_ms_left;
    if(gettimeofday(&time_current,NULL)!=0){
        handle_error_with_exit("error in gettimeofday\n");
    }
    initialize_timeval(&(node.tv), node.timer_ms);
    time_ms_cur=(time_current.tv_usec/1000)+(time_current.tv_sec*1000);
    time_ms_timeval=(node.tv.tv_usec/1000)+(node.tv.tv_sec*1000);
    time_ms_left=(int)time_ms_timeval-time_ms_cur;
    return time_ms_left;
}