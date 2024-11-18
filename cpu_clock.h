#ifndef CPU_CLOCK_H
#define CPU_CLOCK_H

#include <stdint.h>

/* Clock structure */
typedef struct
{
    double frequency;      // Clock frequency in Hz
    uint64_t cycle_count;  // Total cycles executed
    double cycle_duration; // Duration of one cycle in seconds
    double elapsed_time;   // Elapsed time in seconds
    void *platform_data;   // Pointer to platform-specific data
} cpu_clock_t;

/* CPU Clock Configurations */
typedef enum
{
    CPU_CLOCK_APPLE_I,
    CPU_CLOCK_ATARI_2600,
    CPU_CLOCK_COMMODORE_64
} cpu_clock_config_t;

/* Initialize the clock */
int clock_init(cpu_clock_t *clock, double frequency);

/* Destroy the clock */
void clock_destroy(cpu_clock_t *clock);

/* Wait until the next cycle */
void clock_wait_next_cycle(cpu_clock_t *clock);

/* Reset the clock */
void clock_reset(cpu_clock_t *clock);

#endif /* CLOCK_H */
