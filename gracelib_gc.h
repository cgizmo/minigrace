#ifndef GRACELIB_GC_H
#define GRACELIB_GC_H

#include <string.h>
#include "gracelib_types.h"

#define FLAG_FRESH 2
#define FLAG_REACHABLE 4
#define FLAG_DEAD 8
#define FLAG_USEROBJ 16
#define FLAG_BLOCK 32
#define FLAG_MUTABLE 64

#define STACK_SIZE stack_size
extern int stack_size;

void set_stack_size(int);
void gc_init(int, int, int);
void gc_destroy();

void gc_alloc_obj(Object);

void gc_pause();
int gc_unpause();

void gc_mark(Object);
void gc_root(Object);
void gc_pause();
int gc_unpause();
int gc_frame_new();
void gc_frame_end(int);
int gc_frame_newslot(Object);
void gc_frame_setslot(int, Object);

void *glmalloc(size_t);
void glfree(void *);

void gc_stats();

#endif
