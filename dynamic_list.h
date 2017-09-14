#include "basic.h"

struct node* get_new_node(int seq,int lap,struct timespec timespec,int timer_ms);
int delete_head(struct node** head, struct node* old_head);
void insert_at_head(struct node* new_node, struct node** head,struct node** tail);
void insert_ordered(int seq,int lap,struct timespec timespec,int timer_ms, struct node** head, struct node** tail);
void print(struct node* head);
void reverse_print(struct node* head);
