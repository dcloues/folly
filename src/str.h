#ifndef STR_H
#define STR_H
#include <stdbool.h>

typedef struct {
	char *str;
	int refs;
} hstr;

hstr *hstr_create(char *);
void hstr_init(hstr *, char *);
void hstr_retain(hstr *);
void hstr_release(hstr *);
char *hstr_to_str(hstr *);
bool hstr_comparator(hstr *, hstr *);

#endif
