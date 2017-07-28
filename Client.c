#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"
struct addr *addr = NULL;
struct itimerspec sett_timer, rst_timer;//timer e reset timer globali
int great_alarm=0;//se diventa 1 è scattato il timer grande

void timer_handler(int sig, siginfo_t *si, void *uc) {
    (void) sig;
    (void) si;
    (void) uc;
    struct window_snd_buf *win_buffer = si->si_value.sival_ptr;
    //int seq_numb=si->si_value.sival_int;
    if (sendto(addr->sockfd, ((*win_buffer).payload), MAXPKTSIZE, 0, (struct sockaddr *) &(addr->dest_addr),
               sizeof(addr->dest_addr)) == -1) {//ritrasmetto il pacchetto di cui è scaduto il timer
        handle_error_with_exit("error in sendto\n");
    }
    if (timer_settime((*win_buffer).time_id, 0, &sett_timer, NULL) == -1) {//avvio timer
        handle_error_with_exit("error in timer_settime\n");
    }
}
void handler(){
    great_alarm=1;
    return;
}
int get_command(int sockfd,struct sockaddr_in serv_addr,char*filename){//svolgi la get con connessione già instaurata
    int byte_written=0,fd,byte_expected,seq_to_send =0,window_base_snd=0,ack_numb=0,window_base_rcv=0,W=param_serv.window;//primo pacchetto della finestra->primo non riscontrato
    int pkt_fly=0;
    char value;
    double timer=param_serv.timer_ms,loss_prob=param_serv.loss_prob;
    struct temp_buffer temp_buff;//pacchetto da inviare
    struct window_rcv_buf win_buf_rcv[2*W];
    struct window_snd_buf win_buf_snd[2 * W];
    struct addr temp_addr;
    struct sigaction sa;
    memset(win_buf_rcv,0,sizeof(struct window_rcv_buf)*(2*W));//inizializza a zero
    memset(win_buf_snd,0,sizeof(struct window_snd_buf)*(2*W));//inizializza a zero
    socklen_t len=sizeof(serv_addr);

    make_timers(win_buf_snd, W);//crea 2w timer
    set_timer(&sett_timer, 1, 10);//inizializza struct necessaria per scegliere il timer
    reset_timer(&rst_timer);//inizializza struct necessaria per resettare il timer

    temp_addr.sockfd = sockfd;
    temp_addr.dest_addr = serv_addr;
    addr = &temp_addr;//inizializzo puntatore globale necessario per signal_handler

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;//chiama timer_handler quando ricevi il segnale SIGRTMIN
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGRTMIN, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }

    strcpy(temp_buff.payload,filename);
    temp_buff.seq=0;
    temp_buff.ack=-5;
    if(sendto(sockfd,&temp_buff,sizeof(struct temp_buffer),0,&serv_addr,sizeof(struct sockaddr_in))==-1){//richiesta del client
        handle_error_with_exit("error in sendto\n");
    }
    pkt_fly++;
    seq_to_send=(seq_to_send+1)%(2*W);
    if(timer_settime(win_buf_snd[seq_to_send].time_id, 0, &sett_timer, NULL)==-1){
        handle_error_with_exit("error in timer_settime\n");
    }

    while(1) {//non è scattato il timer grande,riscriverlo meglio
        alarm(3);
        if(recvfrom(sockfd, &temp_buff, sizeof(struct temp_buffer), 0, &serv_addr, &len)!=-1) {//risposta del server
            break;
        }
        else if(great_alarm==1){//dopo tot ritrasmissioni ancora il client non riceve nulla
            great_alarm=0;
            return byte_written;
        }
    }
    alarm(0);
    if(timer_settime(win_buf_snd[seq_to_send].time_id, 0, &rst_timer, NULL)==-1){
        handle_error_with_exit("error in timer_settime\n");
    }
    pkt_fly--;
    window_base_snd=(window_base_snd+1)%(2*W);
    //verifico che esito risposta positivo vedendo temp_buff dove avevo scritto la risposta del server
    //inizio put file
    fd=open(filename,O_WRONLY || O_CREAT,0666);
    if(fd==-1){
        handle_error_with_exit("error in open\n");
    }
    byte_expected=get_file_size(filename);
    while(byte_written<byte_expected){
        alarm(5);//timeout cautelativo per capire se effettivamente il sender ha ricevuto command_ack
        if(recvfrom(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr*)&serv_addr,&len)!=-1){//bloccante
            alarm(0);//resetto il timer perchè ho ricevuto un pacchetto
            if(temp_buff.seq>(2*W-1)){//num sequenza imprevisto
                //ignora
            }
            else if(!seq_is_in_window(window_base_snd,window_base_snd+W-1,W,temp_buff.seq)){
                //se il numero  non è dentro la finestra
                // un ack è stato smarrito->rinvialo
                if(sendto(sockfd,&(temp_buff.seq), sizeof(int),0,(struct sockaddr*)&serv_addr,sizeof(serv_addr))==-1) {//rinvio ack
                    handle_error_with_exit("error in sendto\n");
                }
            }
            else{//ricevuto numero sequenza in window
                win_buf_rcv[temp_buff.seq].received=1;//segno pacchetto n-esimo come ricevuto
                strcpy(win_buf_rcv[temp_buff.seq].payload,temp_buff.payload);//memorizzo il pacchetto n-esimo
                if(temp_buff.seq==window_base_rcv) {//se pacchetto riempie un buco
                    // scorro la finestra fino al primo ancora non ricevuto
                    while (win_buf_rcv[window_base_rcv].received ==1) {
                        //dentro il buffer non deve esserci il terminatore
                        writen(fd,win_buf_rcv[window_base_rcv].payload,strlen(win_buf_rcv[window_base_rcv].payload));//necessario cosi non copia il terminatore
                        //controllo su writen
                        byte_written+=strlen(win_buf_rcv[window_base_rcv].payload);
                        win_buf_rcv[window_base_rcv].received=0;//segna pacchetto come non ricevuto
                        window_base_rcv=(window_base_rcv+1)%(2*W);//avanza la finestra con modulo di 2W
                    }
                    if(byte_written==byte_expected){
                        while(1){//chiusura di connessione
                            alarm(5);
                            if(recvfrom(sockfd,&temp_buff,MAXPKTSIZE,0,(struct sockaddr*)&serv_addr,&len)!=-1){
                                alarm(0);//resetto il timer perchè ho ricevuto un pacchetto
                                if(temp_buff.seq>(2*W-1)){//num sequenza imprevisto
                                    //ignora
                                }
                                else if(temp_buff.seq==-2){//fine trasferimento
                                    return byte_written;
                                }
                                else if(!seq_is_in_window(window_base_rcv,window_base_rcv+W-1,W,temp_buff.seq)){
                                    //se il numero  non è dentro la finestra
                                    // un ack è stato smarrito->rinvialo
                                    if(sendto(sockfd,&(temp_buff.seq), sizeof(int),0,(struct sockaddr*)&serv_addr,sizeof(serv_addr))==-1) {//rinvio ack
                                        handle_error_with_exit("error in sendto\n");
                                    }
                                }
                            }
                            else if(great_alarm==1) {
                                break;
                            }
                        }
                    }
                }
            }
        }
        else if (great_alarm==1){
            printf("il sender non manda più nulla o errore interno\n");
            great_alarm=0;
            return byte_written;
        }
    }
    great_alarm=0;
    return byte_written;
}

