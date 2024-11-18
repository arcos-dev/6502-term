// memory.h
#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdlib.h>

/* Memory Interface */
typedef struct memory
{
    uint8_t (*read)(struct memory *memory, uint16_t addr);
    void (*write)(struct memory *memory, uint16_t addr, uint8_t data);
    void *context; // Pointer to custom data (e.g., RAM, ROM, IO devices)
} memory_t;

/* RAM Memory Structure */
typedef struct
{
    uint8_t *data;
    size_t size;
} ram_memory_t;

/* Create RAM */
memory_t *memory_create_ram(size_t size);

/* Destroy Memory */
void memory_destroy(memory_t *memory);

#endif /* MEMORY_H */
