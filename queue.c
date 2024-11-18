#include <pthread.h>
#include "queue.h"

/* Initialize the queue */
void queue_init(queue_t *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;

    pthread_mutex_init(&q->mutex, NULL);
}

/* Enqueue a byte */
bool queue_enqueue(queue_t *q, uint8_t byte)
{
    pthread_mutex_lock(&q->mutex);

    if (q->count == QUEUE_SIZE)
    {
        pthread_mutex_unlock(&q->mutex);
        return false; // Queue full
    }

    q->data[q->tail] = byte;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;

    pthread_mutex_unlock(&q->mutex);

    return true;
}

/* Dequeue a byte */
bool queue_dequeue(queue_t *q, uint8_t *byte)
{
    pthread_mutex_lock(&q->mutex);

    if (q->count == 0)
    {
        pthread_mutex_unlock(&q->mutex);
        return false; // Queue empty
    }

    *byte = q->data[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;

    pthread_mutex_unlock(&q->mutex);

    return true;
}

/* Check if queue is empty */
bool queue_is_empty(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);

    bool empty = (q->count == 0);

    pthread_mutex_unlock(&q->mutex);

    return empty;
}

/* Clear the queue */
void queue_clear(queue_t *q)
{
    pthread_mutex_lock(&q->mutex);

    q->head = 0;
    q->tail = 0;
    q->count = 0;

    pthread_mutex_unlock(&q->mutex);
}

/* Destroy the queue */
void queue_destroy(queue_t *q)
{
    if (!q)
        return;

    pthread_mutex_destroy(&q->mutex);
}
