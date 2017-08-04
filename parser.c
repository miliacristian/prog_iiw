#include "basic.h"
#include "io.h"
#include "lock_fcntl.h"
#include "parser.h"
#include "receiver.h"
#include "sender2.h"

void move_pointer(char**string,int n){
    if(string==NULL || *string==NULL){
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
    if(*string==NULL || string==NULL){
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
    if(*string==NULL || string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    int value;
    errno = 0;
    value= (int) strtol(*string, &errptr, 0);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ' && *errptr!='\n')) {
        handle_error_with_exit("invalid number\n");
    }
    *string=errptr;//sposta il puntatore
    return value;
}

long parse_long_and_move(char**string) {
    if(*string==NULL || string==NULL){
        handle_error_with_exit("error in parse_integer\n");
    }
    char*errptr;
    long value;
    errno = 0;
    value=strtol(*string, &errptr, 0);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ' && *errptr!='\n')) {
        handle_error_with_exit("invalid number\n");
    }
	*string=errptr;//sposta il puntatore
    return value;
}

double parse_double_and_move(char**string){
    if(*string==NULL || string==NULL){
        handle_error_with_exit("error in parse_double\n");
    }
    char*errptr;
    double value;
    errno = 0;
    value=strtod(*string, &errptr);
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ' && *errptr!='\n')) {
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
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ')) {
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
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ')) {
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
    if (errno != 0 || (*errptr != '\0' && *errptr!=' ')) {
        handle_error_with_exit("invalid number\n");
    }
    return value;
}

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
