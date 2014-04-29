#ifndef GRACELIB_TYPES_H
#define GRACELIB_TYPES_H

#include <stdint.h>

#define OBJECT_HEADER int32_t flags; \
                      ClassData class;

typedef struct Object* Object;
typedef struct ClassData* ClassData;

struct MethodType {
    int nparts;
    int *argcv;
    Object *types;
    char **names;
};

typedef struct Method {
    const char *name;
    int32_t flags;
    Object(*func)(Object, int, int*, Object*, int);
    int pos;
    struct MethodType *type;
    const char *definitionModule;
    int definitionLine;
} Method;

struct ClassData {
    OBJECT_HEADER;
    char *name;
    Method *methods;
    int nummethods;
    void (*mark)(void *);
    void (*release)(void *);
    const char *definitionModule;
    int definitionLine;
};

struct Object {
    OBJECT_HEADER;
    char data[];
};

struct StackFrameObject {
    OBJECT_HEADER;
    struct StackFrameObject *parent;
    int size;
    char *name;
    char **names;
    Object slots[];
};

struct ClosureEnvObject {
    OBJECT_HEADER;
    char name[256];
    int size;
    Object frame;
    Object *data[];
};

#endif
