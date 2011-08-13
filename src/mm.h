#ifndef MM_H
#define MM_H

#include "linked_list.h"
#include "data.h"

typedef struct _chunk {
	int size;
	hval *free_hint;
	int allocated;
	hval contents[];
} chunk;

typedef struct {
	linked_list *gc_roots;
	chunk **chunks;
	int num_chunks;
	//hval *heap;
	//int heap_size;
	//int free_hint;
	bool gc;
} mem;


mem *mem_create();
void mem_destroy(mem *);

hval *mem_alloc(mem *m);
void mem_free(mem *m, hval *v);
void mem_add_gc_root(mem *m, hval *root);
void mem_remove_gc_root(mem *m, hval *root);
void gc(mem *m);
void gc_with_temp_root(mem *m, hval *root);
void debug_heap_output(mem *mem);

#endif
