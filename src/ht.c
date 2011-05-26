#include "ht.h"
#include <stdio.h>
#include <stdlib.h>

unsigned int hash_index_of(hash *hash, void *key);
void *hash_resolve_chain(hash *hash, hash_entry *entry, void *key);
void hash_entry_destroy(hash_entry *entry, destructor key_dtor, destructor value_dtor, bool top_level);

hash *hash_create(hash_function hash_func, key_comparator comp)
{
	hash *h = malloc(sizeof(hash));
	if (h == NULL)
	{
		perror("Unable to allocate memory for hash");
	} else {
		h->buckets = 32;
		h->table = malloc(sizeof(hash_entry) * h->buckets);
		for (int i=0; i < h->buckets; i++) {
			h->table[i].key = NULL;
			h->table[i].value = NULL;
			h->table[i].next = NULL;
		}

		if (h->table == NULL)
		{
			perror("Unable to allocate memory for hash table");
			// don't return a half-ready table
			free(h);
			h = NULL;
		}
		else
		{
			h->size = 0;
			h->hasher = hash_func;
			h->key_comparator = comp;
		}
	}

	return h;
}

void hash_destroy(hash *h, destructor key_dtor, destructor value_dtor)
{
	int i = h->buckets;
	while (--i >= 0)
	{
		hash_entry *entry = h->table + i;
		hash_entry_destroy(entry, key_dtor, value_dtor, true);
	}

	free(h->table);
	free(h);
}

void hash_entry_destroy(hash_entry *entry, destructor key_dtor, destructor value_dtor, bool top_level)
{
	if (!entry || !entry->key)
	{
		return;
	}

	hash_entry_destroy(entry->next, key_dtor, value_dtor, false);
	key_dtor(entry->key);
	printf("hash_entry_destroy key, value: %p %p\n", entry->key, entry->value);
	value_dtor(entry->value);

	if (!top_level)
	{
		free(entry);
	}
}

void *hash_put(hash *hash, void *key, void *value)
{
	unsigned int i = hash_index_of(hash, key);
	hash_entry *candidate = hash->table + i;

	// since the table doesn't point to pointers to hash_entries,
	// we need different logic to chain.
	if (candidate->key)
	{
		while (candidate->next)
		{
			if (hash->key_comparator(candidate->key, key))
			{
				if (candidate->value == value)
				{
					return NULL;
				}
				else
				{
					void *old_value = candidate->value;
					candidate->value = value;
					return old_value;
				}
			}
			candidate = candidate->next;
		}

		hash->size++;
		candidate->next = malloc(sizeof(hash_entry));
		candidate->next->key = key;
		candidate->next->value = value;
		candidate->next->next = NULL;
		return NULL;
	}
	else
	{
		hash->size++;
		candidate->key = key;
		candidate->value = value;
		return NULL;
	}
}

unsigned int hash_index_of(hash *hash, void *key)
{
	int hash_result = hash->hasher(key);
	unsigned int bucket = hash_result % hash->buckets;
	return bucket;
}

void *hash_get(hash *hash, void *key)
{
	int i = hash_index_of(hash, key);
	hash_entry *candidate = hash->table + i;

	while (candidate && candidate->key)
	{
		if (hash->key_comparator(candidate->key, key))
		{
			return candidate->value;
		}
		
		candidate = candidate->next;
	}

	return NULL;
}

void *hash_remove(hash *hash, void *key)
{
	int i = hash_index_of(hash, key);
	hash_entry *ent = hash->table + i;
	if (!ent->key)
	{
		perror("key not found!");
		return NULL;
	}


	void *old_value = NULL;
	if (hash->key_comparator(ent->key, key))
	{
		old_value = ent->value;
		hash_entry *old_next = ent->next;
		if (old_next)
		{
			*ent = *ent->next;
			free(old_next);
			old_next = NULL;
		}
		else
		{
			ent->key = NULL;
			ent->value = NULL;
		}
	}
	else
	{
		while (ent->next)
		{
			if (hash->key_comparator(ent->next->key, key))
			{
				old_value = ent->next->value;
				ent->next = ent->next->next;
				
				break;
			}
		}
		
	}

	hash->size--;
	return old_value;
}

void hash_dump(hash *hash, char *(*value_to_string)(void *))
{
	printf("hash: size=%d with %d buckets\n", hash->size, hash->buckets);
	for (int i=0; i < hash->buckets; i++)
	{
		if (hash->table[i].key)
		{
			printf("%04d:", i);
			hash_entry *entry = hash->table + i;
			do
			{
				char *val_str = value_to_string
						? value_to_string(entry->value)
						: (char *) entry->value;
				printf(" %d: %s", hash->hasher(entry->key), val_str);
				entry = entry->next;
			} while (entry);
			printf("\n");
		}

	}
}

void *hash_resolve_chain(hash *hash, hash_entry *entry, void *key)
{
	return NULL;
}
