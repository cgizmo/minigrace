#ifndef GRACELIB_THREADS_H
#define GRACELIB_THREADS_H

#include "gracelib_types.h"

typedef struct ThreadState *ThreadState;
typedef int thread_id;

struct ThreadState
{
    thread_id id;

    struct StackFrameObject **frame_stack;
    struct ClosureEnvObject **closure_stack;

    Object sourceObject;

    int calldepth;
    Object return_value;
    char (*callstack)[256];
    jmp_buf *return_stack;

    int callcount;
    int tailcount;
};

ThreadState thread_alloc(int);
void thread_pushstackframe(ThreadState, struct StackFrameObject *, char *);
void thread_pushclosure(ThreadState, Object);

#endif
