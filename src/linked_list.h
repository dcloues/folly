#ifndef LINKED_LIST_H
#define LINKED_LIST_H

enum comparison { LT = -1, EQ = 0, GT = 1 };

//typedef comparison (*comparator)(void *, void*);
typedef int (*comparator)(void*, void*);
typedef void (*destructor)(void *);

typedef struct _ll_node {
	struct _ll_node *next;
	void *data;
} ll_node;

typedef struct _linked_list {
	ll_node *head;
	ll_node *tail;
	int size;
} linked_list;

linked_list *ll_create(void);
void ll_destroy(linked_list *list, destructor dest);
ll_node *ll_node_create(void *data);
void ll_insert_tail(linked_list *list, void *data);
void ll_insert_head(linked_list *list, void *data);
int ll_remove_first(linked_list *list, const void *what);
int ll_remove(linked_list *list, const void *what, const int limit);
//void ll_remove_all(linked_list *list, filter element_filter);
ll_node *ll_search(linked_list *list, void *seek, comparator comp);

#endif
