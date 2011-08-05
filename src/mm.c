#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "linked_list.h"
#include "log.h"
#include "mm.h"
#include "type.h"

void mark(hval *hv);
void sweep(mem *m);

mem *mem_create() {
	mem *m = malloc(sizeof(mem));
	if (m == NULL) {
		perror("Unable to allocate memory for mem");
		exit(1);
	}

	m->gc_roots = ll_create();
	m->heap = ll_create();
	m->gc = false;
	
	return m;
}

void mem_destroy(mem *mem) {
	ll_destroy(mem->gc_roots, NULL);
	ll_destroy(mem->heap, NULL);
	free(mem);
}

hval *mem_alloc(mem *m) {
	hval *hv = malloc(sizeof(hval));
	if (hv == NULL) {
		perror("Unable to allocate memory for hval");
		exit(1);
	}

	ll_insert_tail(m->heap, hv);
	return hv;
}

void mem_free(mem *m, hval *hv) {
	if (ll_remove_first(m->heap, hv) == 1) {
		free(hv);
	}
}

void mem_add_gc_root(mem *m, hval *root) {
	hlog("add_gc_root: %p\n", root);
	ll_insert_head(m->gc_roots, root);
}

void mem_remove_gc_root(mem *m, hval *root) {
	hlog("remove_gc_root: %p\n", root);
	if (-1 == ll_remove_first(m->gc_roots, root)) {
		hlog("ERROR: Unable to remove gc root\n");
	}
}

void gc_with_temp_root(mem *m, hval *root) {
	if (root) {
		mem_add_gc_root(m, root);
		gc(m);
		mem_remove_gc_root(m, root);
	} else {
		gc(m);
	}
}

void gc(mem *m) {
	m->gc = true;
	ll_node *node = m->heap->head;
	while (node) {
		hval *data = node->data;
		data->reachable = false;
		node = node->next;
	}

	node = m->gc_roots->head;
	hlog("marking\n");
	while (node) {
		/*hlog("gc root: %p\n", node->data);*/
		mark(node->data);
		node = node->next;
	}

	sweep(m);
	m->gc = false;
}

void mark(hval *hv) {
	/*hlog("mark: %p\n", hv);*/

	if (!hv || hv->reachable) {
		return;
	}

	hv->reachable = true;
	if (hv->members) {
		hash_iterator *iter = hash_iterator_create(hv->members);
		while (iter->current_key) {
			mark(iter->current_value);
			hash_iterator_next(iter);
		}

		hash_iterator_destroy(iter);
	}

	if (hv->type == list_t) {
		ll_node *node = hv->value.list->head;
		while (node) {
			mark((hval *) node->data);
			node = node->next;
		}
	}
}

void sweep(mem *mem)
{
	hlog("sweeping\n");
	linked_list *new_heap = ll_create();
	ll_node *node = mem->heap->head;
	ll_node *next = NULL;

	while (node) {
		hval *data = node->data;
		next = node->next;
		if (data->reachable) {
			ll_insert_head(new_heap, data);
		} else {
			hlog("sweeping: %p\n", data);
			hval_destroy(data, mem, false);
		}

		node = next;
	}

	ll_destroy(mem->heap, NULL);
	mem->heap = new_heap;
}
