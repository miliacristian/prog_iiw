/*NAME
        flock - apply or remove an advisory lock on an open file
int flock(int fd, int operation);*/
/*DESCRIPTION
        Apply or remove an advisory lock on the open file specified by fd.  The
        argument operation is one of the following:

LOCK_SH  Place a shared lock.  More than one  process  may  hold  a
        shared lock for a given file at a given time.
LOCK_EX  Place  an  exclusive  lock.   Only one process may hold an
exclusive lock for a given file at a given time.
LOCK_UN  Remove an existing lock held by this process.
A call to flock() may block if an incompatible lock is held by  another
        process.   To  make  a  nonblocking request, include LOCK_NB (by ORing)
with any of the above operations.*/
//EWOULDBLOCK
        //The file is locked and the LOCK_NB flag was selected.
//A  process  may  hold  only one type of lock (shared or exclusive) on a
//file.  Subsequent flock() calls on an already locked file will  convert
//an existing lock to the new lock mode.
//Locks  created  by flock() are associated with an open file description
//(see open(2)).  This means that duplicate file descriptors (created by,
//for  example,  fork(2) or dup(2)) refer to the same lock, and this lock
  //      may be modified or released using any of these  descriptors.
#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "timer.h"
#include "Client.h"
#include "Server.h"
#include "list_client.h"
#include "list_server.h"
#include "get_client.h"
#include "get_server.h"
#include "communication.h"

int file_lock_read(int fd){
    int file_lock;
    if((file_lock=flock(fd,LOCK_SH))==-1){
        handle_error_with_exit("error in flock_read\n");
    }
    return file_lock;
}
int file_lock_unlock(int fd){
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
