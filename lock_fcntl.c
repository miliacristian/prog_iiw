#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zconf.h>
#include <errno.h>

//usage my_lock_wait();
//sezione critica
//my_lock_release();
static struct flock	lock_it, unlock_it;
static int		lock_fd = -1;
/* fcntl() will fail if my_lock_init() not called */


//Because of the buffering performed by the stdio(3) library, the use  of
        //record  locking  with  routines  in that package should be avoided; use
//read(2) and write(2) instead.

void my_lock_init(char *pathname)
{
    char	lock_file[1024];

    /* must copy caller's string, in case it's a constant */
    strncpy(lock_file, pathname, sizeof(lock_file));
    if ( (lock_fd = mkstemp(lock_file)) < 0) {
        fprintf(stderr, "errore in mkstemp");
        exit(1);
    }

    if (unlink(lock_file) == -1) { 	/* but lock_fd remains open */
        fprintf(stderr, "errore in unlink per %s", lock_file);
        exit(1);
    }
    lock_it.l_type = F_WRLCK;//F_RDLCK,F_UNLCK
    lock_it.l_whence = SEEK_SET;//SEEK_CUR, SEEK_END
    lock_it.l_start = 0;/* Number of bytes to lock */
    lock_it.l_len = 0;

    unlock_it.l_type = F_UNLCK;
    unlock_it.l_whence = SEEK_SET;
    unlock_it.l_start = 0;
    unlock_it.l_len = 0;
    return;
}
//In  order  to place a read lock, fd must be open for reading.  In order
//to place a write lock, fd must be open  for  writing.   To  place  both
        //types of lock, open a file read-write.

void my_lock_wait()
{
    int rc;

    while ( (rc = fcntl(lock_fd, F_SETLKW, &lock_it)) < 0) {//F_SETLKW if a conflicting lock is held on  the  file,
        //then  wait  for that lock to be released.
        if (errno == EINTR)
            continue;
        else {
            fprintf(stderr, "errore fcntl in my_lock_wait");
            exit(1);
        }
    }
    return;
}

void my_lock_release(){
    if (fcntl(lock_fd, F_SETLKW, &unlock_it) < 0) {
        fprintf(stderr, "errore fcntl in my_lock_release");
        exit(1);
    }
    return;
}

