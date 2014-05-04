#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <pthread.h>

#include "gracelib_threads.h"
#include "gracelib_gc.h"

// For "die"
#include "gracelib.h"

typedef struct ThreadParams ThreadParams;

struct ThreadParams
{
    thread_id my_id;
    thread_id parent_id;
    Object block;
};

static void grace_thread(void *);
static void thread_state_init(thread_id);
static void thread_state_destroy(void);
static ThreadState thread_alloc(thread_id);

static pthread_key_t thread_state;
static volatile thread_id num_threads;
static pthread_mutex_t threading_mutex;

// TODO : make that into a linked list and get rid of the MAX_NUM_THREADS
// Currently, MAX_NUM_THREADS is actually "MAX_NUM_THREAD_CREATIONS".
static pthread_t *threads;

void threading_init()
{
    pthread_mutex_init(&threading_mutex, NULL);
    pthread_key_create(&thread_state, NULL);
    num_threads = 0;

    // Init initial thread state
    thread_state_init(num_threads);
    num_threads++;

    threads = malloc(MAX_NUM_THREADS * sizeof(pthread_t));
    threads[0] = NULL;
}

void threading_destroy()
{
    // Free memory allocated for the initial thread.
    thread_state_destroy();

    // Remove the key (all threads should have freed their thread state at
    // this point).
    pthread_key_delete(thread_state);

    pthread_mutex_destroy(&threading_mutex);
    free(threads);
}

ThreadState get_state()
{
    return (ThreadState)pthread_getspecific(thread_state);
}

// TODO : fix this so that it cannot create threads if the threading system is shut down.
thread_id grace_thread_create(Object block)
{
    thread_id id;
    ThreadParams *params = malloc(sizeof(ThreadParams));

    debug("grace_thread_create: probably creating thread %d", num_threads);

    // Get the thread ID;
    pthread_mutex_lock(&threading_mutex);
    id = num_threads;
    num_threads++;

    // Fill up thread parameters
    params->my_id = id;
    params->parent_id = get_state()->id;
    params->block = block; // TODO : block_copy(block); ?

    // Start thread
    pthread_create(&threads[id], NULL, (void *)&grace_thread, (void *)params);

    // Only unlock the mutex once threads[id] is initialized.
    pthread_mutex_unlock(&threading_mutex);

    //pthread_join(threads[id], NULL);
    debug("grace_thread_create: created thread %d", id);
    return id;
}

void wait_for_all_threads()
{
    if (get_state()->id != 0)
    {
        gracedie("Only initial thread 0 is allowed to wait on all child threads.");
    }

    pthread_mutex_lock(&threading_mutex);

    for (int i = 1; i < num_threads; i++)
    {
        debug("wait_for_all_threads: joining on thread %d.\n", i);
        pthread_join(threads[i], NULL);
        debug("wait_for_all_threads: thread %d finished.\n", i);
    }

    pthread_mutex_unlock(&threading_mutex);
}

static void grace_thread(void *thread_params_)
{
    ThreadParams *thread_params = (ThreadParams *)thread_params_;

    thread_state_init(thread_params->my_id);
    debug("grace_thread: thread with ID %d and parent %d created.\n", get_state()->id, thread_params->parent_id);

    /* The block is of the form
     * { parent_id -> ... }
     * So 1 part method "apply" on object, 1 argument per part.
     */
    int partcv[1] = { 1 };
    Object params[1] = { alloc_Float64(thread_params->parent_id) };

    callmethod(thread_params->block, "apply", 1, partcv, params);

    debug("grace_thread: thread with ID %d and parent %d finished.\n", get_state()->id, thread_params->parent_id);

    // Thread is finished, destroy internal state.
    thread_state_destroy();
    free(thread_params_); // TODO : good idea to do this here ?
    // TODO : when adding a linked list to hold threads, remove the thread from
    // the list around here.
    pthread_exit(0);
}

static void thread_state_init(thread_id id)
{
    if (pthread_getspecific(thread_state) == NULL)
    {
        pthread_setspecific(thread_state, thread_alloc(id));
    }
    else
    {
        die("Thread state initialised more than once.");
    }
}

static void thread_state_destroy()
{
    void *ptr = pthread_getspecific(thread_state);
    free(ptr);
    pthread_setspecific(thread_state, NULL);
}


static ThreadState thread_alloc(thread_id id)
{
    ThreadState state = malloc(sizeof(struct ThreadState));

    state->id = id;

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

