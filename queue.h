#ifndef QUEUE_H
#define QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#define QUEUE_SIZE 1024

typedef struct
{
    uint8_t data[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t mutex;
} queue_t;

/* Initialize the queue */
void queue_init(queue_t *q);

/* Enqueue a byte */
bool queue_enqueue(queue_t *q, uint8_t byte);

/* Dequeue a byte */
bool queue_dequeue(queue_t *q, uint8_t *byte);

/* Check if queue is empty */
bool queue_is_empty(queue_t *q);

/* Clear the queue */
void queue_clear(queue_t *q);

/* Destroy the queue */
void queue_destroy(queue_t *q);

#endif /* QUEUE_H */
