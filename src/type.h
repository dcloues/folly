#ifndef TYPE_H
#define TYPE_H

#include "linked_list.h"
#include "ht.h"

typedef enum { string_t, number_t, list_t, function_t } type;

typedef struct {
	type type;
	union {
		int number;
		union {
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

//hval *number_create(void);
//hval *string_create(void);
//hval *list_create(void);

hval *hval_string_create(char *str);
hval *hval_number_create(int num);
hval *hval_hash_create(void);
char *hval_to_string(hval *);

#endif

