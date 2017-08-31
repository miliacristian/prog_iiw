#include <signal.h>
#include <time.h>

#ifndef PROG_IIW_SERVER_H
#define PROG_IIW_SERVER_H

#endif //PROG_IIW_SERVER_H
//variabili globali
extern int msgid,child_mtx_id,mtx_prefork_id,great_alarm_serv;//dopo le fork tutti i figli sanno quali sono gli id
extern struct select_param param_serv;
extern char*dir_server;
