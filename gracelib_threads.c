#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <pthread.h>

#include "gracelib_threads.h"
#include "gracelib_msg.h"
#include "gracelib_gc.h"

#include "gracelib.h"

typedef struct ThreadParams ThreadParams;
typedef struct ThreadList ThreadList;

// If id = 0, then "thread" field is not initialised.
struct ThreadList
{
    thread_id id;

    pthread_t thread;
    MessageQueue *msg_queue;
    struct ThreadList *prev;
    struct ThreadList *next;
};

struct ThreadParams
{
    ThreadState *my_thread_state;
    ThreadList *thread_list_entry;
    Object block;
    Object block_arg;
};

static void grace_thread(void *);
static void thread_state_init(ThreadState *);
static void thread_state_destroy(void);
static ThreadState *thread_state_alloc(thread_id, thread_id, GCTransit *);
static ThreadList *thread_list_cons(ThreadList **);
static void thread_list_remove(ThreadList **, ThreadList *);
static void thread_list_free(ThreadList *);

static int active = 0;

static pthread_key_t thread_state;
static volatile thread_id next_thread_id;
static pthread_mutex_t threading_mutex;

static ThreadList *threads;

// Single threaded function! This function is meant to run on Grace initialisation.
// No threads should be created before the call to this function returns.
void threading_init()
{
    if (active)
    {
        gracedie("Tried to initialise active threading subsystem.");
    }

    ThreadState *state;
    ThreadList *t;

    pthread_mutex_init(&threading_mutex, NULL);
    pthread_key_create(&thread_state, NULL);
    next_thread_id = 0;

    // Init initial thread state
    state = thread_state_alloc(0, NO_PARENT, NULL);
    thread_state_init(state);
    next_thread_id++;

    // Add the thread to the list of threads.
    threads = NULL;
    t = thread_list_cons(&threads);
    t->id = 0;
    t->msg_queue = state->msg_queue;

    // Mark the system as active.
    active = 1;
}

// Single threaded function! This function assumes that all threads have terminated
// and that no new threads can be created.
// This can be achieved by doing:
//   threading_deactivate();
//   wait_for_all_threads();
void threading_destroy()
{
    if (active)
    {
        gracedie("Tried to destroy active threading subsystem.");
    }

    // Free memory allocated for the initial thread.
    thread_state_destroy();

    // Remove the key (all threads should have freed their thread state at
    // this point).
    pthread_key_delete(thread_state);

    pthread_mutex_destroy(&threading_mutex);

    // Free threads linked list. It shouldn't be used anymore at this point.
    // In normal execution, "threads" should only contain thread 0 here, but not
    // sure this can be guaranteed if threads crash.
    thread_list_free(threads);
}

void threading_deactivate()
{
    // Always ensure this is the same mutex as the one held by grace_thread_create.
    pthread_mutex_lock(&threading_mutex);
    active = 0;
    pthread_mutex_unlock(&threading_mutex);
}

ThreadState *get_state()
{
    return (ThreadState *)pthread_getspecific(thread_state);
}

// Tries to create a new Grace thread.
// Returns the thread_id of the newly created thread.
// Returns ERR_THREADING_INACTIVE if threading system is deactivated.
thread_id grace_thread_create(Object block, Object block_arg, GCTransit *transit)
{
    thread_id id;
    ThreadParams *params = malloc(sizeof(ThreadParams));

    debug("grace_thread_create: probably creating thread %d.", next_thread_id);

    // Get the thread ID;
    pthread_mutex_lock(&threading_mutex);

    if (!active)
    {
        debug("grace_thread_create: threading inactive, not creating thread %d.", next_thread_id);
        pthread_mutex_unlock(&threading_mutex);
        return ERR_THREADING_INACTIVE;
    }

    ThreadList *t = thread_list_cons(&threads); // requires lock on "threads"
    id = next_thread_id;
    next_thread_id++;

    // Fill up thread parameters
    params->my_thread_state = thread_state_alloc(id, get_state()->id, transit);
    params->thread_list_entry = t;
    params->block = block; // TODO : block_copy(block); ?
    params->block_arg = block_arg; // TODO : copy ?
    t->id = id;
    t->msg_queue = params->my_thread_state->msg_queue;

    // Start thread
    pthread_create(&t->thread, NULL, (void *)&grace_thread, (void *)params);

    // Only unlock the mutex once threads[id] is initialized.
    pthread_mutex_unlock(&threading_mutex);

    debug("grace_thread_create: created thread %d.", id);
    return id;
}

