#include "dynamic_list.h"

void insert_first(struct node *new_node, struct node **head, struct node **tail){//inserisce il primo nodo
    if(new_node==NULL || head==NULL || tail==NULL){
        handle_error_with_exit("error in insert_first\n");
    }
    *head = new_node;
    *tail = new_node;
    return;
}

void insert_at_tail(struct node *new_node,struct node**head,struct node** tail){//inserisce un nodo in coda
    if(head==NULL){
        handle_error_with_exit("error in insert_at_head **head is NULL\n");
    }
    if(tail==NULL){
        handle_error_with_exit("error in insert_at_head **tail is NULL\n");
    }
    if(*head == NULL) {
        insert_first(new_node, head, tail);
        return;
    }
    (*tail)->next = new_node;
    new_node->prev = *tail;
    *tail = new_node;
}


//alloca e inizializza un nodo della lista dinamica ordinata
struct node* get_new_node(int seq,int lap,struct timespec timespec,int timer_ms) {
    if(seq<0 || lap<0 ){
        handle_error_with_exit("error in get_new_node seq or lap invalid\n");
    }
    if(timer_ms<0){
        handle_error_with_exit("error in get_new_node timer smaller than 0\n");
    }
    struct node* new_node = (struct node*)malloc(sizeof(struct node));
    if(new_node==NULL){
        handle_error_with_exit("error in malloc get_new_node\n");
    }
    new_node->seq = seq;
    new_node->tv.tv_sec=timespec.tv_sec;
    new_node->tv.tv_nsec=timespec.tv_nsec;
    new_node->timer_ms=timer_ms;
    new_node->lap=lap;
    new_node->prev = NULL;
    new_node->next = NULL;
    return new_node;
}

int delete_head(struct node** head, struct node* old_head){
    //initializza oldhead con il primo nodo della lista e distrugge il primo nodo della lista
    if(head==NULL){
        handle_error_with_exit("error in delete head\n");
    }
    if(*head == NULL){
        return -1;
    }
    if (old_head == NULL) {
        handle_error_with_exit("oldHead NULL\n");
    }
    old_head->timer_ms = (*head)->timer_ms;
    old_head->tv =(*head)->tv;
    old_head->seq =(*head)->seq;
    old_head->next = (*head)->next;
    old_head->prev = (*head)->prev;
    old_head->lap = (*head)->lap;
    if ((*head)-> next == NULL){
        free(*head);
        *head = NULL;

    }else{
        free(*head);
        *head = old_head->next;
        (*head)-> prev = NULL;
    }
    return 0;
}

void insert_at_head(struct node* new_node,struct node** head,struct node** tail) {//inserisce un nodo in testa alla lista
    if(head==NULL){
        handle_error_with_exit("error in insert_at_head **head is NULL\n");
    }
    if(tail==NULL){
        handle_error_with_exit("error in insert_at_head **tail is NULL\n");
    }
    if(*head == NULL) {
        insert_first(new_node, head, tail);
        return;
    }
    (*head)->prev = new_node;
    new_node->next = *head;
    *head = new_node;
    return;
}


char first_is_smaller(struct node node1, struct node node2){//verifica se il primo nodo contiene tempi più piccoli del secondo nodo
    initialize_timeval(&(node1.tv), node1.timer_ms);
    initialize_timeval(&(node2.tv), node2.timer_ms);
    if(node1.tv.tv_sec>node2.tv.tv_sec){
        return 0;
    }
    else if(node1.tv.tv_sec==node2.tv.tv_sec){
        if(node1.tv.tv_nsec>=node2.tv.tv_nsec){
            return 0;
        }
        else {
            return 1;
        }
    }
    else{
        return 1;
    }
}

void insert_ordered(int seq,int lap,struct timespec timespec,int timer_ms, struct node** head, struct node** tail){
    //inserisce ordinatamente un nodo nella lista ordinata per istanti temporali
    struct node* temp = *tail;
    struct node* next_node = NULL;
    struct node* new_node = get_new_node(seq,lap,timespec,timer_ms);
    if(head==NULL || tail==NULL){
        handle_error_with_exit("error in insert_ordered,head or tail are NULL\n");
    }
    if(*head == NULL) {
        insert_first(new_node, head, tail);
        return;
    }
    if(first_is_smaller((**tail),*new_node)){
        insert_at_tail(new_node,head, tail);
    }else{
        while(!first_is_smaller(*temp,*new_node)){
            if(temp->prev != NULL){
                temp = temp->prev;
            }else{
                insert_at_head(new_node, head, tail);
                return;
            }
        }
        next_node = temp->next;
        new_node->prev = temp;
        new_node->next = next_node;
        temp->next = new_node;
        next_node->prev = new_node;
    }
    return;
}




