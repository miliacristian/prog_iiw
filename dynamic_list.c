#include "dynamic_list.h"

void initialize_timeval(struct timespec *tv,int timer_ms){
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
//Creates a new Node and returns pointer to it.
struct Node* GetNewNode(int seq,int timer_ms) {
    struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
    newNode->seq = seq;
    if(clock_gettime(CLOCK_MONOTONIC,&(newNode->tv))!=0){
        handle_error_with_exit("error in get_time\n");
    }
    newNode->timer_ms=timer_ms;
    newNode->prev = NULL;
    newNode->next = NULL;
    return newNode;
}
int deleteHead(struct Node** head, struct Node* oldHead){
    oldHead->tv =(*head)->tv;
    oldHead->seq =(*head)->seq;
    oldHead->next = (*head)->next;
    oldHead->prev = (*head)->prev;
    if(*head == NULL){
        fprintf(stderr, "empty list\n");
        return -1;
    }
    else if ((*head)-> next == NULL){
        *head = NULL;
        free(*head);
    }else{
        *head = oldHead->next;
        (*head)-> prev = NULL;
        free(*head);
    }
    return 0;
}

void InsertAtHead(struct Node* newNode,struct Node** head,struct Node** tail) {
    if(*head == NULL) {
        *head = newNode;
        *tail = newNode;
        return;
    }
    (*head)->prev = newNode;
    newNode->next = *head;
    *head = newNode;
}

char first_is_smaller(struct Node node1, struct Node node2){
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

void InsertOrdered(int seq,int timer_ms, struct Node** head, struct Node** tail){
    struct Node* temp = *tail;
    struct Node* nextNode = NULL;
    struct Node* newNode = GetNewNode(seq,timer_ms);
    if(*head == NULL) {
        *head = newNode;
        *tail = newNode;
        return;
    }
    if(first_is_smaller((**tail),*newNode)){
        (*tail)->next = newNode;
        newNode->prev = *tail;
        *tail = newNode;
    }else{
        while(!first_is_smaller(*temp,*newNode)){
            if(temp->prev != NULL){
                temp = temp->prev;
            }else{
                InsertAtHead(newNode, head, tail);
                return;
            }
        }
        nextNode = temp->next;
        newNode->prev = nextNode;
        newNode->next = temp->next;
        temp->next = newNode;
        newNode->prev = temp;
    }
}

void Print(struct Node* head) {
    struct Node* temp = head;
    while(temp != NULL) {
        printf("seq %d sec %d usec %d\n",temp->seq,temp->tv.tv_sec,temp->tv.tv_nsec);
        temp = temp->next;
    }
    printf("\n");
}

void ReversePrint(struct Node* head) {
    struct Node* temp = head;
    if(temp == NULL) return; // empty list, exit
    // Going to last Node
    while(temp->next != NULL) {
        temp = temp->next;
    }
    // Traversing backward using prev pointer
    while(temp != NULL) {
        printf("%d ",temp->seq);
        temp = temp->prev;
    }
    printf("\n");
}