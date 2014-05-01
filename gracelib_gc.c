#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>

#include "gracelib_types.h"
#include "gracelib_gc.h"

// For die and debug.
#include "gracelib.h"

int expand_living();
int rungc();

// GC mutex for gc_* functions.
pthread_mutex_t gc_mutex;
// Alloc/dealloc mutex for gcmalloc/glfree functions.
pthread_mutex_t alloc_mutex;

struct GC_Root
{
    Object object;
    struct GC_Root *next;
};

struct GC_Root *GC_roots;

int stack_size = 1024;

int gc_period = 100000;

size_t heapsize;
size_t heapcurrent;
size_t heapmax;

int freedcount = 0;
int objects_living_size = 0;
int objects_living_next = 0;
int objects_living_max = 0;
int gc_dofree = 1;
int gc_dowarn = 0;
int gc_enabled = 1;

int gc_paused;
int gc_wouldHaveRun;

Object *gc_stack;
int gc_framepos = 0;
int gc_stack_size;

int freed_since_expansion;

int objectcount = 0;
Object *objects_living;

void set_stack_size(int stack_size_)
{
    stack_size = stack_size_;
}

void gc_init(int gc_dofree_, int gc_dowarn_, int gc_period_)
{
    pthread_mutex_init(&gc_mutex, NULL);
    pthread_mutex_init(&alloc_mutex, NULL);

    objects_living_size = 2048;
    objects_living = calloc(sizeof(Object), objects_living_size);

    gc_period = gc_period_;

    gc_dofree = gc_dofree_;
    gc_dowarn = gc_dowarn_;
    gc_enabled = gc_dofree | gc_dowarn;

    if (gc_dowarn)
    {
        gc_dofree = 0;
    }
}

void gc_destroy()
{
    pthread_mutex_destroy(&gc_mutex);
    pthread_mutex_destroy(&alloc_mutex);
}

void gc_alloc_obj(Object o)
{
    pthread_mutex_lock(&gc_mutex);

    objectcount++;

    if (gc_enabled)
    {
        if (objects_living_next >= objects_living_size)
        {
            expand_living();
        }

        if (objectcount % gc_period == 0)
        {
            rungc();
        }

        objects_living[objects_living_next++] = o;

        if (objects_living_next > objects_living_max)
        {
            objects_living_max = objects_living_next;
        }
    }

    pthread_mutex_unlock(&gc_mutex);
}

void gc_mark(Object o)
{
    if (o == NULL)
    {
        return;
    }

    if (o->flags & FLAG_REACHABLE)
    {
        return;
    }

    ClassData c = o->class;
    o->flags |= FLAG_REACHABLE;

    if (c && c->mark)
    {
        c->mark(o);
    }
}

void gc_root(Object o)
{
    struct GC_Root *r = malloc(sizeof(struct GC_Root));
    r->object = o;

    pthread_mutex_lock(&gc_mutex);
    r->next = GC_roots;
    GC_roots = r;
    pthread_mutex_unlock(&gc_mutex);
}

void gc_pause()
{
    pthread_mutex_lock(&gc_mutex);
    gc_paused++;
    pthread_mutex_unlock(&gc_mutex);
}

int gc_unpause()
{
    pthread_mutex_lock(&gc_mutex);
    gc_paused--;
    pthread_mutex_unlock(&gc_mutex);

    return gc_wouldHaveRun;
}

// TODO : might require more work for threading. Need to figure out what this functions does.
// Check all gc_frame_* functions.
int gc_frame_new()
{
    pthread_mutex_lock(&gc_mutex);

    if (gc_stack == NULL)
    {
        gc_stack_size = STACK_SIZE * 1024;
        gc_stack = calloc(sizeof(Object), gc_stack_size);
    }

    pthread_mutex_unlock(&gc_mutex);

    return gc_framepos;
}

void gc_frame_end(int pos)
{
    pthread_mutex_lock(&gc_mutex);
    gc_framepos = pos;
    pthread_mutex_unlock(&gc_mutex);
}

int gc_frame_newslot(Object o)
{
    pthread_mutex_lock(&gc_mutex);

    if (gc_framepos == gc_stack_size)
    {
        pthread_mutex_unlock(&gc_mutex);
        die("gc shadow stack size exceeded\n");
        return -1; // Should be unreachable
    }

    int gc_framepos_old = gc_framepos;
    gc_stack[gc_framepos] = o;
    gc_framepos++;

    pthread_mutex_unlock(&gc_mutex);
    return gc_framepos_old;
}

void gc_frame_setslot(int slot, Object o)
{
    pthread_mutex_lock(&gc_mutex);
    gc_stack[slot] = o;
    pthread_mutex_unlock(&gc_mutex);
}

