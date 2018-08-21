#include "list.h"

//deletes a list
void delete_list(ListHead *lst){
	ListElement* elem=lst->first;
	while(elem!=NULL){
		free(elem->item);
		elem=elem->next;
		free(elem->prev);
	}
}
