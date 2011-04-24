#ifndef TYPE_H
#define TYPE_H

#include "linked_list.h"

typedef enum { string_t, number_t, list_t, function_t } type;

typedef struct {
	type type;
} value;

typedef struct {
	value base;
	int number;
} number;

typedef struct {
	value base;
	char *data;
} string;

typedef struct {
	value base;
	linked_list *data;
} list;

typedef struct {
	value base;
} function;

number *number_create(void);
string *string_create(void);
list *list_create(void);

#endif

