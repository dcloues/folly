#ifndef TYPE_H
#define TYPE_H

#include "linked_list.h"
#include "ht.h"
#include "str.h"

typedef enum { string_t, number_t, hash_t, list_t, quoted_list_t, native_function_t } type;

typedef struct hval hval;

typedef hval *(*native_function)(hval *this, hval *args);

struct hval {
	type type;
	union {
		int number;
		hstr *str;
		linked_list *list;
		union hash {
			hash *members;
			hash *parent;
		} hash;
		native_function native_fn;
	} value;
};

void hval_destroy(hval *hv);
hval *hval_string_create(hstr *str);
hval *hval_number_create(int num);
hval *hval_list_create();
hval *hval_hash_create(void);
hval *hval_hash_create_child(hval *parent);
hval *hval_hash_get(hval *hv, hstr *str);
hval *hval_hash_put(hval *hv, hstr *str, hval *value);
hval *hval_native_function_create(native_function fn);
char *hval_to_string(hval *);
const char *hval_type_string(type t);

#endif