int list_command(int sockfd,struct sockaddr_in serv_addr){//svolgi la list con connessione già instaurata

}
int put_command(int sockfd,struct sockaddr_in serv_addr,char*filename){

}
struct sockaddr_in syn_ack(int sockfd,struct sockaddr_in main_servaddr){
    struct sigaction sa;
    socklen_t len=sizeof(main_servaddr);
    sa.sa_sigaction = handler;
    char rtx=0;
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    while(rtx<6){//scaduto 5 volte il timer il server non risponde
        sendto(sockfd,NULL,0,0,(struct sockaddr *)&main_servaddr,sizeof(main_servaddr));//mando syn al processo server principale
        alarm(3);//alarm per ritrasmissione del syn
        if(recvfrom(sockfd,NULL,0,0,(struct sockaddr *)&main_servaddr,&len)!=-1){
            alarm(0);
            printf("connessione instaurata\n");
            great_alarm=0;
            return main_servaddr;//ritorna l'indirizzo del processo figlio del server
        }
        rtx++;
    }
    great_alarm=0;
    printf("il server non è in ascolto\n");
    return NULL;
}

void*thread_job(void*arg){
    //waitpid dei processi del client
    pid_t pid;
    while(1) {
        while ((pid = waitpid(-1, NULL, 0)) > 0) {
            printf("process %d\n", pid);
        }
    }
    return NULL;
}
void create_thread_waitpid(){
    pthread_t tid;
    pthread_create(&tid,0,thread_job,NULL);
    return;
}
void client_list_job(){
    struct sockaddr_in serv_addr,cliaddr;
    int sockfd;
	printf("client list job\n");
    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET,"127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket\n");
    }
    memset((void *)&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family=AF_INET;
    cliaddr.sin_port=htons(0);
    if (bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
       handle_error_with_exit("error in bind\n");
    }
    serv_addr=syn_ack(sockfd,serv_addr);
    list_command(sockfd,serv_addr);
	exit(EXIT_SUCCESS);
}
void client_get_job(char*filename){
	printf("client get job\n");
    struct sockaddr_in serv_addr,cliaddr;
    int sockfd;
    printf("client list job\n");
    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET,"127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket\n");
    }
    memset((void *)&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family=AF_INET;
    cliaddr.sin_port=htons(0);
    if (bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    serv_addr=syn_ack(sockfd,serv_addr);
    get_command(sockfd,serv_addr,filename);
	exit(EXIT_SUCCESS);
}
void client_put_job(char*filename){//upload e filename già verificato
	printf("client put_job");
    struct sockaddr_in serv_addr,cliaddr;
    int sockfd;
    printf("client list job\n");
    memset((void *)&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET,"127.0.0.1", &serv_addr.sin_addr) <= 0) {
        handle_error_with_exit("error in inet_pton");
    }
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket\n");
    }
    memset((void *)&cliaddr, 0, sizeof(cliaddr));
    cliaddr.sin_family=AF_INET;
    cliaddr.sin_port=htons(0);
    if (bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0) {
        handle_error_with_exit("error in bind\n");
    }
    serv_addr=syn_ack(sockfd,serv_addr);
    put_command(sockfd,serv_addr,filename);
	exit(EXIT_SUCCESS);
}

