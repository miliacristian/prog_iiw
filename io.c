
#include "basic.h"
#include "io.h"

ssize_t writen(int fd, void *buf, size_t n){//scrive n byte su un file
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;
    if(buf==NULL){
        handle_error_with_exit("error in writen buf is NULL\n");
    }
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
//
ssize_t readn(int fd,void *buf, size_t n){//legge n byte da un file
    size_t nleft;
    ssize_t nread;
    char *ptr;
    ptr = buf;
    nleft = n;
    if(buf==NULL){
        handle_error_with_exit("error in readn buf is NULL\n");
    }
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

int readline(int fd, void *vptr, int maxlen)//legge dal file fino a newline o fino a maxlen byte
{
    int  n, rc;
    char c, *ptr;
    ptr = vptr;
    if (maxlen<=0){
	handle_error_with_exit("error in readline\n");
    }
    if(vptr==NULL){
        handle_error_with_exit("error2 in readline\n");
    }
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
