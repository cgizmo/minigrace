#ifndef GRACELIB_MSG_H
#define GRACELIB_MSG_H

#include <pthread.h>

typedef struct MessageQueue MessageQueue;
typedef struct MessageQueueElement MessageQueueElement;

struct MessageQueue
{
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;

    MessageQueueElement *head;
    MessageQueueElement *tail;
};

// TODO : immutability semantics for data, and make sure it is not garbage collected
// mid way
struct MessageQueueElement
{
    const void *data;
    struct MessageQueueElement *next;
};

MessageQueue *message_queue_init(void);
void message_queue_destroy(MessageQueue *);
void message_queue_post(MessageQueue *, void *);
const void *const message_queue_poll(MessageQueue *);

#endif

