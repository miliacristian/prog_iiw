#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

void client_list_job(){
	printf("client list job\n");
	exit(EXIT_SUCCESS);
}
void client_get_job(){
	printf("client get job\n");
	exit(EXIT_SUCCESS);
}
void client_put_job(){
	printf("client put_job");
	exit(EXIT_SUCCESS);
}

struct select_param param_client ;
char*dir_client;

int main(int argc,char*argv[]) {
    char *filename,*command,conf_upload[4],buff[MAXPKTSIZE+1],*line;
    int   sockfd,path_len,fd;
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
    //impostare con setsockopt timer (ricezione o trasmissione?)
    // per dire che il server non è in ascolto
    if((command=malloc(sizeof(char)*5))==NULL){
        handle_error_with_exit("error in malloc buffercommand\n");
    }
    if((filename=malloc(sizeof(char)*(MAXFILENAME)))==NULL){
        handle_error_with_exit("error in malloc filename\n");
    }
    for(;;) {
        check_and_parse_command(command,filename);//inizializza command,filename e size
        if(filename!=NULL){
            printf("filename %s\n",filename);
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
            			client_put_job();//i figli non ritorna mai
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
            		client_get_job();//i figli non ritorna mai
        	}
        }
        else if(1){//list
	    	if ((pid = fork()) == -1) {
            		handle_error_with_exit("error in fork\n");
        	}
        	if (pid == 0) {
            		client_list_job();//i figli non ritorna mai
        	}
            	/*char*l="list";
            	if(sendto(sockfd,l,strlen(l),0,(struct sockaddr*)&main_servaddr,sizeof(main_servaddr))<0){
                	handle_error_with_exit("error in request list\n");
            	}
            	printf("invio comando list in corso\n");
            	len=sizeof(servaddr);
            	if((recvfrom(sockfd,buff,MAXPKTSIZE,0,(struct sockaddr*)&servaddr,&len))<0){
                	handle_error_with_exit("error in recv\n");
            	}
            	printf("messaggio ricevuto client\n");
            	buff[MAXPKTSIZE]=0;
            	printf("%s\n",buff);*/
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
