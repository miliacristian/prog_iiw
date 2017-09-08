#include <time.h>
#include <signal.h>
#include "basic.h"

#ifndef PROG_IIW_TIMER_H
#define PROG_IIW_TIMER_H

#endif //PROG_IIW_TIMER_H

long calculate_time_left(struct node node);
void sleep_struct(struct timespec* sleep_time, long timer_ns_left);
int calculate_sample_RTT(struct timespec tx_time);
int calculate_est_RTT(int est_RTT, int sample_RTT);
int calculate_dev_RTT(int est_RTT, int sample_RTT, int dev_RTT);
int calculate_timeout(int est_RTT, int dev_RTT);