#ifndef OBJECT_H
#define OBJECT_H

#include "data.h"

void mod_object_init(runtime *, native_function_spec **functions, int *function_count);
NATIVE_FUNCTION(mod_object_eachpair);

#endif

