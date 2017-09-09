#include "dynamic_list.h"

void initialize_timeval(struct timespec *tv,int timer_ms){//funzione che somma i tempi di una struct e di un timer
// mettendo il risultato dentro la struct
    if(tv==NULL){
        handle_error_with_exit("error in initialize timeval\n");
    }
    long temp;
    temp=tv->tv_nsec+(timer_ms*1000000);
    if(temp>=1000000000){
        tv->tv_nsec=temp%1000000000;
        tv->tv_sec+=(temp-tv->tv_nsec)/1000000000;
    }else{
        tv->tv_nsec = temp;
    }
    //printf("dopo imcremento timer sec %d usec %d timer %d\n",tv->tv_sec, tv->tv_usec, timer_ms);
    return;
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
        printf("empty list\n");
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
    //printf("nodo eliminato seq %d, lap %d\n", old_head->seq, old_head->lap);
    return 0;
}

void insert_at_head(struct node* new_node,struct node** head,struct node** tail) {//inserisce un nodo in testa alla lista
    if(head==NULL){
        handle_error_with_exit("error in insert_at_head **head is NULL\n");
    }
    if(tail==NULL){
        handle_error_with_exit("error in insert_at_head **head is NULL\n");
    }
    if(*head == NULL) {
        *head = new_node;
        *tail = new_node;
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
        handle_error_with_exit("error in insert_ordered head or tail are NULL\n");
    }
    if(*head == NULL) {
        *head = new_node;
        *tail = new_node;
        return;
    }
    if(first_is_smaller((**tail),*new_node)){
        (*tail)->next = new_node;
        new_node->prev = *tail;
        *tail = new_node;
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
        new_node->prev = next_node;
        new_node->next = temp->next;
        temp->next = new_node;
        new_node->prev = temp;
    }
    return;
}

/*void print(struct node* head) {//stampa la lista ordinata partendo dall'inizio
    struct node* temp = head;
    if (temp == NULL) {
        return;
    }
    while(temp != NULL) {
        printf("seq %d sec %d usec %d\n",temp->seq,temp->tv.tv_sec,temp->tv.tv_nsec);
        temp = temp->next;
    }
}

void reverse_print(struct node* head) {//stampa la lista ordianta partendo dalla fine
    struct node *temp = head;
    if (temp == NULL) {
        return; // empty list, exit
    // Going to last Node
    }
    while(temp->next != NULL) {
        temp = temp->next;
    }
    // Traversing backward using prev pointer
    while(temp != NULL) {
        printf("seq %d sec %d usec %d\n",temp->seq,temp->tv.tv_sec,temp->tv.tv_nsec);
        temp = temp->prev;
    }
}*/