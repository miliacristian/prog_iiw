#include "basic.h"
#include "timer.h"
#include "file_lock.h"

int file_lock_read(int fd){
    int file_lock;
    if((file_lock=flock(fd,LOCK_SH))==-1){
        handle_error_with_exit("error in flock_read\n");
    }
    return file_lock;
}
int file_unlock(int fd){
    int file_lock;
    if((file_lock=flock(fd,LOCK_UN))==-1){
        handle_error_with_exit("error in flock_read\n");
    }
    return file_lock;
}
int file_lock_write(int fd){
    int file_lock;
    if((file_lock=flock(fd,LOCK_EX))==-1){
        handle_error_with_exit("error in flock_read\n");
    }
    return file_lock;
}
int file_try_lock_read(int fd){
    return flock(fd,LOCK_SH|LOCK_NB);
    //caller must see this return value and check errno if is EWOULDBLOCK
}
int file_try_lock_write(int fd){
    return flock(fd,LOCK_EX|LOCK_NB);
    //caller must see this return value and check errno if is EWOULDBLOCK
}
