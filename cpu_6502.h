#ifndef CPU_6502_H
#define CPU_6502_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "cpu_clock.h"
#include "queue.h"
#include "bus.h"

/* Constants */
#define INPUT_ADDR  0xD011  // Input address for keyboard
#define OUTPUT_ADDR 0xD012  // Output address for serial output

#define MAX_BREAKPOINTS 16   // Maximum number of breakpoints

/* Status Register Flags */
typedef enum
{
    FLAG_CARRY     = 0,
    FLAG_ZERO      = 1,
    FLAG_INTERRUPT = 2,
    FLAG_DECIMAL   = 3,
    FLAG_BREAK     = 4,
    FLAG_UNUSED    = 5,
    FLAG_OVERFLOW  = 6,
    FLAG_NEGATIVE  = 7
} status_flag_t;

/* CPU Status Codes */
typedef enum
{
    CPU_SUCCESS = 0,
    CPU_ERROR_INVALID_ARGUMENT,
    CPU_ERROR_MEMORY_OVERFLOW,
    CPU_ERROR_INVALID_OPCODE,
    CPU_ERROR_FILE_NOT_FOUND,
    CPU_ERROR_READ_FAILED
} cpu_status_t;

/* Breakpoint Structure */
typedef struct {
    uint16_t addresses[MAX_BREAKPOINTS];
    int count;
} breakpoint_t;

/* CPU Structure Representing the 6502 CPU State */
typedef struct
{
    /* CPU Registers */
    struct
    {
        uint8_t A;   // Accumulator
        uint8_t X;   // X Index Register
        uint8_t Y;   // Y Index Register
        uint16_t PC; // Program Counter
        uint8_t SP;  // Stack Pointer
        uint8_t P;   // Processor Status
    } reg;

    /* Bus reference */
    bus_t *bus;

    /* Clock and timing */
    cpu_clock_t clock;

    // Performance metrics
    double performance_percent; // Simulation performance as a percentage
    double render_time;         // Render time in seconds
    double actual_fps;          // Calculated frames per second

    /* Queues for I/O */
    queue_t input_queue;
    queue_t output_queue;
    pthread_mutex_t input_queue_mutex;
    pthread_mutex_t output_queue_mutex;
    pthread_cond_t output_queue_cond;

    /* Interrupt flags */
    bool IRQ_pending;
    bool NMI_pending;

    /* Mutex for Interrupt Handling */
    pthread_mutex_t interrupt_mutex;

    /* Pause Control */
    bool paused;
    pthread_mutex_t pause_mutex;
    pthread_cond_t pause_cond;

    /* Debug Mode Flag */
    bool debug_mode;
} cpu_6502_t;

/* CPU Interface Functions */
cpu_status_t cpu_init(cpu_6502_t *cpu);
uint8_t cpu_read(cpu_6502_t *cpu, uint16_t addr);
void cpu_write(cpu_6502_t *cpu, uint16_t addr, uint8_t data);
void cpu_destroy(cpu_6502_t *cpu);
void cpu_reset(cpu_6502_t *cpu);
cpu_status_t cpu_load_program(cpu_6502_t *cpu, const char *filename, uint16_t addr);
cpu_status_t cpu_execute_instruction(cpu_6502_t *cpu, breakpoint_t *bp);
void cpu_set_clock_frequency(cpu_6502_t *cpu, double frequency);
void cpu_print_state(const cpu_6502_t *cpu);
void cpu_set_debug_mode(cpu_6502_t *cpu, bool enabled);

/* Interrupt Handling Functions */
void cpu_inject_IRQ(cpu_6502_t *cpu);
void cpu_inject_NMI(cpu_6502_t *cpu);

/* Pause Control Functions */
void cpu_pause(cpu_6502_t *cpu);
void cpu_resume(cpu_6502_t *cpu);

#endif /* CPU_6502_H */
