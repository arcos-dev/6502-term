#include <stdio.h>
#include <stdlib.h>
#include "bus.h"

/* Creates and initializes a new bus */
bus_t *bus_create(void)
{
    bus_t *bus = malloc(sizeof(bus_t));

    if (!bus)
    {
        fprintf(stderr, "bus_create: Failed to allocate memory for bus.\n");
        return NULL;
    }

    bus->device_count = 0; // Initialize device count
    return bus;
}

/* Destroys the bus and frees allocated resources */
void bus_destroy(bus_t *bus)
{
    if (bus)
    {
        // Note: The bus does not own the devices; they should be managed
        // externally
        free(bus);
    }
}

/* Connects a device to the bus */
void bus_connect_device(bus_t *bus, memory_t *device, uint16_t start_addr,
                        uint16_t end_addr)
{
    if (!bus || !device)
    {
        fprintf(stderr, "bus_connect_device: Invalid bus or device pointer.\n");
        return;
    }

    if (bus->device_count >= MAX_DEVICES)
    {
        fprintf(stderr,
                "bus_connect_device: Maximum number of devices reached.\n");
        return;
    }

    bus_device_t *dev = &bus->devices[bus->device_count++];
    dev->device = device;
    dev->start_addr = start_addr;
    dev->end_addr = end_addr;
}

/* Reads a byte from a specific memory address via the bus */
uint8_t bus_read(bus_t *bus, uint16_t addr)
{
    for (int i = 0; i < bus->device_count; ++i)
    {
        bus_device_t *dev = &bus->devices[i];

        if (addr >= dev->start_addr && addr <= dev->end_addr)
        {
            return dev->device->read(dev->device, addr);
        }
    }

    // If no device handles the address, return 0xFF
    return 0xFF;
}

/* Writes a byte to a specific memory address via the bus */
void bus_write(bus_t *bus, uint16_t addr, uint8_t data)
{
    for (int i = 0; i < bus->device_count; ++i)
    {
        bus_device_t *dev = &bus->devices[i];

        if (addr >= dev->start_addr && addr <= dev->end_addr)
        {
            dev->device->write(dev->device, addr, data);
            return;
        }
    }

    // If no device handles the address, do nothing or handle as needed
}
