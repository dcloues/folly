#include <assert.h>
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

#if GC_REPORTING
#define GC_LOG(...) (fprintf(stderr, __VA_ARGS__))
#else
#define GC_LOG(...)
#endif

void mark(hval *hv);
void sweep(mem *m);
chunk *chunk_create(size_t element_size, int count);
static hval *mem_alloc_helper(size_t size, mem *m, bool run_gc);

chunk *chunk_create(size_t element_size, int count)
{
	GC_LOG("chunk_create: %ld * %d = %ld\n", element_size, count, element_size * count);
	chunk *chnk = smalloc(sizeof(chunk) + count * element_size);
	chnk->count = count;
	chnk->element_size = element_size;
	chnk->raw_size = count * element_size;
	chnk->free_hint = chnk->base;
	chnk->allocated = 0;
	chnk->free_list = ll_create();
	hval *hv;
	for (char *ptr = chnk->base, *max = chnk->base + count * element_size; ptr < max; ptr += element_size) {
		hv = (hval *) ptr;
		hv->type = free_t;
		hv->members = NULL;
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
	for (int i=0; i < sizeof(m->chunks) / sizeof(chunk_list); i++) {
		m->chunks[i].num_chunks = 0;
		m->chunks[i].chunks = NULL;
	}
	m->gc = false;
	return m;
}

void mem_destroy(mem *mem) {
	/*for (chunk *chnk = mem->chunks + mem->num_chunks - 1; chnk >= mem->chunks; chnk--) {*/
	for (int i = 0; i < sizeof(mem->chunks) / sizeof(chunk_list); i++) {
		chunk_list *chunk_list = mem->chunks + i;
		chunk *chnk = NULL;
		for (int j = 0; j < chunk_list->num_chunks; j++) {
			chnk = chunk_list->chunks[j];
			hval *hv = NULL;
			for (char *pt = chnk->base + chnk->raw_size - chnk->element_size; pt >= chnk->base; pt -= chnk->element_size) {
				hv = (hval *) pt;
				if (hv->type != free_t) {
					hval_destroy(hv, mem, false);
				}

				if (hv->members != NULL) {
					hash_destroy(hv->members, NULL, NULL, NULL, NULL);
					hv->members = NULL;
				}
			}

			ll_destroy(chnk->free_list, NULL, NULL);
			free(chnk);
		}

		if (chunk_list->num_chunks) {
			free(chunk_list->chunks);
		}
	}

	/*free(mem->chunks);*/
	ll_destroy(mem->gc_roots, NULL, NULL);
	free(mem);
}

hval *chunk_get_free(chunk *chnk)
{
	if (chnk->allocated == chnk->count) {
		return NULL;
	}

	hval *hv = NULL;
	if (chnk->free_list->size > 0) {
		hv = (hval *) chnk->free_list->head->data;
		ll_remove_first(chnk->free_list, hv);
		chnk->allocated++;
		return hv;
	}

	if (chnk->free_hint < chnk->base + chnk->raw_size - chnk->element_size && ((hval *)chnk->free_hint)->type == free_t) {
		hval *hv = (hval *) chnk->free_hint;
		chnk->free_hint += chnk->element_size;
		chnk->allocated++;
		return hv;
	}

	return NULL;
}

hval *mem_alloc(size_t size, mem *m) {
	hval *p = mem_alloc_helper(size, m, true);
	GC_LOG("mem_alloc created %p (size %ld)\n", p, size);
	return p;
}

static hval *mem_alloc_helper(size_t size, mem *m, bool run_gc)
{
	unsigned int bucket = 0;
	unsigned int bucket_size = 8;
	while (bucket_size < size) {
		bucket++;
		bucket_size = bucket_size << 1;
	}

	chunk_list *chunk_list = m->chunks + bucket;
	hval *hv = NULL;
	for (int i=chunk_list->num_chunks - 1; i >= 0; i--) {
		chunk *chnk = chunk_list->chunks[i];
		hv = chunk_get_free(chnk);
		if (hv) {
			return hv;
		}
	}

	if (run_gc) {
		gc(m);
		return mem_alloc_helper(size, m, false);
	}

#if GC_REPORTING
	printf("growing heap:\n");
	debug_heap_output(m);
#endif

	if (chunk_list->chunks) {
#if GC_REPORTING
		printf("reallocating existing chunks\n");
#endif
		chunk_list->chunks = realloc(chunk_list->chunks, sizeof(chunk *) * (chunk_list->num_chunks + 1));
	} else {
		chunk_list->chunks = malloc(sizeof(chunk *));
	}

	chunk_list->chunks[chunk_list->num_chunks] = chunk_create(bucket_size, DEFAULT_CHUNK_SIZE);
	hv = chunk_get_free(chunk_list->chunks[chunk_list->num_chunks]);
	if (hv == NULL) {
		exit(2);
	}
	chunk_list->num_chunks++;
#if GC_REPORTING
	printf("grew heap:\n");
	debug_heap_output(m);
#endif
	return hv;
}

void mem_free(mem *m, hval *hv) {
	// TODO consider resetting the free hint?
	GC_LOG("mem_free: %p\n", hv);
	hv->type = free_t;
	/*assert(ll_search_simple(m->gc_roots, hv) == NULL);*/
}

void mem_add_gc_root(mem *m, hval *root) {
	/*hlog("add_gc_root: %p\n", root);*/
	ll_node *node = ll_search_simple(m->gc_roots, root);

	ll_insert_head(m->gc_roots, root);
}

void mem_remove_gc_root(mem *m, hval *root) {
	int index = ll_remove_first(m->gc_roots, root);
	assert(index != -1);
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
	for (int i = 0; i < sizeof(m->chunks) / sizeof(chunk_list); i++) {
		for (int j = 0; j < m->chunks[i].num_chunks; j++) {
			chunk *chunk = m->chunks[i].chunks[j];
			for (char *pt = chunk->base, *max = chunk->base + chunk->raw_size; pt < max; pt += chunk->element_size) {
				((hval *) pt)->reachable = false;
			}
		}
	}

#if GC_REPORTING
	fprintf(stderr, "%d gc roots\n", m->gc_roots->size);
#endif
	ll_node *node = m->gc_roots->head;
	while (node) {
		mark(node->data);
		node = node->next;
	}

	sweep(m);
	m->gc = false;
}

void mark(hval *hv) {
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
		assert(hv->value.list != NULL);
		ll_node *node = hv->value.list->head;
		while (node) {
			mark((hval *) node->data);
			node = node->next;
		}
	}
}

