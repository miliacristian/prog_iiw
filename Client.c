#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

void handler(){
    return;
}
struct sockaddr_in syn_ack(int sockfd,struct sockaddr_in main_servaddr){
    struct sigaction sa;
    socklen_t len=sizeof(main_servaddr);
    sa.sa_sigaction = handler;
    if (sigemptyset(&sa.sa_mask) == -1) {
        handle_error_with_exit("error in sig_empty_set\n");
    }
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        handle_error_with_exit("error in sigaction\n");
    }
    while(1){
        sendto(sockfd,NULL,0,0,(struct sockaddr *)&main_servaddr,sizeof(main_servaddr));//mando syn al processo server principale
        alarm(3);//alarm per ritrasmissione del syn
        if(recvfrom(sockfd,NULL,0,0,(struct sockaddr *)&main_servaddr,&len)!=-1){
            alarm(0);
            printf("connessione instaurata\n");
            return main_servaddr;//ritorna l'indirizzo del processo figlio del server
        }
    }
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
	exit(EXIT_SUCCESS);
}
void client_get_job(char*filename){
	printf("client get job\n");
    struct sockaddr_in main_servaddr,cliaddr;
    int sockfd;
    printf("client list job\n");
    memset((void *)&main_servaddr, 0, sizeof(main_servaddr));
    main_servaddr.sin_family = AF_INET;
    main_servaddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET,"127.0.0.1", &main_servaddr.sin_addr) <= 0) {
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
    syn_ack(sockfd,main_servaddr);
	exit(EXIT_SUCCESS);
}
void client_put_job(char*filename){//upload e filename già verificato
	printf("client put_job");
    struct sockaddr_in main_servaddr,cliaddr;
    int sockfd;
    printf("client list job\n");
    memset((void *)&main_servaddr, 0, sizeof(main_servaddr));
    main_servaddr.sin_family = AF_INET;
    main_servaddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET,"127.0.0.1", &main_servaddr.sin_addr) <= 0) {
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
    syn_ack(sockfd,main_servaddr);
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