void *glmalloc(size_t s)
{
    pthread_mutex_lock(&alloc_mutex);
    heapsize += s;
    heapcurrent += s;

    if (heapcurrent >= heapmax)
    {
        heapmax = heapcurrent;
    }

    pthread_mutex_unlock(&alloc_mutex);

    // calloc should be thread safe on all modern UNIX systems.
    void *v = calloc(1, s + sizeof(size_t));
    size_t *i = v;
    *i = s;
    return v + sizeof(size_t);
}

void glfree(void *p)
{
    size_t *i = p - sizeof(size_t);
    debug("glfree: freed %p (%i)", p, *i);
    memset(i, 0, *i);
    free(i);

    pthread_mutex_lock(&alloc_mutex);
    heapcurrent -= *i;
    pthread_mutex_unlock(&alloc_mutex);
}

// Pre: run within gracelib_stats()
void gc_stats()
{
    pthread_mutex_lock(&gc_mutex);
    pthread_mutex_lock(&alloc_mutex);
    fprintf(stderr, "Total objects allocated: %i\n", objectcount);
    fprintf(stderr, "Total objects freed:     %i\n", freedcount);
    fprintf(stderr, "Total heap allocated: %zuB\n", heapsize);
    fprintf(stderr, "                      %zuKiB\n", heapsize / 1024);
    fprintf(stderr, "                      %zuMiB\n", heapsize / 1024 / 1024);
    fprintf(stderr, "                      %zuGiB\n", heapsize / 1024 / 1024 / 1024);
    fprintf(stderr, "Peak heap allocated:  %zuB\n", heapmax);
    fprintf(stderr, "                      %zuKiB\n", heapmax / 1024);
    fprintf(stderr, "                      %zuMiB\n", heapmax / 1024 / 1024);
    fprintf(stderr, "                      %zuGiB\n", heapmax / 1024 / 1024 / 1024);
    pthread_mutex_unlock(&alloc_mutex);
    pthread_mutex_unlock(&gc_mutex);
}

/* Non-exported functions */

// Pre : gc_mutex is locked and held by the thread calling expand_living.
int expand_living()
{
    int freed = rungc();
    int mul = 2;

    if (freed_since_expansion * 3 > objects_living_size)
    {
        mul = 1;
    }

    int i;
    Object *before = objects_living;
    objects_living = calloc(sizeof(Object), objects_living_size * mul);
    int j = 0;

    for (i = 0; i < objects_living_size; i++)
    {
        if (before[i] != NULL)
        {
            objects_living[j++] = before[i];
        }
    }

    if (mul == 1)
    {
        freed_since_expansion -= (objects_living_max - j);
    }
    else
    {
        freed_since_expansion = 0;
    }

    objects_living_max = j;
    objects_living_next = j;
    objects_living_size *= mul;
    debug("expanded next %i size %i mul %i freed %i fse %i", j, objects_living_size, mul, freed, freed_since_expansion);
    free(before);
    return 0;
}

// Pre : gc_mutex is locked and held by the thread calling run_gc.
int rungc()
{
    int i;

    if (gc_paused)
    {
        debug("skipping GC; paused %i times", gc_paused);
        gc_wouldHaveRun = 1;
        return 0;
    }

    gc_wouldHaveRun = 0;
    int32_t unreachable = 0xffffffff ^ FLAG_REACHABLE;

    for (i = 0; i < objects_living_max; i++)
    {
        Object o = objects_living[i];

        if (o == NULL)
        {
            continue;
        }

        o->flags = o->flags & unreachable;
    }

    struct GC_Root *r = GC_roots;

    int freednow = 0;

    while (r != NULL)
    {
        gc_mark(r->object);
        r = r->next;
    }

    for (i = 0; i < gc_framepos; i++)
    {
        gc_mark(gc_stack[i]);
    }

    int reached = 0;
    int unreached = 0;
    int freed = 0;
    int doinfo = (getenv("GRACE_GC_INFO") != NULL);

    for (i = 0; i < objects_living_max; i++)
    {
        Object o = objects_living[i];

        if (o == NULL)
        {
            continue;
        }

        if (o->flags & FLAG_REACHABLE)
        {
            reached++;
        }
        else
        {
            if (gc_enabled)
            {
                o->flags |= FLAG_DEAD;
                debug("reaping %p (%s)", o, o->class->name);

                if (gc_dofree)
                {
                    if (o->class->release != NULL)
                    {
                        o->class->release(o);
                    }

                    glfree(o);
                    objects_living[i] = NULL;
                    freednow++;
                }

                freed++;
            }
            else
            {
                o->flags &= 0xfffffffd;
                unreached++;
            }
        }
    }

    debug("reaped %i objects", freednow);
    freedcount += freednow;
    freed_since_expansion += freednow;

    if (doinfo)
    {
        fprintf(stderr, "Reachable:   %i\n", reached);
        fprintf(stderr, "Unreachable: %i\n", unreached);
        fprintf(stderr, "Freed:       %i\n", freed);
        fprintf(stderr, "Heap:        %zu\n", heapcurrent);
    }

    return freednow;
}