void sweep(mem *mem)
{
	for (int i = 0; i < sizeof(mem->chunks) / sizeof(chunk_list); i++) {
		for (int j=0; j < mem->chunks[i].num_chunks; j++) {
			chunk *chnk = mem->chunks[i].chunks[j];
			GC_LOG("===== SWEEP CHUNK %d %p\n", j, chnk);
			hval *hv = NULL;
			for (char *pt = chnk->base, *max = chnk->base + chnk->raw_size; pt < max; pt += chnk->element_size) {
				hv = (hval *) pt;
				if (hv->type != free_t && !hv->reachable) {
					GC_LOG("freeing unreachable %p\n", hv);
					hval_destroy(hv, mem, false);
					hv->type = free_t;
					chnk->allocated--;
					ll_insert_head(chnk->free_list, hv);
				}
			}
		}
	}
}

void debug_heap_output(mem *mem)
{
	fprintf(stderr, "Heap status:\n");
	int total = 0;
	size_t element_size = 8;
	for (int i = 0; i < sizeof(mem->chunks) / sizeof(chunk_list); i++) {
		fprintf(stderr, " %8ld-byte elements: %d\n", element_size, mem->chunks[i].num_chunks);
		for (int j = 0; j < mem->chunks[i].num_chunks; j++) {
			chunk *chnk = mem->chunks[i].chunks[j];
			printf("  chunk %p: %6d / %6d\n", chnk, chnk->allocated, chnk->count);
		}

		element_size = element_size << 1;
	}
}

