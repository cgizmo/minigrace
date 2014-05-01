#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <setjmp.h>
#include <pthread.h>

#include "gracelib_threads.h"
#include "gracelib_gc.h"

// For "die"
#include "gracelib.h"

static int thread_state_init(void);
static void thread_state_destroy(void);
static ThreadState thread_alloc(thread_id);

static pthread_key_t thread_state;
static volatile int num_threads;
static pthread_mutex_t threading_mutex;

void threading_init() {
    pthread_mutex_init(&threading_mutex, NULL);
    pthread_key_create(&thread_state, NULL);
    num_threads = 0;

    // Init initial thread state
    thread_state_init();
}

void threading_destroy() {
    // Free memory allocated for the initial thread.
    thread_state_destroy();

    // Remove the key (all threads should have freed their thread state at
    // this point).
    pthread_key_delete(thread_state);

    pthread_mutex_destroy(&threading_mutex);
}

ThreadState get_state()
{
    return (ThreadState)pthread_getspecific(thread_state);
}

static int thread_state_init()
{
    int tmp;

    if (pthread_getspecific(thread_state) == NULL)
    {
        pthread_setspecific(thread_state, thread_alloc(num_threads));
    }
    else
    {
        die("Thread state initialised more than once.");
    }

    pthread_mutex_lock(&threading_mutex);
    tmp = num_threads;
    num_threads++;
    pthread_mutex_unlock(&threading_mutex);

    return tmp;
}

static void thread_state_destroy()
{
    void *ptr = pthread_getspecific(thread_state);
    free(ptr);
    pthread_setspecific(thread_state, NULL);

    pthread_mutex_lock(&threading_mutex);
    num_threads--;
    pthread_mutex_unlock(&threading_mutex);
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
