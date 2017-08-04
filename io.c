
#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

ssize_t writen(int fd, void *buf, size_t n){
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
    ptr = buf;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if ((nwritten < 0) && (errno == EINTR)){//se è stato interrotto da un segnale continua
            nwritten = 0;
            }
            else{
                return(-1);/* errore */
            }
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return(n-nleft);
}

ssize_t readn(int fd,void *buf, size_t n){
    size_t nleft;
    ssize_t nread;
    char *ptr;
    ptr = buf;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) <= 0) {
            if ((nread < 0) && (errno == EINTR)){//se è stato interrotto da un segnale continua
                nread = 0;
            }
            else{
                return(-1);/* errore */
            }
        }
        nleft -= nread;
        ptr += nread;
    }
    return(n-nleft);
}

int readline(int fd, void *vptr, int maxlen)
{
    int  n, rc;
    char c, *ptr;

    ptr = vptr;
    for (n = 1; n < maxlen; n++) {
        if ((rc = (int)read(fd, &c, 1)) == 1) {
            *ptr++ = c;
            if (c == '\n') break;
        }
        else
        if (rc == 0) {		/* read ha letto l'EOF */
            if (n == 1) return(0);	/* esce senza aver letto nulla */
            else break;
        }
        else return(-1);		/* errore */
    }

    *ptr = 0;	/* per indicare la fine dell'input */
    return(n);	/* restituisce il numero di byte letti */
}
