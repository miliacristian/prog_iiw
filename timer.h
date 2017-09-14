#include <time.h>
#include <signal.h>
#include "basic.h"

long calculate_time_left(struct node node);
void sleep_struct(struct timespec* sleep_time, long timer_ns_left);
int calculate_sample_RTT(struct timespec tx_time);
double calculate_est_RTT(double est_RTT, double sample_RTT);
void adaptive_timer(struct shm_sel_repeat* shm, int seq);