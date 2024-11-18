// memory.c
#include "memory.h"

/* RAM Read Function */
static uint8_t ram_read(memory_t *memory, uint16_t addr)
{
    ram_memory_t *ram = (ram_memory_t *)memory->context;

    if (addr >= ram->size)
        return 0xFF; // Return default value for out-of-bounds

    return ram->data[addr];
}

/* RAM Write Function */
static void ram_write(memory_t *memory, uint16_t addr, uint8_t data)
{
    ram_memory_t *ram = (ram_memory_t *)memory->context;

    if (addr >= ram->size)
        return; // Ignore out-of-bounds writes

    ram->data[addr] = data;
}

/* Create RAM */
memory_t *memory_create_ram(size_t size)
{
    memory_t *memory = malloc(sizeof(memory_t));

    if (!memory)
        return NULL;

    ram_memory_t *ram = malloc(sizeof(ram_memory_t));

    if (!ram)
    {
        free(memory);
        return NULL;
    }

    ram->data = calloc(size, sizeof(uint8_t));

    if (!ram->data)
    {
        free(ram);
        free(memory);
        return NULL;
    }

    ram->size = size;
    memory->read = ram_read;
    memory->write = ram_write;
    memory->context = ram;

    return memory;
}

/* Destroy Memory */
void memory_destroy(memory_t *memory)
{
    if (memory)
    {
        ram_memory_t *ram = (ram_memory_t *)memory->context;

        if (ram)
        {
            free(ram->data);
            free(ram);
        }

        free(memory);
    }
}
