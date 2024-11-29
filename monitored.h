#ifndef MONITORED_H
#define MONITORED_H

#include <stddef.h>   // For size_t
#include <stdint.h>   // For uint8_t, uint16_t
#include "cpu_6502.h" // Include the CPU structure
#include "memory.h"   // Include the memory interface

/**
 * @brief Addresses in memory used for monitoring.
 */
#define MONITORED_ADDR_OUTPUT_CHAR       0x6000
#define MONITORED_ADDR_TEST_STATUS       0x6001
#define MONITORED_ADDR_ADDITIONAL_STATUS 0x6002

/**
 * @brief Structure representing monitored RAM context.
 */
typedef struct
{
    uint8_t *data;   /**< Pointer to the memory data array. */
    size_t size;     /**< Size of the memory in bytes. */
    cpu_6502_t *cpu; /**< Pointer to the CPU structure for interacting with the
                        output queue. */
} monitored_ram_t;

/**
 * @brief Creates a monitored RAM memory structure.
 *
 * @param size The size of the memory in bytes. Must be a power of two.
 * @param cpu  Pointer to the CPU structure.
 * @return Pointer to the created memory structure on success, or NULL on
 * failure.
 */
memory_t *memory_create_monitored_ram(size_t size, cpu_6502_t *cpu);

/**
 * @brief Destroys the monitored RAM memory structure and frees allocated
 * resources.
 *
 * @param mem Pointer to the memory structure to destroy.
 */
void memory_destroy_monitored_ram(memory_t *mem);

#endif /* MONITORED_H */
