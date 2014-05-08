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
static PollResult poll_with_timeout(Object *, const int);
static Object alloc_AID_object(thread_id);
static void init_module_object(void);

Object module_actors_init(void);
Object actors_spawn(Object, int, int *, Object *, int);
Object actors_post(Object, int, int *, Object *, int);
Object actors_poll(Object, int, int *, Object *, int);
Object actors_timedpoll(Object, int, int *, Object *, int);

/* Globals */
static pthread_once_t once_control = PTHREAD_ONCE_INIT;

ClassData AID;
ClassData TimedOut;

Object actors_module;
Object timed_out_singleton;

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
    poll_with_timeout(&data, POLL_NO_TIMEOUT);
    return data;
}

Object actors_timedpoll(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 1)
    {
        gracedie("actors.timedpoll requires one argument");
    }

    Object data;
    PollResult res = poll_with_timeout(&data, integerfromAny(argv[0]));

    if (res == POLL_TIMED_OUT)
    {
        return timed_out_singleton;
    }

    return data;
}

Object actors_TimedOut(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    return (Object)TimedOut; // TODO : Should this be an alloc_Type ?
}

Object actors_AID(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    return (Object)AID; // TODO : Should this be an alloc_Type ?
}

static PollResult poll_with_timeout(Object *result, const int timeout)
{
    GCTransit *data_transit;
    MessageQueue *msg_queue = get_state()->msg_queue;

    PollResult res = message_queue_poll(msg_queue, result, &data_transit, timeout);

    if (res == POLL_OK)
    {
        // Ground the object in the current frame
        gc_frame_newslot(*result);
        gc_arrive(data_transit);
    }

    return res;
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
    // TODO : fix the "fake unique method" hack (it's there b/c of pattern matching,
    // see act4.grace in samples/actors).
    AID = alloc_class("AID", 1);
    add_Method(AID, "__ unique AID", NULL);

    TimedOut = alloc_class("TimedOut", 1);
    add_Method(TimedOut, "__ unique TimedOut", NULL);

    timed_out_singleton = alloc_obj(0, TimedOut);
    gc_root(timed_out_singleton);

    // Initialize module
    ClassData c = alloc_class("Module<actors>", 6);

    add_Method(c, "spawn", &actors_spawn);
    add_Method(c, "post", &actors_post);
    add_Method(c, "poll", &actors_poll);
    add_Method(c, "timedpoll", &actors_timedpoll);
    add_Method(c, "TimedOut", &actors_TimedOut);
    add_Method(c, "AID", &actors_AID);

    actors_module = alloc_newobj(0, c);
    gc_root(actors_module);
}