struct select_param param_client ;
char*dir_client;

int main(int argc,char*argv[]) {
    char *filename,*command,conf_upload[4],buff[MAXPKTSIZE+1],*line;
    int   path_len,fd;
    socklen_t len;
    pid_t pid;
    struct    sockaddr_in servaddr,cliaddr,main_servaddr;

    if(argc!=2){
       	handle_error_with_exit("usage <directory>\n");
    }
    dir_client=argv[1];
    check_if_dir_exist(dir_client);
    fd=open("/home/cristian/Scrivania/parameter.txt",O_RDONLY);
    if(fd==-1){
        handle_error_with_exit("error in read parameters into file\n");
    }
    line=malloc(sizeof(char)*MAXLINE);
	command=line;
    memset(line,'\0',MAXLINE);
    if(readline(fd,line,MAXLINE)<=0){
	handle_error_with_exit("error in read line\n");
    }
    param_client.window=parse_integer_and_move(&line);
    skip_space(&line);
    param_client.loss_prob=parse_double_and_move(&line);
    skip_space(&line);
    param_client.timer_ms=parse_long_and_move(&line);
    path_len=strlen(dir_client);
    if(close(fd)==-1){
	handle_error_with_exit("error in close file\n");
    }
    line=command;//rimando indietro line spostato
    command=NULL;
    free(line);
    create_thread_waitpid();
    if((command=malloc(sizeof(char)*5))==NULL){
        handle_error_with_exit("error in malloc buffercommand\n");
    }
    if((filename=malloc(sizeof(char)*(MAXFILENAME)))==NULL){
        handle_error_with_exit("error in malloc filename\n");
    }
    for(;;) {
        check_and_parse_command(command,filename);//inizializza command,filename e size
        if(filename!=NULL){
        }
        if(!is_blank(filename) && (strcmp(command,"put")==0)){
            char*path=alloca(sizeof(char)*(strlen(filename)+path_len+1));
            strcpy(path,dir_client);
            move_pointer(&path,path_len);
            strcpy(path,filename);
            path=path-path_len;
            printf("%s\n",path);
            if(!check_if_file_exist(path)){
                printf("file not exist\n");
                continue;
            }
            else{
                printf("file size is %d bytes\n",get_file_size(path));
                while(1){
                    printf("confirm upload file %s [y/n]\n",filename);
                    if(fgets(conf_upload,MAXLINE,stdin)==NULL){
                        handle_error_with_exit("error in fgets\n");
                    }
                    if(strlen(conf_upload)!=2){
                            printf("type y or n\n");
                            continue;
                    }
                    else if(strncmp(conf_upload,"y",1)==0){
                   
                        printf("confirm\n");
			if ((pid = fork()) == -1) {
            			handle_error_with_exit("error in fork\n");
        		}
        		if (pid == 0) {
            			client_put_job(filename);//i figli non ritorna mai
        		}
                        break;
                    }
                    else if(strncmp(conf_upload,"n",1)==0){
                        printf("not confirm\n");
                        break;//esci e riscansiona l'input
                    }
                }
            }
        }
        else if(!is_blank(filename) && strcmp(command,"get")==0){
            //fai richiesta al server del file
		if ((pid = fork()) == -1) {
            		handle_error_with_exit("error in fork\n");
        	}
        	if (pid == 0) {
            		client_get_job(filename);//i figli non ritorna mai
        	}
        }
        else if(1){//list
	    	if ((pid = fork()) == -1) {
            		handle_error_with_exit("error in fork\n");
        	}
        	if (pid == 0) {
            		client_list_job();//i figli non ritorna mai
        	}
        }
        //make_request_to_server(sockfd, servaddr, command, filename);//fai la richiesta al server
        //ogni nuova richiesta viene mandata al main process server
    }
    return EXIT_SUCCESS;
    //il client dopo aver verificato la stringa la manda:
    //list senza spazi
    //get 1 spazio nome file oppure put 1 spazio nome file
    //e l'eventuale dimensione del file dove la mettiamo??
}
