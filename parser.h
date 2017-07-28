
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


double parse_double(char*string);
long parse_long(char*string);
int parse_integer(char*string);
struct parameter check_and_parse_parameter_value(char**argv);
void check_and_parse_command(char*command,char*filename);
void move_pointer(char**string,int n);
int skip_space(char**string);
char is_blank(char*string);
int parse_integer_and_move(char**string);
long parse_long_and_move(char**string);
double parse_double_and_move(char**string);

