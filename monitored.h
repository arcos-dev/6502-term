// monitored.h
#ifndef MONITORED_H
#define MONITORED_H

#include <stddef.h>   // For size_t
#include <stdint.h>   // For uint8_t, uint16_t
#include "memory.h"   // Include the memory interface
#include "cpu_6502.h" // Include the CPU structure

/**
 * @brief Addresses in memory used for monitoring.
 */
#define MONITORED_ADDR_OUTPUT_CHAR       0x6000
#define MONITORED_ADDR_TEST_STATUS       0x6001
#define MONITORED_ADDR_ADDITIONAL_STATUS 0x0002

/**
 * @brief Structure representing monitored RAM context.
 *
 * This structure holds the data for the monitored RAM, including a pointer
 * to the actual memory data, its size, and a reference to the CPU for
 * interacting with the output queue.
 */
typedef struct
{
    uint8_t *data;   /**< Pointer to the memory data array. */
    size_t size;     /**< Size of the memory in bytes. Must be a power of two
                          forefficient masking. */
    cpu_6502_t *cpu; /**< Pointer to the CPU structure for accessing the output
                          queue. */
} monitored_ram_t;

/**
 * @brief Creates a monitored RAM memory structure.
 *
 * This function allocates and initializes a monitored RAM structure that
 * wraps around a block of memory. It sets up custom read and write
 * handlers to monitor specific memory addresses for character output and
 * test status reporting.
 *
 * @param size The size of the memory in bytes. Must be a power of two (e.g.,
 * 0x10000 for 64KB).
 * @param cpu  Pointer to the CPU structure, used for interacting with the
 * output queue.
 *
 * @return Pointer to the created memory structure on success, or NULL on
 * failure.
 */
memory_t *memory_create_monitored_ram(size_t size, cpu_6502_t *cpu);

/**
 * @brief Destroys the monitored RAM memory structure and frees allocated
 * resources.
 *
 * This function releases all memory allocated for the monitored RAM structure,
 * including the data array and the monitored_ram_t context.
 *
 * @param mem Pointer to the memory structure to destroy.
 *
 * @note After calling this function, the memory pointer becomes invalid and
 * should not be used.
 */
void memory_destroy_monitored_ram(memory_t *mem);

#endif /* MONITORED_H */
