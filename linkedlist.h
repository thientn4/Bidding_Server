#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>

typedef struct node {
    void* value;
    struct node* next;
} node_t;

typedef struct list {
    node_t* head;
    int length;
    int (*comparator)(void*, void*);
  	sem_t mutex;
} List_t;


void insertFront(List_t* list, void* valref) {
  	sem_wait(list->mutex); // block 
  
    if (list->length == 0)
        list->head = NULL;
  
    node_t** head = &(list->head);
    node_t* new_node;
    new_node = malloc(sizeof(node_t));

    new_node->value = valref;

    new_node->next = *head;
    *head = new_node;
    list->length++; 
  
  	sem_post(list->mutex); // unblock
}

void insertRear(List_t* list, void* valref) {
  	
    if (list->length == 0) {
        insertFront(list, valref);
        return;
    }
  
  	sem_wait(list->mutex); // block 
  
    node_t* head = list->head;
    node_t* current = head;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = malloc(sizeof(node_t));
    current->next->value = valref;
    current->next->next = NULL;
    list->length++;
  
  	sem_post(list->mutex); // unblock
}

void insertInOrder(List_t* list, void* valref) {
    if (list->length == 0) {
        insertFront(list, valref);
        return;
    }
		
  	sem_wait(list->mutex); // block 
  
    node_t** head = &(list->head);
    node_t* new_node;
    new_node = malloc(sizeof(node_t));
    new_node->value = valref;
    new_node->next = NULL;

    if (list->comparator(new_node->value, (*head)->value) <= 0) {
        new_node->next = *head;
        *head = new_node;
    } 
    else if ((*head)->next == NULL){ 
        (*head)->next = new_node;
    }                                
    else {
        node_t* prev = *head;
        node_t* current = prev->next;
        while (current != NULL) {
            if (list->comparator(new_node->value, current->value) > 0) {
                if (current->next != NULL) {
                    prev = current;
                    current = current->next;
                } else {
                    current->next = new_node;
                    break;
                }
            } else {
                prev->next = new_node;
                new_node->next = current;
                break;
            }
        }
    }
    list->length++;
  
  	sem_post(list->mutex); // unblock
}

void* removeFront(List_t* list) {
  	sem_wait(list->mutex); // block 
  
    node_t** head = &(list->head);
    void* retval = NULL;
    node_t* next_node = NULL;

    if (list->length == 0) {
      	sem_post(list->mutex); // unblock
        return NULL;
    }

    next_node = (*head)->next;
    retval = (*head)->value;
    list->length--;

    node_t* temp = *head;
    *head = next_node;
    free(temp);

  	sem_post(list->mutex); // unblock
    return retval;
}

void* removeRear(List_t* list) {
    if (list->length == 0) {
        return NULL;
    } else if (list->length == 1) {
        return removeFront(list);
    }

  	sem_wait(list->mutex); // block 
  
    void* retval = NULL;
    node_t* head = list->head;
    node_t* current = head;

    while (current->next->next != NULL) { 
        current = current->next;
    }

    retval = current->next->value;
    free(current->next);
    current->next = NULL;

    list->length--;
  
  	sem_post(list->mutex); // unblock
    return retval;
}

void* removeByIndex(List_t* list, int index) {
    if (list->length <= index) {
        return NULL;
    }
  
		sem_wait(list->mutex); // block 
  	
    node_t** head = &(list->head);
    void* retval = NULL;
    node_t* current = *head;
    node_t* prev = NULL;
    int i = 0;

    if (index == 0) {
    	retval = (*head)->value;
        
			node_t* temp = *head;
      *head = current->next;
      free(temp);
        
			list->length--;
      
      sem_post(list->mutex); // unblock
      return retval;
    }
    while (i++ != index) {
        prev = current;
        current = current->next;
    }

    prev->next = current->next;
    retval = current->value;
    free(current);

    list->length--;

  	sem_post(list->mutex); // unblock
    return retval;
}

void deleteList(List_t* list) {
    if (list->length == 0)
        return;
    while (list->head != NULL){
        removeFront(list);
    }
    list->length = 0;
}

void sortList(List_t* list) {
    List_t* new_list = malloc(sizeof(List_t));	
    
	new_list->length = 0;
    new_list->comparator = list->comparator;
    new_list->head = NULL;

    int i = 0;
    int len = list->length;
    for (; i < len; ++i)
    {
        void* val = removeRear(list);
        insertInOrder(new_list, val); 
    }

    node_t* temp = list->head;
    list->head = new_list->head;

    new_list->head = temp;
    list->length = new_list->length;  

    deleteList(new_list);
    free(new_list);  
}
