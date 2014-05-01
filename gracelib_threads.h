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

void threading_init(void);
void threading_destroy(void);
ThreadState get_state(void);

#endif
