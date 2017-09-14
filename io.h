#include <unistd.h>

ssize_t writen(int fd, void *buf, size_t n);
ssize_t readn(int fd, void *buf, size_t n);
int readline(int fd, void *vptr, int maxlen);
