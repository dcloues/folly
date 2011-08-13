#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"
#include "linked_list.h"
#include "log.h"
#include "mm.h"
#include "smalloc.h"
#include "type.h"

#define DEFAULT_CHUNK_SIZE 512

void mark(hval *hv);
void sweep(mem *m);
chunk *chunk_create(int size);
static hval *mem_alloc_helper(mem *m, bool run_gc);

chunk *chunk_create(int size)
{
	chunk *chnk = smalloc(sizeof(chunk) + size * sizeof(hval));
	chnk->size = size;
	chnk->free_hint = chnk->contents;
	chnk->allocated = 0;
	chnk->free_list = ll_create();
	for (hval *mark_free = chnk->contents; mark_free < chnk->contents + chnk->size; mark_free++) {
		mark_free->type = free_t;
		mark_free->members = NULL;
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
	/*for (chunk *chnk = mem->chunks + mem->num_chunks - 1; chnk >= mem->chunks; chnk--) {*/
	for (int i = 0; i < mem->num_chunks; i++) {
		chunk *chnk = mem->chunks[i];
		for (hval *hv = chnk->contents + chnk->size - 1; hv >= chnk->contents; hv--) {
			if (hv->type != free_t) {
				hval_destroy(hv, mem, false);
			}

			if (hv->members != NULL) {
				hash_destroy(hv->members, NULL, NULL, NULL, NULL);
				/*free(hv->members);*/
				hv->members = NULL;
			}
		}

		ll_destroy(chnk->free_list, NULL, NULL);
		free(chnk);
	}

	free(mem->chunks);
	ll_destroy(mem->gc_roots, NULL, NULL);
	free(mem);
}

hval *chunk_get_free(chunk *chnk)
{
	if (chnk->allocated == chnk->size) {
		return NULL;
	}

	hval *hv = NULL;
	if (chnk->free_list->size > 0) {
		hv = (hval *) chnk->free_list->head->data;
		ll_remove_first(chnk->free_list, hv);
		chnk->allocated++;
		return hv;
	}

	if (chnk->free_hint < chnk->contents + chnk->size && chnk->free_hint->type == free_t) {
		hval *hv = chnk->free_hint;
		chnk->free_hint++;
		chnk->allocated++;
		return hv;
	}

	return NULL;
}

hval *mem_alloc(mem *m) {
	return mem_alloc_helper(m, true);
}

static hval *mem_alloc_helper(mem *m, bool run_gc)
{
	hval *hv = NULL;
	for (int i=m->num_chunks - 1; i >= 0; i--) {
		chunk *chnk = m->chunks[i];
		hv = chunk_get_free(chnk);
		if (hv) {
			return hv;
		}
	}

	if (run_gc) {
		gc(m);
		return mem_alloc_helper(m, false);
	}

	printf("growing heap:\n");
	debug_heap_output(m);

	m->chunks = realloc(m->chunks, sizeof(chunk *) * (m->num_chunks + 1));
	m->chunks[m->num_chunks] = chunk_create(DEFAULT_CHUNK_SIZE);
	hv = chunk_get_free(m->chunks[m->num_chunks]);
	if (hv == NULL) {
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

#if GC_REPORTING
	fprintf(stderr, "%d gc roots\n", m->gc_roots->size);
#endif
	ll_node *node = m->gc_roots->head;
	while (node) {
#if GC_REPORTING
		fprintf(stderr, "gc root: %p\n", node->data);
#endif
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
		/*for (hval *hv = chnk->contents; hv < chnk->contents + chnk->size; hv++) {*/
		for (hval *hv = chnk->contents + chnk->size - 1; hv >= chnk->contents; --hv) {
			if (hv->type != free_t && !hv->reachable) {
				hval_destroy(hv, mem, false);
				hv->type = free_t;
				chnk->allocated--;
				ll_insert_head(chnk->free_list, hv);
				/*chnk->free_hint = hv;*/
			}
		}
	}
}

void debug_heap_output(mem *mem)
{
	printf("heap: %d chunks\n", mem->num_chunks);
	int total = 0;
	for (int i = 0; i < mem->num_chunks; i++) {
		chunk *chnk = mem->chunks[i];
		printf(" chunk %p: %6d / %6d\n", chnk, chnk->allocated, chnk->size);
	}
}
