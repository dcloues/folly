#ifndef TYPE_H
#define TYPE_H

#include <stdbool.h>
#include "linked_list.h"
#include "data.h"
#include "ht.h"
#include "mm.h"
#include "str.h"
#include "runtime.h"

hstr *FN_ARGS;
hstr *FN_EXPR;
hstr *FN_SELF;
hstr *PARENT;
hstr *STRING;

hval *hval_create(type t, runtime *rt);
void hval_retain(hval *hv);
void hval_release(hval *hv, mem *m);
void hval_destroy(hval *hv, mem *m, bool recursive);
hval *hval_clone(hval *hv, runtime *rt);
hval *hval_string_create(hstr *str, runtime *rt);
hval *hval_number_create(int num, runtime *rt);
hval *hval_list_create(runtime *rt);
hval *hval_hash_create(runtime *rt);
hval *hval_hash_create_child(hval *parent, runtime *rt);
hval *hval_hash_get(hval *hv, hstr *str, runtime *rt);
hval *hval_hash_put(hval *hv, hstr *str, hval *value, mem *m);
hval *hval_hash_put_all(hval *dest, hval *src, mem *m);
hval *hval_native_function_create(native_function fn, runtime *rt);
void hval_list_insert_head(hval *list, hval *val);
void hval_list_insert_tail(hval *list, hval *val);
char *hval_to_string(hval *);
const char *hval_type_string(type t);
int hash_hstr(hstr *);
expression *expr_create(expression_type);
void expr_retain(expression *);
void expr_destroy(expression *, mem *);
void type_init_globals();
void type_destroy_globals();
hval *hval_bind_function(hval *, hval *, mem *);
hval *hval_get_self(hval *);
bool hval_is_callable(hval *test);

#endif

