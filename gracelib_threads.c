#include <stdlib.h>
#include <setjmp.h>

#include "gracelib_threads.h"
#include "gracelib_gc.h"

ThreadState thread_alloc(thread_id id) {
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

void thread_pushstackframe(ThreadState state, struct StackFrameObject *f, char *name) {
    state->frame_stack[state->calldepth - 1] = f;
    f->name = name;
}

void thread_pushclosure(ThreadState state, Object c) {
    state->closure_stack[state->calldepth - 1] = (struct ClosureEnvObject *)c;
}
