#ifndef TYPE_H
#define TYPE_H

#include "linked_list.h"
#include "ht.h"

typedef enum { string_t, number_t, hash_t, list_t, quoted_list_t } type;

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

typedef hval *(*native_function)(hval *this, hval *args);

void hval_destroy(hval *hv);
hval *hval_string_create(const char *str, const int len);
hval *hval_number_create(int num);
hval *hval_hash_create(void);
hval *hval_native_function_create(native_function fn);
char *hval_to_string(hval *);
const char *hval_type_string(type t);

#endif

