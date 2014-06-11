#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h> // TODO : check if this is the same header on Mac/Linux/Cygwin
#include <errno.h>

#include "gracelib_types.h"
#include "gracelib_msg.h"
#include "gracelib_gc.h"

static struct timespec make_absolute_time(const int);
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

void message_queue_post(MessageQueue *msg_queue, Object data, GCTransit *data_transit,
                        const PostBehaviour b)
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
    else if (b == PREPEND)
    {
        elem->next = msg_queue->head;
        msg_queue->head = elem;
    }
    else
    {
        msg_queue->tail->next = elem;
        msg_queue->tail = elem;
    }

    pthread_cond_signal(&msg_queue->queue_cond);
    pthread_mutex_unlock(&msg_queue->queue_lock);
}

PollResult message_queue_poll(MessageQueue *msg_queue,
                              Object *data, GCTransit **data_transit,
                              const int timeout)
{
    pthread_mutex_lock(&msg_queue->queue_lock);

    if (msg_queue->head == NULL && msg_queue->tail == NULL)
    {
        // No timeout, wait to infinity until there is a message in the queue
        if (timeout == POLL_NO_TIMEOUT)
        {
            pthread_cond_wait(&msg_queue->queue_cond, &msg_queue->queue_lock);
        }
        // Otherwise, create the timespec structure and return POLL_TIMED_OUT
        // on time out.
        else
        {
            struct timespec abs_time = make_absolute_time(timeout);
            int rc = pthread_cond_timedwait(&msg_queue->queue_cond, &msg_queue->queue_lock,
                                            &abs_time);

            if (rc == ETIMEDOUT)
            {
                pthread_mutex_unlock(&msg_queue->queue_lock);
                return POLL_TIMED_OUT;
            }
        }
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

    return POLL_OK;
}

static struct timespec make_absolute_time(const int timeout)
{
    struct timeval now;
    struct timespec wake_time;
    unsigned long timeout_nsec = timeout * 1000000UL;

    gettimeofday(&now, NULL);
    wake_time.tv_sec = now.tv_sec;
    wake_time.tv_nsec = now.tv_usec * 1000UL;

    wake_time.tv_sec += (wake_time.tv_nsec + timeout_nsec) / 1000000000UL;
    wake_time.tv_nsec = (wake_time.tv_nsec + timeout_nsec) % 1000000000UL;

    return wake_time;
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
