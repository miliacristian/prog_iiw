#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "parser.h"
#include "basic.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc,char*argv[]) {
    char *filename,*command,conf_upload[4],buff[MAXPKTSIZE+1];
    int   sockfd,path_len;
    socklen_t len;
    struct    sockaddr_in servaddr,cliaddr,main_servaddr;
    if (argc !=2) {
        handle_error_with_exit("usage <directory>\n");
        //se timer==0:timer adattativo,altrimenti timer normale
        //ip del server può essere lasciato string
    }
	argv=argv;
    //path_len=strlen(parameter.directory);
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        handle_error_with_exit("error in socket\n");
    }
    memset((void *)&main_servaddr, 0, sizeof(main_servaddr));      /* azzera servaddr */
    main_servaddr.sin_family = AF_INET;       /* assegna il tipo di indirizzo */
    main_servaddr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET,"127.0.0.1", &main_servaddr.sin_addr) <= 0) {
        /* inet_pton (p=presentation) vale anche per indirizzi IPv6 */
        handle_error_with_exit("error in inet_pton");
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
        check_and_parse_command(command,filename);//inizializza word,command,filename e size
        printf("command %s\n",command);
        if(filename!=NULL){
            printf("filename %s\n",filename);
        }
        if(!is_blank(filename) && (strcmp(command,"put")==0)){
            char*path=alloca(sizeof(char)*(strlen(filename)+path_len+1));
            //strcpy(path,parameter.directory);
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
                        //make request & break;
                        printf("confirm\n");
                        break;
                    }
                    else if(strncmp(conf_upload,"n",1)==0){
                        printf("not confirm\n");
                        break;//esci e riscansiona l'input
                    }
                }
            }
        }
        else if(!is_blank(filename) && strcmp(command,"get")){
            //fai richiesta al server del file
        }
        else if(1){
            char*l="list";
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
            printf("%s\n",buff);
        }
        //make_request_to_server(sockfd, servaddr, command, filename);//fai la richiesta al server
        //ogni nuova richiesta viene mandata al main process server
    }
    //comment
    return EXIT_SUCCESS;
    //il client dopo aver verificato la stringa la manda:
    //list senza spazi
    //get 1 spazio nome file oppure put 1 spazio nome file
    //e l'eventuale dimensione del file dove la mettiamo??
}
