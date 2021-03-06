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
static Object post(Object, Object, const PostBehaviour);
static PollResult poll_with_timeout(Object *, const int);
static Object alloc_AID_object(thread_id);
static void init_module_object(void);

Object module_actors_prim_init(void);
Object actors_prim_me(Object, int, int *, Object *, int);
Object actors_prim_spawn(Object, int, int *, Object *, int);
Object actors_prim_post(Object, int, int *, Object *, int);
Object actors_prim_priority_post(Object, int, int *, Object *, int);
Object actors_prim_poll(Object, int, int *, Object *, int);
Object actors_prim_timed_poll(Object, int, int *, Object *, int);

/* Globals */
static pthread_once_t once_control = PTHREAD_ONCE_INIT;

ClassData AID;
ClassData TimedOut;

Object actors_prim_module;
Object timed_out_singleton;

Object module_actors_prim_init()
{
    pthread_once(&once_control, init_module_object);
    return actors_prim_module;
}

Object actors_prim_me(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    return alloc_AID_object(get_state()->id);
}

/* spawn(block : Block) -> AID.
 * Spawn a new actor in a new thread.
 * Takes a function of type (parent : AID) -> Actor.
 * Returns the resulting actor's AID.
 */
Object actors_prim_spawn(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 2)
    {
        gracedie("actors_prim.spawn requires two argument");
    }

    // Mark the block and it's argument as "in transit" so the GC does not free it.
    GCTransit *block_transit = gc_transit(argv[0]);
    GCTransit *arg_transit = gc_transit(argv[1]);

    thread_id id = grace_thread_create(argv[0], argv[1], block_transit, arg_transit);
    debug("actors_prim_spawn: made an actor with id %d.\n", id);

    return alloc_AID_object(id);
}

Object actors_prim_post(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 2)
    {
        gracedie("actors_prim.post requires two arguments");
    }

    return post(argv[0], argv[1], APPEND);
}

Object actors_prim_priority_post(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 2)
    {
        gracedie("actors_prim.priority_post requires two arguments");
    }

    return post(argv[0], argv[1], PREPEND);
}

Object actors_prim_poll(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 0)
    {
        gracedie("actors_prim.poll requires no argument");
    }

    Object data;
    poll_with_timeout(&data, POLL_NO_TIMEOUT);
    return data;
}

Object actors_prim_timed_poll(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (nparams != 1 || argcv[0] != 1)
    {
        gracedie("actors_prim.timed_poll requires one argument");
    }

    Object data;
    PollResult res = poll_with_timeout(&data, integerfromAny(argv[0]));

    if (res == POLL_TIMED_OUT)
    {
        return timed_out_singleton;
    }

    return data;
}

Object actors_prim_TimedOut(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    return (Object)TimedOut; // TODO : Should this be an alloc_Type ?
}

Object actors_prim_AID(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    return (Object)AID; // TODO : Should this be an alloc_Type ?
}

Object AID_asString(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    thread_id id = ((AIDObject *)self)->id;
    char str[100];

    snprintf(str, 100, "c_AID<%d>", id);
    return alloc_String(str);
}

Object AID_Equals(Object self, int nparams, int *argcv, Object *argv, int flags)
{
    if (argv[0]->class != AID)
    {
        return alloc_Boolean(0);
    }

    return alloc_Boolean(((AIDObject *)argv[0])->id == ((AIDObject *)self)->id);
}

static Object post(Object dest, Object data, const PostBehaviour b)
{
    AIDObject *aid = (AIDObject *)dest;
    GCTransit *data_transit = gc_transit(data);
    MessageQueue *msg_queue = get_thread_message_queue(aid->id);

    if (msg_queue == NULL)
    {
        gracelib_raise_exception("cannot post, no actor at given AID");
    }

    message_queue_post(msg_queue, data, data_transit, b);

    return alloc_done();
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
    // see act4.grace in samples/actors_prim).
    AID = alloc_class("AID", 5);
    add_Method(AID, "__ unique AID", NULL);
    add_Method(AID, "==", &AID_Equals);
    add_Method(AID, "!=", &Object_NotEquals);
    add_Method(AID, "asString", &AID_asString);
    add_Method(AID, "copy", &immutable_primitive_copy);

    TimedOut = alloc_class("TimedOut", 1);
    add_Method(TimedOut, "__ unique TimedOut", NULL);

    timed_out_singleton = alloc_obj(0, TimedOut);
    gc_root(timed_out_singleton);

    // Initialize module
    ClassData c = alloc_class("Module<actors_prim>", 8);

    add_Method(c, "me", &actors_prim_me);
    add_Method(c, "spawn", &actors_prim_spawn);
    add_Method(c, "post", &actors_prim_post);
    add_Method(c, "priority_post", &actors_prim_priority_post);
    add_Method(c, "poll", &actors_prim_poll);
    add_Method(c, "timed_poll", &actors_prim_timed_poll);
    add_Method(c, "TimedOut", &actors_prim_TimedOut);
    add_Method(c, "AID", &actors_prim_AID);

    actors_prim_module = alloc_newobj(0, c);
    gc_root(actors_prim_module);
}