// TODO : make O(1) access.
MessageQueue *get_thread_message_queue(thread_id id)
{
    pthread_mutex_lock(&threading_mutex);

    for (ThreadList *x = threads; x != NULL; x = x->next)
    {
        if (x->id == id)
        {
            pthread_mutex_unlock(&threading_mutex);
            return x->msg_queue;
        }
    }

    pthread_mutex_unlock(&threading_mutex);
    return NULL;
}

void wait_for_all_threads()
{
    if (get_state()->id != 0)
    {
        gracedie("Only initial thread 0 is allowed to wait on all child threads.");
    }

    ThreadList *threads_copy = NULL;

    pthread_mutex_lock(&threading_mutex);

    for (ThreadList *x = threads; x != NULL; x = x->next)
    {
        thread_list_cons(&threads_copy)->thread = x->thread;
    }

    pthread_mutex_unlock(&threading_mutex);

    debug("wait_for_all_threads: joining on threads.\n");

    for (ThreadList *x = threads_copy; x != NULL; x = x->next)
    {
        pthread_join(x->thread, NULL);
    }

    debug("wait_for_all_threads: threads finished.\n");
}

static void grace_thread(void *thread_params_)
{
    ThreadParams *thread_params = (ThreadParams *)thread_params_;

    thread_state_init(thread_params->my_thread_state);
    debug("grace_thread: thread with ID %d and parent %d created.\n", get_state()->id, get_state()->parent_id);

    /* The block is of the form
     * { block_arg -> ... }
     * So 1 part method "apply" on object, 1 argument per part.
     */
    int partcv[1] = { 1 };
    Object params[1] = { thread_params->block_arg };

    callmethod(thread_params->block, "apply", 1, partcv, params);

    debug("grace_thread: thread with ID %d and parent %d finished.\n", get_state()->id, get_state()->parent_id);

    // Thread is finished, remove it from "threads" and destroy internal state.
    pthread_mutex_lock(&threading_mutex);
    thread_list_remove(&threads, thread_params->thread_list_entry);
    pthread_mutex_unlock(&threading_mutex);

    thread_state_destroy();
    free(thread_params_);
    pthread_exit(0);
}

static void thread_state_init(ThreadState *state)
{
    if (pthread_getspecific(thread_state) == NULL)
    {
        pthread_setspecific(thread_state, state);
    }
    else
    {
        die("Thread state initialised more than once.");
    }
}

static void thread_state_destroy()
{
    ThreadState *state = get_state();

    // Release all objects held in transit.
    GCTransit *tmp, *transit = state->transit;

    while (transit != NULL)
    {
        tmp = transit->tnext;
        gc_arrive(transit);
        transit = tmp;
    }

    message_queue_destroy(state->msg_queue);
    gc_stack_destroy(state->gc_stack);
    free(state->frame_stack - 1);
    free(state->closure_stack - 1);
    free(state->callstack);
    free(state->return_stack - 1);
    free(state);

    pthread_setspecific(thread_state, NULL);
}


static ThreadState *thread_state_alloc(thread_id id, thread_id parent_id, GCTransit *transit)
{
    ThreadState *state = malloc(sizeof(ThreadState));

    state->id = id;
    state->parent_id = parent_id;

    state->msg_queue = message_queue_alloc();

    state->gc_stack = gc_stack_create();
    state->transit = transit;

    state->frame_stack = calloc(STACK_SIZE + 1, sizeof(struct StackFrameObject *));
    state->frame_stack++;

    state->closure_stack = calloc(STACK_SIZE + 1, sizeof(struct ClosureEnvObject *));
    state->closure_stack++;

    state->sourceObject = NULL;
    state->calldepth = 0;
    state->return_value = NULL;
    state->callstack = calloc(STACK_SIZE, 256);

    // We need return_stack[-1] to be available.
    state->return_stack = calloc(STACK_SIZE + 1, sizeof(jmp_buf));
    state->return_stack++;

    state->callcount = 0;
    state->tailcount = 0;

    return state;
}

static ThreadList *thread_list_cons(ThreadList **head)
{
    ThreadList *t = malloc(sizeof(ThreadList));
    t->prev = NULL;
    t->next = *head;

    if (*head != NULL)
    {
        (*head)->prev = t;
    }

    (*head) = t;
    return t;
}

// Pre: "t" is a member of the list pointed to by "head".
static void thread_list_remove(ThreadList **head, ThreadList *t)
{
    assert(t != NULL);

    if (t->prev != NULL)
    {
        t->prev->next = t->next;
    }

    if (t->next != NULL)
    {
        t->next->prev = t->prev;
    }

    // t was head of the list.
    if (*head == t)
    {
        *head = t->next;
    }

    free(t);
}

static void thread_list_free(ThreadList *head)
{
    ThreadList *tmp;

    while (head != NULL)
    {
        tmp = head;
        head = head->next;
        free(tmp);
    }
}
