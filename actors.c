#include <stdio.h>
#include <pthread.h>

#include "gracelib_types.h"
#include "gracelib_threads.h"
#include "gracelib_msg.h"
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
Object actors_post(Object, int, int *, Object *, int);
Object actors_poll(Object, int, int *, Object *, int);

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
    if (nparams != 1 || argcv[0] != 1)
    {
        gracedie("actors.spawn requires one argument");
    }

    // The parent ID of the new thread is the ID of the current thread.
    Object parent_aid = alloc_AID_object(get_state()->id);
    thread_id id = grace_thread_create(argv[0], parent_aid);
    debug("actors_spawn: made an actor with id %d.\n", id);

    return alloc_AID_object(id);
}

Object actors_post(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 2)
    {
        gracedie("actors.post requires two arguments");
    }

    AIDObject *aid = (AIDObject *)argv[0];
    Object data = argv[1];
    MessageQueue *msg_queue = get_thread_message_queue(aid->id);

    message_queue_post(msg_queue, data);

    return alloc_done();
}

Object actors_poll(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 0)
    {
        gracedie("actors.poll requires no argument");
    }

    MessageQueue *msg_queue = get_state()->msg_queue;

    return (Object)message_queue_poll(msg_queue);
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
    ClassData c = alloc_class("Module<actors>", 3);

    add_Method(c, "spawn", &actors_spawn);
    add_Method(c, "post", &actors_post);
    add_Method(c, "poll", &actors_poll);

    actors_module = alloc_newobj(0, c);
    gc_root(actors_module);
}
