#include <stdio.h>
#include "list.h"
#include <stdlib.h>
#include "errors.h"

extern int quiet;

struct list* new_list() 
{
	struct list* lst;

	if( ( lst = (struct list*) malloc(sizeof(struct list)) )==NULL ) 
    {
		//err_die("Malloc failed", quiet);
		err_print("Malloc failed", quiet);
        return NULL;
    }
	lst->head = NULL;
	return lst;
};

struct list_item* new_list_item(unsigned long content) 
{
	struct list_item* lst_item;

	if( (lst_item = (struct list_item*) malloc(sizeof(struct list_item)) )==NULL  )
    {
		//err_die("Malloc failed", quiet);
		err_print("Malloc failed", quiet);
        return NULL;
    }

	lst_item->next = NULL;
	lst_item->prev = NULL;
	lst_item->content = content;
	return lst_item;
};

// Fixed memory leak point 3
// This function don't work , cause the pointer->prev always equal NULL
void delete_list(struct list* list) 
{
    //struct list_item* pointer = NULL;
	//pointer = list->head;

    if (list->head) 
    {
        list->head->prev = NULL;
        while( list->head->next )
        {
            list->head->next->prev = list->head;
            list->head = list->head->next; // Find last list element
        }
        
	    //Go back from tail to head deleteing list items on the way 
   	    while(list->head->prev) 
        {
            list->head = list->head->prev;
            free(list->head->next);
            list->head->next = NULL; 
            
	    };
        free(list->head);
        list->head = NULL;
    }
    /*
    if (pointer) 
    {
        pointer->prev = NULL;
        while( pointer->next )
        {
            pointer->next->prev = pointer;
            pointer = pointer->next; // Find last list element
        }
        
	    //Go back from tail to head deleteing list items on the way 
   	    while(pointer->prev) 
        {
            pointer = pointer->prev;
            free(pointer->next);
            pointer->next = NULL; 
	    };
        free(pointer);
        pointer = NULL;
    }
    */
    
    free(list);
    list = NULL;
};
	
int compare(struct list_item* item1, struct list_item* item2) {
	if(item2==NULL) return ERROR;
	if(item1==NULL) return 1;
	if(item1->content == item2->content) return 0;
	if(item1->content > item2->content) return 1;
	if(item1->content < item2->content) return -1;
};

int insert(struct list* lst, unsigned long content) {

	struct list_item *temp_item, *item;
	int cmp;

	item = new_list_item(content);
    if( item == NULL )
    {
        return MALLOC_FAILED;
    }
	
	cmp = compare(lst->head, item);
	if(lst->head==NULL) {
		lst->head=item;
		return OK;
	} else if (cmp==1) {
		item->next=lst->head;
		lst->head = item;
		item->prev = NULL;
		return OK;
	} else if (cmp==0) {
		free(item);
		return N_OK;
	} else if (cmp==-1) {
		temp_item = lst->head;
		while(compare(temp_item->next, item)==-1) {
			temp_item = temp_item->next;
		}
		/* temp_item points to last list element less then item */
		/* we shall insert item after temp_item */
		if(compare(temp_item->next, item)==0) {
			free(item);
			return N_OK;
		} else if(compare(temp_item->next, item)==ERROR) {
			free(item);
			return ERROR;
		} else if(compare(temp_item->next, item)==1) {
			item->next=temp_item->next;
			item->prev=temp_item;
			if(temp_item->next) temp_item->next->prev = item;
			temp_item->next = item;
			return OK;
		};
	} else if (compare(lst->head, item)==ERROR) {
		free(item);
		return ERROR;
	};
};

// Fixed memory leak point 4 
int in_list(struct list* lst, unsigned long content) {
	struct list_item *temp_item, *item;
        int cmp;

	item = new_list_item(content); // malloc at  , line 19 ( list.c ) 

    if( item == NULL )
    {
        return MALLOC_FAILED;
    }

	if(lst->head==NULL)
    {
        if( item )
        {
            free( item );
        }
        return N_OK;
    }
	temp_item=lst->head;
	
	while(compare(temp_item, item) < 0) temp_item = temp_item->next;
	if (compare(temp_item, item)==0)
    {
        if( item )
        {
            free( item );
        }
        return OK;
    }
    
    if( item )
    {
        free( item );
    }

	return N_OK;
};
