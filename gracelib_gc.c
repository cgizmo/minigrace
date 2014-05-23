#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <pthread.h>

#include "gracelib_types.h"
#include "gracelib_gc.h"
#include "gracelib_threads.h"

// For die and debug.
#include "gracelib.h"

struct GC_Root
{
    Object object;
    struct GC_Root *next;
};

int expand_living();
int rungc();

// GC mutex for gc_* functions.
pthread_mutex_t gc_mutex;
// Alloc/dealloc mutex for glmalloc/glfree functions.
pthread_mutex_t alloc_mutex;

static struct GC_Root *GC_roots;
static GCTransit *objs_in_transit;
static GCStack *thread_stacks;

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

int freed_since_expansion;

int objectcount = 0;
Object *objects_living;

void set_stack_size(int stack_size_)
{
    stack_size = stack_size_;
}

// Note: GC is hardcoded as disabled in this version of minigrace.
void gc_init(int gc_dofree_, int gc_dowarn_, int gc_period_)
{
    pthread_mutex_init(&gc_mutex, NULL);
    pthread_mutex_init(&alloc_mutex, NULL);

    objects_living_size = 2048;
    objects_living = calloc(sizeof(Object), objects_living_size);

    gc_period = gc_period_;

    gc_dofree = gc_dofree_;
    gc_dowarn = gc_dowarn_;
    gc_enabled = 0; // gc_dofree | gc_dowarn;

    if (gc_dowarn)
    {
        gc_dofree = 0;
    }

    GC_roots = NULL;
    thread_stacks = NULL;
    objs_in_transit = NULL;
}

void gc_destroy()
{
    // TODO : do something with objects in transit and thread stacks ?

    pthread_mutex_destroy(&gc_mutex);
    pthread_mutex_destroy(&alloc_mutex);
}

void gc_alloc_obj(Object o)
{
    if (!gc_enabled)
    {
        return;
    }

    assert(o != NULL);

    /* An object is ignored until the GC is told about it. Ideally, every
     * allocated object should be entered in the GC with the gc_frame_newslot,
     * gc_frame_setslot, gc_root or gc_transit functions.
     * This fixes issues with the GC in threads (e.g. thread 2 preempting thread 1 and
     * running the GC at an unsafe point of thread1. For example, between an allocation
     * and a gc_* call marking the object as reachable.)
     */
    o->flags |= FLAG_IGNORED;

    pthread_mutex_lock(&gc_mutex);

    objectcount++;

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
    if (!gc_enabled)
    {
        return;
    }

    struct GC_Root *r = malloc(sizeof(struct GC_Root));

    r->object = o;

    pthread_mutex_lock(&gc_mutex);

    r->next = GC_roots;

    GC_roots = r;

    if (o != NULL)
    {
        // Clear the ignored flag.
        o->flags &= (0xFFFFFFFF ^ FLAG_IGNORED);
    }

    pthread_mutex_unlock(&gc_mutex);
}

GCTransit *gc_transit(Object o)
{
    if (!gc_enabled)
    {
        return NULL;
    }

    GCTransit *x = malloc(sizeof(GCTransit));
    x->object = o;
    x->prev = NULL;

    pthread_mutex_lock(&gc_mutex);
    x->next = objs_in_transit;

    if (objs_in_transit != NULL)
    {
        objs_in_transit->prev = x;
    }

    objs_in_transit = x;

    if (o != NULL)
    {
        // Clear the ignored flag.
        o->flags &= (0xFFFFFFFF ^ FLAG_IGNORED);
    }

    pthread_mutex_unlock(&gc_mutex);

    return x;
}

void gc_arrive(GCTransit *x)
{
    if (!gc_enabled)
    {
        return;
    }

    assert(x != NULL);

    pthread_mutex_lock(&gc_mutex);

    if (x->prev != NULL)
    {
        x->prev->next = x->next;
    }

    if (x->next != NULL)
    {
        x->next->prev = x->prev;
    }

    // x was head of the list.
    if (objs_in_transit == x)
    {
        objs_in_transit = x->next;
    }

    pthread_mutex_unlock(&gc_mutex);

    free(x);
}

void gc_pause()
{
    if (!gc_enabled)
    {
        return;
    }

    pthread_mutex_lock(&gc_mutex);
    gc_paused++;
    pthread_mutex_unlock(&gc_mutex);
}

