#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

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

    // Mark the block as "in transit" so the GC does not free it.
    GCTransit *block_transit = gc_transit(argv[0]);

    // The parent ID of the new thread is the ID of the current thread.
    Object parent_aid = alloc_AID_object(get_state()->id);
    GCTransit *aid_transit = gc_transit(parent_aid);

    thread_id id = grace_thread_create(argv[0], parent_aid, block_transit, aid_transit);
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
    GCTransit *data_transit = gc_transit(data);
    MessageQueue *msg_queue = get_thread_message_queue(aid->id);

    message_queue_post(msg_queue, data, data_transit);

    return alloc_done();
}

Object actors_poll(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 0)
    {
        gracedie("actors.poll requires no argument");
    }

    Object data;
    GCTransit *data_transit;
    MessageQueue *msg_queue = get_state()->msg_queue;

    message_queue_poll(msg_queue, &data, &data_transit);

    // Ground the object in the current frame
    gc_frame_newslot(data);
    gc_arrive(data_transit);

    return data;
}

Object actors_sleep(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    sleep(1);
    return alloc_done();
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
    ClassData c = alloc_class("Module<actors>", 4);

    add_Method(c, "spawn", &actors_spawn);
    add_Method(c, "post", &actors_post);
    add_Method(c, "poll", &actors_poll);
    add_Method(c, "sleep", &actors_sleep);

    actors_module = alloc_newobj(0, c);
    gc_root(actors_module);
}
