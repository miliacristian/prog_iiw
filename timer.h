#include <time.h>
#include <signal.h>
#include "basic.h"

#ifndef PROG_IIW_TIMER_H
#define PROG_IIW_TIMER_H

#endif //PROG_IIW_TIMER_H

void start_timer(timer_t timer_id, struct itimerspec *its);
void stop_timer(timer_t timer_id);
void make_timeout_timer(timer_t* timer_id);
void start_timeout_timer(timer_t timer_id, long msec);
void make_timers(struct window_snd_buf *win_buf, int W);
void set_timer(struct itimerspec *its, long msec);
void stop_all_timers(struct window_snd_buf* win_buf_snd, int W);