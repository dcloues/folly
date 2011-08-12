#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "linked_list.h"
#include "log.h"
#include "mm.h"
#include "smalloc.h"
#include "type.h"

#define DEFAULT_CHUNK_SIZE 512

void mark(hval *hv);
void sweep(mem *m);
chunk *chunk_create(int size);

chunk *chunk_create(int size)
{
	chunk *chnk = smalloc(sizeof(chunk) + size * sizeof(hval));
	chnk->size = size;
	chnk->free_hint = 0;
	for (hval *mark_free = chnk->contents; mark_free < chnk->contents + chnk->size; mark_free++) {
		mark_free->type = free_t;
	}
	
	return chnk;
}

mem *mem_create() {
	mem *m = malloc(sizeof(mem));
	if (m == NULL) {
		perror("Unable to allocate memory for mem");
		exit(1);
	}

	m->gc_roots = ll_create();
	m->chunks = smalloc(sizeof(chunk *));
	m->num_chunks = 1;
	m->chunks[0] = chunk_create(DEFAULT_CHUNK_SIZE);

	m->gc = false;
	
	return m;
}

void mem_destroy(mem *mem) {
	ll_destroy(mem->gc_roots, NULL, NULL);
	free(mem);
}

hval *chunk_get_free(chunk *chnk)
{
	if (chnk->contents[chnk->free_hint].type == free_t) {
		return chnk->contents + chnk->free_hint;
	}

	for (hval *hv = chnk->contents; hv < chnk->contents + chnk->size; hv++) {
		if (hv->type == free_t) {
			return hv;
		}
	}

	return NULL;
}

hval *mem_alloc(mem *m) {
	hval *hv = NULL;
	for (int i=0; i < m->num_chunks; i++) {
		chunk *chnk = m->chunks[i];
		hv = chunk_get_free(chnk);
		if (hv) {
			return hv;
		}
	}

	m->chunks = realloc(m->chunks, sizeof(chunk *) * (m->num_chunks + 1));
	m->chunks[m->num_chunks] = chunk_create(DEFAULT_CHUNK_SIZE);
	hv = chunk_get_free(m->chunks[m->num_chunks]);
	if (hv == NULL) {
		printf("unable to grow heap\n");
		exit(2);
	}
	m->num_chunks++;
	return hv;
}

void mem_free(mem *m, hval *hv) {
	// TODO consider resetting the free hint?
	hv->type = free_t;
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
	for (int i = 0; i < m->num_chunks; i++) {
		chunk *chunk = m->chunks[i];
		for (hval *chunk_val = chunk->contents, *max = chunk->contents + chunk->size; chunk_val < max; chunk_val++) {
			chunk_val->reachable = false;
		}
	}

	ll_node *node = m->gc_roots->head;
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
	for (int i = 0; i < mem->num_chunks; i++) {
		chunk *chnk = mem->chunks[i];
		for (hval *hv = chnk->contents; hv < chnk->contents + chnk->size; hv++) {
			if (hv->type != free_t && !hv->reachable) {
				hval_destroy(hv, mem, false);
				hv->type = free_t;
			}
		}
	}
}
