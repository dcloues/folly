
#include "stdlib.h"
#include "linked_list.h"

linked_list *ll_create(void) {
	linked_list *list = malloc(sizeof(linked_list));
	if (list) {
		list->size = 0;
		list->head = NULL;
		list->tail = NULL;
	}

	return list;
}

void ll_destroy(linked_list *list, destructor dest) {
	ll_node *node = list->head;
	ll_node *tmp = NULL;
	while (node) {
		if (dest) {
			dest(node->data);
		}

		tmp = node;
		node = node->next;
		free(tmp);
		tmp = NULL;
	}

	free(list);
}

void ll_insert_tail(linked_list *list, void *data) {
	ll_node *node = ll_node_create(data);
	if (list->tail) {
		list->tail->next = node;
	} else {
		list->head = node;
	}
	list->tail = node;
	list->size++;
}

void ll_insert_head(linked_list *list, void *data) {
	ll_node *node = ll_node_create(data);
	node->next = list->head;
	list->head = node;
	list->size++;
	if (list->size == 1) {
		list->tail = list->head;
	}
}

ll_node *ll_node_create(void *data) {
	ll_node *node = malloc(sizeof(ll_node));
	if (node) {
		node->data = data;
		node->next = NULL;
	}

	return node;
}

ll_node *ll_search(linked_list *list, void *seek, comparator comp) {
	ll_node *node = list->head;
	while (node) {
		if (comp(seek, node->data) == 0) {
			return node;
		}
		node = node->next;
	}

	return NULL;
}

ll_node *ll_search_simple(linked_list *list, void *seek) {
	ll_node *node = list->head;
	while (node) {
		if (node->data == seek) {
			return node;
		}
	}

	return NULL;
}

int ll_remove_first(linked_list *list, const void *what) {
	ll_remove(list, what, 1);
}

int ll_remove(linked_list *list, const void *what, const int limit) {
	if (list->size == 0) {
		return -1;
	} else if (list->size == 1) {
		if (list->head->data == what) {
			list->size--;
			free(list->head);
			list->head = NULL;
			list->tail = NULL;
			return 1;
		} else {
			return -1;
		}
	} else {
		ll_node *node = list->head;
		ll_node *prev = NULL;
		int removed = 0;
		while (node && (limit < 0 || removed < limit)) {
			if (node->data == what) {
				list->size--;
				removed++;
				if (prev) {
					prev->next = node->next;
					free(node);
					node = prev->next;
					if (!node) {
						list->tail = prev;
					}
				} else {
					list->head = node->next;
					if (!list->head) {
						list->tail = NULL;
					}
					free(node);
					node = list->head;
				}
			} else {
				prev = node;
				node = node->next;
			}
		}

		return removed;
	}
}

void ll_remove_node(linked_list *list, ll_node *node) {
	if (node == list->head) {
		list->head = node->next;
		if (!list->head) {
			list->tail = NULL;
		}
	}
}

