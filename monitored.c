// monitored.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "monitored.h"
#include "queue.h"

static uint8_t monitored_ram_read(memory_t *mem, uint16_t addr)
{
    // Check if the memory structure or context is NULL
    if (!mem || !mem->context)
    {
        fprintf(stderr, "monitored_ram_read: mem or mem->context is NULL.\n");
        return 0xFF;
    }

    // Read the data from the monitored RAM
    monitored_ram_t *ram = (monitored_ram_t *)mem->context;
    return ram->data[addr & (ram->size - 1)];
}

static void monitored_ram_write(memory_t *mem, uint16_t addr, uint8_t data)
{
    // Check if the memory structure or context is NULL
    if (!mem || !mem->context)
    {
        fprintf(stderr, "monitored_ram_write: mem or mem->context is NULL.\n");
        return;
    }

    // Write the data to the monitored RAM
    monitored_ram_t *ram = (monitored_ram_t *)mem->context;
    ram->data[addr & (ram->size - 1)] = data;

    // Handle monitored addresses
    switch (addr)
    {
        case MONITORED_ADDR_OUTPUT_CHAR:
            // Send character to serial output
            queue_enqueue(&ram->cpu->output_queue, data);
            break;

        case MONITORED_ADDR_TEST_STATUS:
            if (data == 0x00)
            {
                const char *message = "6502 FUNCTIONAL TEST PASSED\n";
                for (size_t i = 0; message[i] != '\0'; i++)
                {
                    queue_enqueue(&ram->cpu->output_queue, (uint8_t)message[i]);
                }
            }
            else
            {
                const char *message = "6502 FUNCTIONAL TEST FAILED\n";
                for (size_t i = 0; message[i] != '\0'; i++)
                {
                    queue_enqueue(&ram->cpu->output_queue, (uint8_t)message[i]);
                }
            }
            break;

        case MONITORED_ADDR_ADDITIONAL_STATUS:
            // Display additional test results
            if (data == 0x00)
            {
                const char *message = "ADDITIONAL TEST PASSED\n";
                for (size_t i = 0; message[i] != '\0'; i++)
                {
                    queue_enqueue(&ram->cpu->output_queue, (uint8_t)message[i]);
                }
            }
            else
            {
                char formatted_message[100];
                snprintf(formatted_message, sizeof(formatted_message),
                         "ADDITIONAL TEST FAILED: CODE 0x%02X\n", data);
                for (size_t i = 0; formatted_message[i] != '\0'; i++)
                {
                    queue_enqueue(&ram->cpu->output_queue,
                                  (uint8_t)formatted_message[i]);
                }
            }
            break;

        default:
            // No specific action for unmonitored addresses
            break;
    }
}

memory_t *memory_create_monitored_ram(size_t size, cpu_6502_t *cpu)
{
    // Check if the size is a power of two
    if (size == 0 || (size & (size - 1)) != 0)
    {
        fprintf(stderr, "memory_create_monitored_ram: Invalid size (0x%zx).\n",
                size);
        return NULL;
    }

    // Allocate memory for the monitored RAM context
    monitored_ram_t *ram = malloc(sizeof(monitored_ram_t));

    // Check if the RAM context is NULL
    if (!ram)
    {
        fprintf(stderr, "memory_create_monitored_ram: Failed to allocate "
                        "memory for RAM.\n");
        return NULL;
    }

    // Allocate memory for the data
    ram->data = calloc(size, sizeof(uint8_t));

    // Check if the data is NULL
    if (!ram->data)
    {
        fprintf(stderr, "memory_create_monitored_ram: Failed to allocate "
                        "memory for data.\n");
        free(ram);
        return NULL;
    }

    // Initialize the data to zero
    memset(ram->data, 0, size);
    ram->size = size;
    ram->cpu = cpu;

    // Create the memory structure
    memory_t *memory = malloc(sizeof(memory_t));

    // Check if the memory structure is NULL
    if (!memory)
    {
        fprintf(stderr, "memory_create_monitored_ram: Failed to allocate "
                        "memory for memory structure.\n");
        free(ram->data);
        free(ram);
        return NULL;
    }

    // Initialize the memory structure
    memory->read = monitored_ram_read;
    memory->write = monitored_ram_write;
    memory->context = ram;

    return memory;
}

void memory_destroy_monitored_ram(memory_t *mem)
{
    // Check if the memory structure is NULL
    if (!mem)
    {
        fprintf(stderr, "memory_destroy_monitored_ram: mem is NULL.\n");
        return;
    }

    // Free the monitored RAM context
    monitored_ram_t *ram = (monitored_ram_t *)mem->context;

    // Free the memory data
    if (ram)
    {
        free(ram->data);
        free(ram);
    }

    free(mem);
}
