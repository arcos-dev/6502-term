// event_queue.h
#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

// Define event types
typedef enum
{
    EVENT_SERIAL_INPUT,
    EVENT_SERIAL_OUTPUT,
    EVENT_HELP_MENU,
    EVENT_PROMPT_LOAD_BINARY,
    EVENT_PROMPT_ADJUST_CLOCK,
    EVENT_PROMPT_SET_PC,
    // Add other event types as needed
} EventType;

// Define the Event structure
typedef struct
{
    EventType type;
    union {
        struct
        {
            int ch; // Character input
        } serial_input;
        struct
        {
            uint8_t byte; // Byte output
        } serial_output;
        // Add other event data as needed
    } data;
} Event;

// Node structure for the event queue
typedef struct EventNode
{
    Event event;
    struct EventNode *next;
} EventNode;

// EventQueue structure
typedef struct
{
    EventNode *head;
    EventNode *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} EventQueue;

// Function prototypes
EventQueue *event_queue_create();
bool event_queue_enqueue(EventQueue *queue, Event event);
bool event_queue_dequeue(EventQueue *queue, Event *event);
bool event_queue_is_empty(EventQueue *queue);
void event_queue_destroy(EventQueue *queue);

#endif // EVENT_QUEUE_H
