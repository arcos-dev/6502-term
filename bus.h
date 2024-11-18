#ifndef BUS_H
#define BUS_H

#include "memory.h"

#define MAX_DEVICES 16 // Adjust as needed

/* Bus Device Structure */
typedef struct
{
    memory_t *device;
    uint16_t start_addr;
    uint16_t end_addr;
} bus_device_t;

/* Bus Structure */
typedef struct bus
{
    bus_device_t devices[MAX_DEVICES];
    int device_count;
} bus_t;

/* Bus Interface Functions */

/**
 * @brief Creates and initializes a new bus.
 *
 * @return Pointer to the created bus or NULL on failure.
 */
bus_t *bus_create(void);

/**
 * @brief Destroys the bus and frees allocated resources.
 *
 * @param bus Pointer to the bus to be destroyed.
 */
void bus_destroy(bus_t *bus);

/**
 * @brief Connects a device to the bus.
 *
 * @param bus Pointer to the bus.
 * @param device Pointer to the device to connect.
 * @param start_addr Starting address of the device's address range.
 * @param end_addr Ending address of the device's address range.
 */
void bus_connect_device(bus_t *bus, memory_t *device, uint16_t start_addr,
                        uint16_t end_addr);

/**
 * @brief Reads a byte from a specific memory address via the bus.
 *
 * @param bus Pointer to the bus.
 * @param addr Memory address to read from.
 * @return The byte read from the specified memory address.
 */
uint8_t bus_read(bus_t *bus, uint16_t addr);

/**
 * @brief Writes a byte to a specific memory address via the bus.
 *
 * @param bus Pointer to the bus.
 * @param addr Memory address to write to.
 * @param data Byte to be written.
 */
void bus_write(bus_t *bus, uint16_t addr, uint8_t data);

#endif /* BUS_H */
