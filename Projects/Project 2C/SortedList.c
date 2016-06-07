#define _GNU_SOURCE
#include "SortedList.h"
#include <pthread.h>
#include <string.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element)
{
	if(list == NULL || element == NULL) { return; }
	SortedListElement_t *curr = list->next;
	//Go through list as long as we dont point back to the head/dummy node
	while(curr != list)
	{
		//If element's value is less than the current, then insert it before
		if(strcmp(element->key, curr->key) <= 0)
			break;

		curr  = curr->next;
	}
	if(opt_yield & INSERT_YIELD)
		pthread_yield();

	//Update pointers to add element before the current
	element->next = curr;
	element->prev = curr->prev;
	curr->prev->next = element;
	curr->prev = element;
}

int SortedList_delete(SortedListElement_t *element)
{
	//Prevents segfaults!
	if(element == NULL) { return 1; }
	//Make sure the pointers are not corrupted
	if(element->next->prev == element->prev->next)
	{
		if(opt_yield & DELETE_YIELD)
			pthread_yield();
		//Detach element from list by connecting its previous to its next and vice-versa
		element->prev->next = element->next;
		element->next->prev = element->prev;
		element->next = NULL;
		element->prev = NULL;
		return 0;
	}
	return 1;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key)
{
	if(list == NULL || key == NULL) { return NULL; }
	SortedListElement_t *curr = list->next;
	while(curr != list)
	{
		//Element found
		if(strcmp(curr->key, key) == 0)
			return curr;
		if(opt_yield & SEARCH_YIELD)
			pthread_yield();
		curr = curr->next;
	}
	return NULL;
}

int SortedList_length(SortedList_t *list)
{
	int count = 0;
	if(list == NULL) { return -1; }
	SortedListElement_t *curr = list->next;
	while(curr != list)
	{
		count++;
		if(opt_yield & SEARCH_YIELD)
			pthread_yield();
		curr = curr->next;
	}
	return count;
}
