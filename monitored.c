#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "monitored.h"
#include "queue.h"

/**
 * @brief Reads a byte from the monitored memory.
 *
 * @param mem Pointer to the memory structure.
 * @param addr Address from which to read.
 * @return The byte read from memory or 0xFF if mem is invalid.
 */
static uint8_t monitored_ram_read(memory_t *mem, uint16_t addr)
{
    if (!mem || !mem->context)
    {
        fprintf(stderr, "monitored_ram_read: mem or mem->context is NULL.\n");
        return 0xFF; // Default value or appropriate error handling
    }

    monitored_ram_t *ram = (monitored_ram_t *)mem->context;
    return ram->data[addr & (ram->size - 1)]; // Efficient masking assuming size
                                              // is power of two
}

/**
 * @brief Writes a byte to the monitored memory and handles
 *        specific monitored addresses.
 *
 * @param mem Pointer to the memory structure.
 * @param addr Address where the byte will be written.
 * @param data The byte to write.
 */
static void monitored_ram_write(memory_t *mem, uint16_t addr, uint8_t data)
{
    if (!mem || !mem->context)
    {
        fprintf(stderr, "monitored_ram_write: mem or mem->context is NULL.\n");
        return;
    }

    monitored_ram_t *ram = (monitored_ram_t *)mem->context;
    ram->data[addr & (ram->size - 1)] = data; // Efficient masking

    // Handle writes to specific monitored addresses
    switch (addr)
    {
    case MONITORED_ADDR_OUTPUT_CHAR:
        // Character Output
        queue_enqueue(&ram->cpu->output_queue, data);
        break;

    case MONITORED_ADDR_TEST_STATUS: {
        // Test Status
        const char *message = (data == 0x00)
                                  ? "6502 FUNCTIONAL TEST PASSED\r\n"
                                  : "6502 FUNCTIONAL TEST FAILED\r\n";

        // Enqueue each character of the message
        for (size_t i = 0; message[i] != '\0'; ++i)
        {
            queue_enqueue(&ram->cpu->output_queue, (uint8_t)message[i]);
        }
        break;
    }

    case MONITORED_ADDR_ADDITIONAL_STATUS: {
        // Additional Test Status Reporting
        char formatted_message[100];
        if (data == 0x00)
        {
            snprintf(formatted_message, sizeof(formatted_message),
                     "6502 FUNCTIONAL TEST PASSED (0x%04X)\r\n", addr);
        }
        else
        {
            snprintf(formatted_message, sizeof(formatted_message),
                     "6502 FUNCTIONAL TEST FAILED (0x%04X) with Error Code "
                     "0x%02X\r\n",
                     addr, data);
        }

        // Enqueue each character of the formatted message
        for (size_t i = 0; formatted_message[i] != '\0'; ++i)
        {
            queue_enqueue(&ram->cpu->output_queue,
                          (uint8_t)formatted_message[i]);
        }
        break;
    }

    default:
        // Handle other addresses if necessary
        break;
    }
}

/**
 * @brief Creates a monitored RAM memory structure.
 *
 * @param size Size of the memory in bytes. Must be a power of two.
 * @param cpu Pointer to the CPU structure.
 * @return Pointer to the created memory structure or NULL on failure.
 */
memory_t *memory_create_monitored_ram(size_t size, cpu_6502_t *cpu)
{
    if (size == 0)
    {
        fprintf(stderr, "memory_create_monitored_ram: size is zero.\n");
        return NULL;
    }

    if ((size & (size - 1)) != 0)
    {
        fprintf(stderr,
                "memory_create_monitored_ram: size (0x%zx) "
                "is not a power of two.\n",
                size);
        return NULL;
    }

    monitored_ram_t *ram = malloc(sizeof(monitored_ram_t));

    if (!ram)
    {
        fprintf(stderr, "memory_create_monitored_ram: Failed to allocate "
                        "memory for ram.\n");
        return NULL;
    }

    ram->data = calloc(size, sizeof(uint8_t));

    if (!ram->data)
    {
        fprintf(stderr, "memory_create_monitored_ram: Failed to allocate "
                        "memory for ram->data.\n");
        free(ram);
        return NULL;
    }

    // Initialize memory to zero
    memset(ram->data, 0, size);

    ram->size = size;
    ram->cpu = cpu; // Set pointer to the CPU

    memory_t *memory = malloc(sizeof(memory_t));

    if (!memory)
    {
        fprintf(stderr, "memory_create_monitored_ram: Failed to allocate "
                        "memory for memory.\n");
        free(ram->data);
        free(ram);
        return NULL;
    }

    memory->read = monitored_ram_read;
    memory->write = monitored_ram_write;
    memory->context = ram;

    return memory;
}

/**
 * @brief Destroys the monitored RAM memory structure and frees allocated
 * resources.
 *
 * @param mem Pointer to the memory structure to destroy.
 */
void memory_destroy_monitored_ram(memory_t *mem)
{
    if (!mem)
    {
        fprintf(stderr, "memory_destroy_monitored_ram: mem is NULL.\n");
        return;
    }

    monitored_ram_t *ram = (monitored_ram_t *)mem->context;

    if (ram)
    {
        free(ram->data);
        free(ram);
    }

    free(mem);
}
