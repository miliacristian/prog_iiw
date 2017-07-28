#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
char connect_status=1;
void signal_handler(int signum, siginfo_t *si,void *uc){
	(void)signum;
	(void)si;
	(void)uc;
    connect_status=0;
    return;
}


int file_receiver(int sockfd,int fd,int byte_expected,struct sockaddr_in dest_addr){//ritorna il numero di byte
    //buff[i]==buffer che contiene il pacchetto iesimo
    int byte_written=0,window_base=0,W=param_serv.window;//primo pacchetto della finestra->primo non riscontrato
    double timer=param_serv.timer_ms,loss_prob=param_serv.loss_prob;
    struct temp_buffer temp_buff;
    struct window_rcv_buf win_buf[2*W];
    memset(win_buf,0,sizeof(struct window_rcv_buf)*(2*W));//inizializza a zero
    socklen_t len=sizeof(dest_addr);
    
    struct sigaction sa;
    sa.sa_sigaction =signal_handler;
    if(sigemptyset(&sa.sa_mask)==-1){
	handle_error_with_exit("error in sigempty set\n");
    }
    if (sigaction(SIGALRM, &sa, NULL) == -1){
        printf("sigaction error\n");
        return -1;
    }
	(void)timer;
	(void)loss_prob;
    while(byte_written<byte_expected){
        alarm(5);//timeout cautelativo per capire se effettivamente il sender ha ricevuto command_ack
        if(recvfrom(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr*)&dest_addr,&len)!=-1){
            alarm(0);//resetto il timer perchè ho ricevuto un pacchetto
            if(temp_buff.seq>(2*W-1)){//num sequenza imprevisto
                //ignora
            }
            else if(!seq_is_in_window(window_base,window_base+W-1,W,temp_buff.seq)){
                //se il numero  non è dentro la finestra
                // un ack è stato smarrito->rinvialo
                if(sendto(sockfd,&(temp_buff.seq), sizeof(int),0,(struct sockaddr*)&dest_addr,sizeof(dest_addr))==-1) {//rinvio ack
                    handle_error_with_exit("error in sendto\n");
                }
		    }
            else{//sequenza in window
                win_buf[temp_buff.seq].received=1;//segno pacchetto n-esimo come ricevuto
                strcpy(win_buf[temp_buff.seq].payload,temp_buff.payload);//memorizzo il pacchetto n-esimo
                if(temp_buff.seq==window_base) {//se pacchetto riempie un buco
                    // scorro la finestra fino al primo ancora non ricevuto
                    while (win_buf[window_base].received ==1) {
                        //dentro il buffer non deve esserci il terminatore
                        writen(fd,win_buf[window_base].payload,strlen(win_buf[window_base].payload));//necessario cosi non copia il terminatore
			//controllo su writen
                        byte_written+=strlen(win_buf[window_base].payload);
                        win_buf[window_base].received=0;//segna pacchetto come non ricevuto
                        window_base=(window_base+1)%(2*W);//avanza la finestra con modulo di 2W
                    }
                    if(byte_written==byte_expected){
                        while(connect_status==1){//chiusura di connessione
                            alarm(5);
                            if(recvfrom(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr*)&dest_addr,&len)!=-1){
                                alarm(0);//resetto il timer perchè ho ricevuto un pacchetto
                                if(temp_buff.seq>(2*W-1)){//num sequenza imprevisto
                                    //ignora
                                }
                                else if(temp_buff.seq==-2){//fine trasferimento
                                    return byte_written;
                                }
                                else if(!seq_is_in_window(window_base,window_base+W-1,W,temp_buff.seq)){
                                    //se il numero  non è dentro la finestra
                                    // un ack è stato smarrito->rinvialo
                                    if(sendto(sockfd,&(temp_buff.seq), sizeof(int),0,(struct sockaddr*)&dest_addr,sizeof(dest_addr))==-1) {//rinvio ack
                                        handle_error_with_exit("error in sendto\n");
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else{
            printf("il sender non manda più nulla o errore interno\n");
            return byte_written;
        }
    }
    return byte_written;
}
