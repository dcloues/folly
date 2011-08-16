#ifndef STR_H
#define STR_H
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
	int refs;
	bool hash_calculated;
	int hash;
	char str[];
} hstr;

hstr *hstr_create(char *);
hstr *hstr_create_len(char *, size_t);
void hstr_init(hstr *, char *, size_t);
void hstr_retain(hstr *);
void hstr_release(hstr *);
char *hstr_to_str(hstr *);
bool hstr_comparator(hstr *, hstr *);

#endif
