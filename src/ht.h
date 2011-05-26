#ifndef HT_H
#define HT_H

#include <stdbool.h>
#include "linked_list.h"

typedef int (*hash_function)(void *);
typedef bool (*key_comparator)(void *, void *);

typedef struct _hash_entry {
	void *key;
	void *value;
	struct _hash_entry *next;
} hash_entry;

typedef struct {
	int buckets;
	int size;
	hash_entry *table;
	hash_function hasher;
	key_comparator key_comparator;
} hash;

/**
 * Creates a new hash using the specified hash function and key comparator.
 */
hash *hash_create(hash_function hash_func, key_comparator comp);

void hash_destroy(hash *h, destructor key_dtor, destructor value_dtor);

/* Adds a key to the hash and returns the value that was previously
 * associated with it, if any.
 */
void *hash_put(hash *hash, void *key, void *value);

/* Removes a key from the hash and returns the value that was
 * associated with it, if any.
 */
void *hash_remove(hash *hash, void *key);

void *hash_get(hash *hash, void *key);

void hash_dump(hash *hash, char *(*value_to_string)(void *));

#endif
