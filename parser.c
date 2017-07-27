#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include "parser.h"
#include "basic.h"

void move_pointer(char**string,int n){
    if(string==NULL){
        handle_error_with_exit("error in move_pointer\n");
    }
    else if(n>0) {
        for (int i = 0; i < n; i++) {
            (*string)++;
        }
    }
    else if(n<0){
        for (int i = 0; i < n; i++) {
            (*string)--;
        }
    }
    return;
}

int skip_space(char**string){
    if(string==NULL){
        handle_error_with_exit("error in move_pointer\n");
    }
    int count=0;
    while(**string==' '){
        (*string)++;
        count++;
    }
    return count;
}
char is_blank(char*string){
    if(string==NULL){
        handle_error_with_exit("error in is_blank\n");
    }
    while(*string!='\0'){
        if(*string==' '){
            string++;
        }
        else{
            return 0;
        }
    }
    return 1;
}
int parse_integer_and_move(char**string) {
    if(string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    int value;
    errno = 0;
    value= (int) strtol(*string, &errptr, 0);
    if (errno != 0 || *errptr != '\0' || *errptr!=' ') {
        handle_error_with_exit("invalid number\n");
    }
    *string=errptr;//sposta il puntatore
    return value;
}

long parse_long_and_move(char**string) {
    if(string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    long value;
    errno = 0;
    value=strtol(*string, &errptr, 0);
    if (errno != 0 || *errptr != '\0' || *errptr!=' ') {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}

double parse_double_and_move(char**string){
    if(string==NULL){
        handle_error_with_exit("error in parse_double\n");
    }
    char*errptr;
    double value;
    errno = 0;
    value=strtod(*string, &errptr);
    if (errno != 0 || *errptr != '\0' || *errptr!=' ') {
        handle_error_with_exit("invalid number\n");
    }
    *string=errptr;//sposta il puntatore
    return value;
}
long parse_long(char*string) {
    if(string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    long value;
    errno = 0;
    value= (int) strtol(string, &errptr, 0);
    if (errno != 0 || *errptr != '\0' || *errptr!=' ') {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}
int parse_integer(char*string) {
    if(string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    int value;//
    errno = 0;
    value= (int) strtol(string, &errptr, 0);
    if (errno != 0 || *errptr != '\0' || *errptr!=' ') {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}
double parse_double(char*string){
    if(string==NULL){
        handle_error_with_exit("error in parse_double\n");
    }
    char*errptr;
    double value;
    errno = 0;
    value=strtod(string, &errptr);
    if (errno != 0 || *errptr != '\0' || *errptr!=' ') {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}
/*struct parameter check_and_parse_parameter_value(char**argv){//controlla parametri di esecuzione
    if(argv==NULL){
        handle_error_with_exit("error in check_and_parse_parameter_value\n");
    }
    struct parameter parameter;
    errno=0;
    parameter.server_port=parse_integer(argv[1]);
    parameter.window=parse_integer(argv[2]);
    parameter.timer=parse_double(argv[3]);
    parameter.loss_probability=parse_double(argv[4]);
    if((mkdir(argv[5],0777)==-1) && (errno!=EEXIST)){
        handle_error_with_exit("error in make directory\n");
    }
    if(errno==EEXIST){
        printf("directory already exist\n");
    }
    int path_len=strlen(argv[5]);
    if(argv[5][path_len-1]=='/') {
        parameter.directory=malloc(sizeof(char)*(path_len+1));
        if(parameter.directory==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        strcpy(parameter.directory,argv[5]);
        printf("%s\n",parameter.directory);
    }
    else{
        parameter.directory=malloc(sizeof(char)*(path_len+2));
        if(parameter.directory==NULL){
            handle_error_with_exit("error in malloc\n");
        }
        strcpy(parameter.directory,argv[5]);
        strcat(parameter.directory,"/");
        printf("%s\n",parameter.directory);
    }
    if(parameter.timer<0 || parameter.window<=0 || parameter.loss_probability<0 || parameter.loss_probability>100 ){
        handle_error_with_exit("invalid parameter\n");
    }
    return parameter;
}*/

void check_and_parse_command(char*command,char*filename){
    if(command==NULL || filename==NULL){
        handle_error_with_exit("error in check and parse\n");
    }
    char*main_command,*temp_command;
    int moved=0;
    temp_command=alloca(sizeof(char)*(MAXCOMMANDLINE+1));
    main_command=alloca(sizeof(char)*5);
    while(1){
        printf("Choose one command:\n1)list\n2)get <filename>\n3)put <path_file> <byte_file>\n");
        if(fgets(temp_command,MAXCOMMANDLINE,stdin)==NULL){//fgets aggiunge automaticamente
            // newline e il terminatore di stringa!
            handle_error_with_exit("error in read_line\n");
        }
        temp_command[strlen(temp_command)-1]='\0';
        moved=skip_space(&temp_command);
        if(strlen(temp_command)<4){
            printf("invalid command,command too short\n");
            temp_command=temp_command-moved;
            continue;
        }
        strncpy(main_command,temp_command,4);//copio in main_command i 4 byte del comando
        main_command[4]='\0';
        if(strcmp(main_command,"list")==0){
            move_pointer(&temp_command,4);
            moved+=4;
            if(is_blank(temp_command)){
                strcpy(filename,"");
                strcpy(command,"list");
                break;
            }
            else{
                printf("list doesn't allow parameters\n");
                temp_command=temp_command-moved;
                continue;
            }
        }
        else if(strcmp(main_command,"get ")==0){
            move_pointer(&temp_command,4);
            moved+=4;
            int lenght;
            lenght=strlen(temp_command);
            printf("%d\n",lenght);
            if(lenght==0){
                printf("invalid filename\n");
                temp_command=temp_command-moved;
                continue;
            }
            else{
                strcpy(command,"get");
                strcpy(filename,temp_command);
                break;
            }
        }
        else if(strcmp(main_command,"put ")==0){
            move_pointer(&temp_command,4);
            moved+=4;
            int lenght;
            lenght=strlen(temp_command);
            if(lenght==0){
                printf("invalid filename\n");
                temp_command=temp_command-moved;
                continue;
            }
            else{
                strcpy(command,"put");
                strcpy(filename,temp_command);
                break;
            }
        }
        temp_command=temp_command-moved;
        printf("invalid command\n");
    }
    return;
}
