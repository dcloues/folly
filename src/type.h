#ifndef TYPE_H
#define TYPE_H

#include "linked_list.h"
#include "ht.h"

typedef enum { string_t, number_t, hash_t } type;

typedef struct {
	type type;
	union {
		int number;
		struct {
			char *chars;
			int len;
		} str;
		linked_list *list;
		union hash {
			hash *members;
			hash *parent;
		} hash;
	} value;
} hval;

void hval_destroy(hval *hv);
hval *hval_string_create(const char *str, const int len);
hval *hval_number_create(int num);
hval *hval_hash_create(void);
char *hval_to_string(hval *);

#endif

