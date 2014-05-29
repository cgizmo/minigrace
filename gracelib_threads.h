#ifndef GRACELIB_THREADS_H
#define GRACELIB_THREADS_H

#include <stdio.h>
#include <setjmp.h>

#include "gracelib_gc.h"
#include "gracelib_msg.h"
#include "gracelib_types.h"

#define NO_PARENT -0x01

#define ERR_THREADING_INACTIVE -0x10

typedef struct ThreadState ThreadState;
typedef int thread_id;

struct ThreadState
{
    thread_id id;
    thread_id parent_id;

    MessageQueue *msg_queue;

    GCStack *gc_stack;

    struct StackFrameObject **frame_stack;
    struct ClosureEnvObject **closure_stack;

    Object sourceObject;

    int calldepth;
    Object return_value;
    char (*callstack)[256];
    jmp_buf *return_stack;

    int callcount;
    int tailcount;

    // Exception handling
    jmp_buf error_jump;
    Object currentException;

    jmp_buf *exceptionHandler_stack;
    Object *finally_stack;

    int error_jump_set;
    int exceptionHandlerDepth;
};

void threading_init(void);
void threading_destroy(void);
void threading_deactivate(void);
ThreadState *get_state(void);

// For some reason this function cannot be called just "thread_create", so
// calling it "grace_thread_create" instead.
thread_id grace_thread_create(Object, Object, GCTransit *, GCTransit *);
void wait_for_all_threads(void);
MessageQueue *get_thread_message_queue(thread_id);

#endif
