#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "gracelib.h"
#include "gracelib_gc.h"
#include "gracelib_types.h"

GRACELIB_PROTOTYPE(math_sin);
GRACELIB_PROTOTYPE(math_cos);
GRACELIB_PROTOTYPE(math_tan);
GRACELIB_PROTOTYPE(math_asin);
GRACELIB_PROTOTYPE(math_acos);
GRACELIB_PROTOTYPE(math_atan);
GRACELIB_PROTOTYPE(math_random);

Object math_module = NULL;

GRACELIB_FUNCTION(math_sin)
{

    return alloc_Float64(sin(*(double *) args[0]->data));
}

GRACELIB_FUNCTION(math_cos)
{

    return alloc_Float64(cos(*(double *) args[0]->data));
}

GRACELIB_FUNCTION(math_tan)
{

    return alloc_Float64(tan(*(double *) args[0]->data));
}

GRACELIB_FUNCTION(math_asin)
{

    return alloc_Float64(asin(*(double *) args[0]->data));
}

GRACELIB_FUNCTION(math_acos)
{

    return alloc_Float64(acos(*(double *) args[0]->data));
}

GRACELIB_FUNCTION(math_atan)
{

    return alloc_Float64(atan(*(double *) args[0]->data));
}

GRACELIB_FUNCTION(math_random)
{

    return alloc_Float64((double)rand() / RAND_MAX);
}


// Create and return a grace object which contains the above functions
Object module_math_init()
{

    if (math_module != NULL)
    {
        return math_module;
    }

    srand(time(NULL));

    ClassData c = alloc_class("Module<math>", 7);

    add_Method(c, "sin", &math_sin);
    add_Method(c, "cos", &math_cos);
    add_Method(c, "tan", &math_tan);
    add_Method(c, "asin", &math_asin);
    add_Method(c, "acos", &math_acos);
    add_Method(c, "atan", &math_atan);
    add_Method(c, "random", &math_random);

    Object o = alloc_newobj(0, c);
    math_module = o;
    gc_root(math_module);

    return o;
}
