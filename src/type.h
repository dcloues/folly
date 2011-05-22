#ifndef TYPE_H
#define TYPE_H

#include "linked_list.h"

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
	};
} hval;

//hval *number_create(void);
//hval *string_create(void);
//hval *list_create(void);

#endif

