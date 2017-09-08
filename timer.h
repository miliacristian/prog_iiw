#include <time.h>
#include <signal.h>
#include "basic.h"

#ifndef PROG_IIW_TIMER_H
#define PROG_IIW_TIMER_H

#endif //PROG_IIW_TIMER_H

long calculate_time_left(struct node node);
void sleep_struct(struct timespec* sleep_time, long timer_ns_left);
int calculate_sample_RTT(struct timespec tx_time);
double calculate_est_RTT(double est_RTT, double sample_RTT);
double calculate_dev_RTT(double est_RTT, double sample_RTT, double dev_RTT);
int calculate_timeout(double est_RTT, double dev_RTT);
void adaptive_timer(struct shm_sel_repeat* shm, int seq);