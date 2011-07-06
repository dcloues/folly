#ifndef MM_H
#define MM_H

#include "linked_list.h"
#include "data.h"

typedef struct {
	linked_list *gc_roots;
	linked_list *heap;
} mem;

mem *mem_create();
void mem_destroy(mem *);

hval *mem_alloc(mem *m);
void mem_free(mem *m, hval *v);
void mem_add_gc_root(mem *m, hval *root);
void mem_remove_gc_root(mem *m, hval *root);

#endif
