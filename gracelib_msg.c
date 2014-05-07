#include <stdlib.h>
#include <pthread.h>

#include "gracelib_types.h"
#include "gracelib_msg.h"
#include "gracelib_gc.h"

static void message_queue_free_elements(MessageQueue *);

MessageQueue *message_queue_alloc()
{
    MessageQueue *msg_queue = malloc(sizeof(MessageQueue));

    pthread_mutex_init(&msg_queue->queue_lock, NULL);
    pthread_cond_init(&msg_queue->queue_cond, NULL);
    msg_queue->head = NULL;
    msg_queue->tail = NULL;

    return msg_queue;
}

void message_queue_destroy(MessageQueue *msg_queue)
{
    message_queue_free_elements(msg_queue);

    pthread_mutex_destroy(&msg_queue->queue_lock);
    pthread_cond_destroy(&msg_queue->queue_cond);

    free(msg_queue);
}

void message_queue_post(MessageQueue *msg_queue, Object data, GCTransit *data_transit)
{
    MessageQueueElement *elem = malloc(sizeof(MessageQueueElement));

    elem->data = data;
    elem->data_transit = data_transit;
    elem->next = NULL;

    pthread_mutex_lock(&msg_queue->queue_lock);

    // Empty message queue
    if (msg_queue->head == NULL && msg_queue->tail == NULL)
    {
        msg_queue->head = elem;
        msg_queue->tail = elem;
    }
    else
    {
        msg_queue->tail->next = elem;
        msg_queue->tail = elem;
    }

    pthread_cond_signal(&msg_queue->queue_cond);
    pthread_mutex_unlock(&msg_queue->queue_lock);
}

void message_queue_poll(MessageQueue *msg_queue, Object *data, GCTransit **data_transit)
{
    pthread_mutex_lock(&msg_queue->queue_lock);

    while (msg_queue->head == NULL && msg_queue->tail == NULL)
    {
        pthread_cond_wait(&msg_queue->queue_cond, &msg_queue->queue_lock);
    }

    MessageQueueElement *elem = msg_queue->head;

    if (msg_queue->head == msg_queue->tail)
    {
        msg_queue->head = NULL;
        msg_queue->tail = NULL;
    }
    else
    {
        msg_queue->head = msg_queue->head->next;
    }

    pthread_mutex_unlock(&msg_queue->queue_lock);

    *data = elem->data;
    *data_transit = elem->data_transit;
    free(elem);
}

static void message_queue_free_elements(MessageQueue *msg_queue)
{
    MessageQueueElement *tmp;

    pthread_mutex_lock(&msg_queue->queue_lock);

    while (msg_queue->head != NULL)
    {
        tmp = msg_queue->head;
        msg_queue->head = msg_queue->head->next;
        free(tmp);
    }

    pthread_mutex_unlock(&msg_queue->queue_lock);
}
