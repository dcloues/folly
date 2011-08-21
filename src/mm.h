#ifndef MM_H
#define MM_H

#include "linked_list.h"
#include "data.h"

typedef struct _chunk {
	int count;
	size_t element_size;
	size_t raw_size;
	char *free_hint;
	int allocated;
	linked_list *free_list;
	char base[];
} chunk;

typedef struct _chunk_list {
	chunk **chunks;
	int num_chunks;
} chunk_list;

typedef struct {
	linked_list *gc_roots;
	chunk_list chunks[8];
	bool gc;
} mem;

mem *mem_create();
void mem_destroy(mem *);

hval *mem_alloc(size_t size, mem *m);
void mem_free(mem *m, hval *v);
void mem_add_gc_root(mem *m, hval *root);
void mem_remove_gc_root(mem *m, hval *root);
void gc(mem *m);
void gc_with_temp_root(mem *m, hval *root);
void debug_heap_output(mem *mem);

#endif
