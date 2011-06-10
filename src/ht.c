#include "ht.h"
#include <stdio.h>
#include <stdlib.h>
#include "log.h"

unsigned int hash_index_of(hash *hash, void *key);
void *hash_resolve_chain(hash *hash, hash_entry *entry, void *key);
void hash_entry_destroy(hash_entry *entry, destructor key_dtor, destructor value_dtor, bool top_level);

hash *hash_create(hash_function hash_func, key_comparator comp)
{
	hash *h = malloc(sizeof(hash));
	if (h == NULL)
	{
		hlog("Unable to allocate memory for hash");
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
			hlog("Unable to allocate memory for hash table");
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
	
	void *key = entry->key;
	entry->key = NULL;
	void *value = entry->value;
	entry->value = NULL;
	hash_entry *next = entry->next;

	hash_entry_destroy(next, key_dtor, value_dtor, false);
	hlog("hash_entry_destroy key, value: %p %p\n", key, value);
	if (key != NULL)
	key_dtor(key);
	if (value != NULL)
	value_dtor(value);

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
		hlog("key not found!");
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

void hash_iterate(hash *h, key_value_callback callback, void *ctx)
{
	for (int i = 0; i < h->buckets; i++)
	{
		if (h->table[i].key)
		{
			hash_entry *entry = h->table + i;
			do
			{
				callback(h, entry->key, entry->value, ctx);
				entry = entry->next;
			}  while (entry);
		}
	}
}

hash_iterator *hash_iterator_create(hash *h)
{
	hash_iterator *it = malloc(sizeof(hash_iterator));
	it->hash = h;
	it->bucket = 0;
	it->current_entry = NULL;
	it->current_key = NULL;
	it->current_value = NULL;
	hash_iterator_next(it);
}

void hash_iterator_next(hash_iterator *iter)
{
	iter->current_value = NULL;
	iter->current_key = NULL;
	hash *h = iter->hash;
	if (iter->current_entry)
	{
		iter->current_entry = iter->current_entry->next;
		if (iter->current_entry == NULL)
		{
			iter->bucket++;
		}
	}

	//for (int i = iter->bucket; i < h->buckets; i++) {
	int *i = &iter->bucket;
	for (int *i = &iter->bucket; *i < h->buckets; (*i)++) {
		if (h->table[*i].key)
		{
			iter->current_entry = h->table + *i;
			iter->current_key = iter->current_entry->key;
			iter->current_value = iter->current_entry->value;
			break;
		}
	}
}

void hash_iterator_destroy(hash_iterator *iter)
{
	free(iter);
}

void hash_dump(hash *hash, char *(*key_to_string)(void *), char *(*value_to_string)(void *))
{
	hlog("hash: size=%d with %d buckets\n", hash->size, hash->buckets);
	for (int i=0; i < hash->buckets; i++)
	{
		if (hash->table[i].key)
		{
			hlog("%04d:", i);
			hash_entry *entry = hash->table + i;
			do
			{
				char *key_str = key_to_string
						? key_to_string(entry->key)
						: (char *) entry->key;
				
				hlog("key: %s\n", key_str);
				if (key_to_string)
				{
					free(key_str);
					key_str = NULL;
				}

				char *val_str = value_to_string
						? value_to_string(entry->value)
						: (char *) entry->value;
				hlog(" %d: %s", hash->hasher(entry->key), val_str);
				if (value_to_string)
				{
					free(val_str);
					val_str = NULL;
				}
				entry = entry->next;
			} while (entry);
			hlog("\n");
		}

	}
}

void *hash_resolve_chain(hash *hash, hash_entry *entry, void *key)
{
	return NULL;
}
