#include <stdlib.h>
#include <stdio.h>
#include "linked_list.h"
#include "mm.h"
#include "type.h"

mem *mem_create() {
	mem *m = malloc(sizeof(mem));
	if (m == NULL) {
		perror("Unable to allocate memory for mem");
		exit(1);
	}

	m->gc_roots = ll_create();
	m->heap = ll_create();
	
	return m;
}

void mem_destroy(mem *mem) {
	ll_destroy(mem->gc_roots, (destructor) hval_release);
	ll_destroy(mem->heap, (destructor) hval_release);
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
	ll_remove_first(m->heap, hv);
}

void mem_add_gc_root(mem *m, hval *root) {
	ll_insert_head(m->gc_roots, root);
}

void mem_remove_gc_root(mem *m, hval *root) {
	ll_remove_first(m->heap, root);
}