int gc_unpause()
{
    if (!gc_enabled)
    {
        return -1;
    }

    pthread_mutex_lock(&gc_mutex);
    gc_paused--;
    pthread_mutex_unlock(&gc_mutex);

    return gc_wouldHaveRun;
}

GCStack *gc_stack_create()
{
    const int stack_size = STACK_SIZE * 1024;
    GCStack *stack = calloc(1, sizeof(GCStack) + (sizeof(Object) * stack_size));

    stack->framepos = 0;
    stack->stack_size = stack_size;
    stack->prev = NULL;

    if (gc_enabled)
    {
        pthread_mutex_lock(&gc_mutex);
        stack->next = thread_stacks;

        if (thread_stacks != NULL)
        {
            thread_stacks->prev = stack;
        }

        thread_stacks = stack;
        pthread_mutex_unlock(&gc_mutex);
    }

    return stack;
}

void gc_stack_destroy(GCStack *stack)
{
    assert(stack != NULL);

    if (gc_enabled)
    {
        pthread_mutex_lock(&gc_mutex);

        if (stack->prev != NULL)
        {
            stack->prev->next = stack->next;
        }

        if (stack->next != NULL)
        {
            stack->next->prev = stack->prev;
        }

        // stack was head of the list.
        if (thread_stacks == stack)
        {
            thread_stacks = stack->next;
        }

        pthread_mutex_unlock(&gc_mutex);
    }

    free(stack);
}

// TODO : change gc_frame* fns so that they don't use a global lock when manipulating
// thread local stacks (will need to change the rungc function to deal with individual
// locks).
int gc_frame_new()
{
    if (!gc_enabled)
    {
        return -1;
    }

    return get_state()->gc_stack->framepos;
}

void gc_frame_end(int pos)
{
    if (!gc_enabled)
    {
        return;
    }

    pthread_mutex_lock(&gc_mutex);
    get_state()->gc_stack->framepos = pos;
    pthread_mutex_unlock(&gc_mutex);
}

int gc_frame_newslot(Object o)
{
    if (!gc_enabled)
    {
        return -1;
    }

    pthread_mutex_lock(&gc_mutex);
    GCStack *gc_stack = get_state()->gc_stack;

    if (gc_stack->framepos == gc_stack->stack_size)
    {
        pthread_mutex_unlock(&gc_mutex);
        die("gc_frame_newslot: gc shadow stack size exceeded on thread %d\n",
            get_state()->id);
        return -1; // Should be unreachable
    }

    int framepos_old = gc_stack->framepos;
    gc_stack->stack[gc_stack->framepos] = o;
    gc_stack->framepos++;

    if (o != NULL)
    {
        // Clear the ignored flag.
        o->flags &= (0xFFFFFFFF ^ FLAG_IGNORED);
    }

    pthread_mutex_unlock(&gc_mutex);
    return framepos_old;
}

void gc_frame_setslot(int slot, Object o)
{
    if (!gc_enabled)
    {
        return;
    }

    pthread_mutex_lock(&gc_mutex);
    get_state()->gc_stack->stack[slot] = o;

    if (o != NULL)
    {
        // Clear the ignored flag.
        o->flags &= (0xFFFFFFFF ^ FLAG_IGNORED);
    }

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

    pthread_mutex_lock(&alloc_mutex);
    heapcurrent -= *i;
    pthread_mutex_unlock(&alloc_mutex);

    memset(i, 0, *i);
    free(i);
    debug("glfree: freed %p (%i)", p, *i);
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

// Pre : gc_mutex is locked and held by the thread calling rungc.
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

    GCTransit *x = objs_in_transit;

    GCStack *s = thread_stacks;

    int freednow = 0;

    while (r != NULL)
    {
        gc_mark(r->object);
        r = r->next;
    }

    // Mark the objects in transit
    while (x != NULL)
    {
        gc_mark(x->object);
        x = x->next;
    }

    while (s != NULL)
    {
        for (i = 0; i < s->framepos; i++)
        {
            gc_mark(s->stack[i]);
        }

        s = s->next;
    }

    int ignored = 0;
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

        if (o->flags & FLAG_IGNORED)
        {
            ignored++;
        }
        else if (o->flags & FLAG_REACHABLE)
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
        fprintf(stderr, "Ignored:     %i\n", ignored);
        fprintf(stderr, "Reachable:   %i\n", reached);
        fprintf(stderr, "Unreachable: %i\n", unreached);
        fprintf(stderr, "Freed:       %i\n", freed);
        fprintf(stderr, "Heap:        %zu\n", heapcurrent);
    }

    return freednow;
}

