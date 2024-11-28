// event_queue.c
#include <stdlib.h>
#include "event_queue.h"

// Create a new event queue
EventQueue *event_queue_create()
{
    EventQueue *queue = malloc(sizeof(EventQueue));

    if (!queue)
        return NULL;

    queue->head = queue->tail = NULL;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);

    return queue;
}

// Enqueue a new event
bool event_queue_enqueue(EventQueue *queue, Event event)
{
    if (!queue)
        return false;

    EventNode *node = malloc(sizeof(EventNode));

    if (!node)
        return false;
    node->event = event;
    node->next = NULL;

    pthread_mutex_lock(&queue->mutex);

    if (queue->tail)
    {
        queue->tail->next = node;
        queue->tail = node;
    }
    else
    {
        queue->head = queue->tail = node;
    }

    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    return true;
}

// Dequeue an event; blocks if the queue is empty
bool event_queue_dequeue(EventQueue *queue, Event *event)
{
    if (!queue || !event)
        return false;

    pthread_mutex_lock(&queue->mutex);

    while (queue->head == NULL)
    {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }

    EventNode *node = queue->head;
    *event = node->event;
    queue->head = node->next;

    if (queue->head == NULL)
    {
        queue->tail = NULL;
    }

    pthread_mutex_unlock(&queue->mutex);
    free(node);

    return true;
}

// Check if the queue is empty
bool event_queue_is_empty(EventQueue *queue)
{
    if (!queue)
        return true;

    pthread_mutex_lock(&queue->mutex);
    bool empty = (queue->head == NULL);
    pthread_mutex_unlock(&queue->mutex);

    return empty;
}

// Destroy the event queue
void event_queue_destroy(EventQueue *queue)
{
    if (!queue)
        return;

    pthread_mutex_lock(&queue->mutex);
    EventNode *current = queue->head;

    while (current)
    {
        EventNode *tmp = current;
        current = current->next;
        free(tmp);
    }

    pthread_mutex_unlock(&queue->mutex);
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);
    free(queue);
}
