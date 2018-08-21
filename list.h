#pragma once
#include "common.h"
// a list implementation which is used by the users and group framework
typedef struct _ListElement {
	void *item; //the item in the list
	struct _ListElement *next; //the next list element
	struct _ListElement *prev; //the previous list element
} ListElement;

typedef struct _ListHead {
	ListElement *first; //the first element in a list
	ListElement *last; //the last element in a list
} ListHead;

//deletes a list
void delete_list(ListHead *lst);