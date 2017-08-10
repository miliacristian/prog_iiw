
#include <time.h>
#ifndef PROG_IIW_CLIENT_H
#define PROG_IIW_CLIENT_H

#endif //PROG_IIW_CLIENT_H
extern struct addr *addr;
extern struct itimerspec sett_timer_cli;//timer e reset timer globali
extern int great_alarm;//se diventa 1 è scattato il timer grande
extern timer_t timeout_timer_id; //id  del timer di timeout;
extern struct select_param param_client;
extern char *dir_client;
