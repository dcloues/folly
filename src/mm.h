#ifndef MM_H
#define MM_H

#include "linked_list.h"
#include "data.h"

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
