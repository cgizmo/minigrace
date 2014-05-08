#ifndef GRACELIB_MSG_H
#define GRACELIB_MSG_H

#include <pthread.h>

#include "gracelib_types.h"
#include "gracelib_gc.h"

#define POLL_NO_TIMEOUT -1

typedef struct MessageQueue MessageQueue;
typedef struct MessageQueueElement MessageQueueElement;
typedef enum PollResult PollResult;

struct MessageQueue
{
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;

    MessageQueueElement *head;
    MessageQueueElement *tail;
};

// TODO : immutability semantics for data.
struct MessageQueueElement
{
    Object data;
    GCTransit *data_transit;

    struct MessageQueueElement *next;
};

enum PollResult
{
    POLL_OK, POLL_TIMED_OUT
};

MessageQueue *message_queue_alloc(void);
void message_queue_destroy(MessageQueue *);
void message_queue_post(MessageQueue *, Object, GCTransit *);
PollResult message_queue_poll(MessageQueue *, Object *, GCTransit **, const int);

#endif

