#include "list.h"

//deletes a list
void delete_list(ListHead *lst){
	if(lst==NULL){
		return;
	}
	ListElement* elem=lst->first;
	while(elem!=NULL){
		if(elem->item!=NULL) {
			free(elem->item);
		}
		if (elem->next != NULL) {
			free(elem->prev);
		}
		elem=elem->next;
	}
}
