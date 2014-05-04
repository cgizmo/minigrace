#include <stdio.h>
#include <pthread.h>

#include "gracelib_types.h"
#include "gracelib_threads.h"
#include "gracelib_gc.h"
#include "gracelib.h"

typedef struct AIDObject AIDObject;

/* Structures */
struct AIDObject
{
    OBJECT_HEADER;
    thread_id id;
};

/* Prototypes */
static Object alloc_AID_object(thread_id);
static void init_module_object(void);

Object module_actors_init(void);
Object actors_spawn(Object, int, int *, Object *, int);

/* Globals */
static pthread_once_t once_control = PTHREAD_ONCE_INIT;

ClassData AID;
Object actors_module;

Object module_actors_init()
{
    pthread_once(&once_control, init_module_object);
    return actors_module;
}


/* spawn(block : Block) -> AID.
 * Spawn a new actor in a new thread.
 * Takes a function of type (parent : AID) -> Actor.
 * Returns the resulting actor's AID.
 */
Object actors_spawn(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1)
    {
        gracedie("actors.spawn requires one argument");
    }

    thread_id id = grace_thread_create(argv[0]);
    debug("actors_spawn: made an actor with id %d.\n", id);

    return alloc_AID_object(id);
}

static Object alloc_AID_object(thread_id id)
{
    Object o = alloc_obj(sizeof(AIDObject) - sizeof(struct Object), AID);
    AIDObject *aid = (AIDObject *)o;

    aid->id = id;

    return o;
}

static void init_module_object()
{
    // Initialize global class data
    AID = alloc_class("AID", 0);

    // Initialize module
    ClassData c = alloc_class("Module<actors>", 1);

    add_Method(c, "spawn", &actors_spawn);

    actors_module = alloc_newobj(0, c);
    gc_root(actors_module);
}
