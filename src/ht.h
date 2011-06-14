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

typedef struct {
	hash *hash;
	int bucket;
	hash_entry *current_entry;
	void *current_key;
	void *current_value;
} hash_iterator;

typedef void (*key_value_callback)(hash *, void *, void *, void *);

/**
 * Creates a new hash using the specified hash function and key comparator.
 */
hash *hash_create(hash_function hash_func, key_comparator comp);

void hash_destroy(hash *h, destructor key_dtor, destructor value_dtor);

/* Adds a key to the hash and returns the value that was previously
 * associated with it, if any.
 */
void *hash_put(hash *hash, void *key, void *value);

/**
 * Adds all values from src to dest, invoking overwrite_dtor
 * to clean up any values that are overwritten.
 */
void hash_put_all(hash *dest, hash *src, destructor overwrite_dtor);

/* Removes a key from the hash and returns the value that was
 * associated with it, if any.
 */
void *hash_remove(hash *hash, void *key);

void *hash_get(hash *hash, void *key);

void hash_dump(hash *hash, char *(key_to_string)(void *), char *(*value_to_string)(void *));

//void hash_iterate(hash *h, void (*callback)(hash *, void *, void *, void *), void *ctx);
void hash_iterate(hash *h, key_value_callback callback, void *ctx);
void hash_iterator_next(hash_iterator *);
void hash_iterator_destroy(hash_iterator *);
hash_iterator *hash_iterator_create(hash *);

#endif
