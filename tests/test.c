#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bus.h"
#include "cpu_6502.h"

// Example program:
// 1.  LDA #$FF       ; Load 0xFF into Accumulator
// 2.  TAY            ; Transfer Accumulator to Y
// 3.  AND #$0F       ; AND Accumulator with 0x0F
// 4.  STA $2000      ; Store Accumulator at address $2000
// 5.  TYA            ; Transfer Y to Accumulator
// 6.  EOR #$F0       ; EOR Accumulator with 0xF0
// 7.  STA $2001      ; Store Accumulator at address $2001
// 8.  LDA #$7F       ; Load 0x7F into Accumulator
// 9.  ADC #$7F       ; Add 0x7F to Accumulator with carry
// 10. STA $2002      ; Store Accumulator at address $2002
// 11. LDA #$50       ; Load 0x50 into Accumulator
// 12. ORA #$0F       ; OR Accumulator with 0x0F
// 13. STA $2003      ; Store Accumulator at address $2003
// 14. SBC #$20       ; Subtract 0x20 from Accumulator with borrow
// 15. DEY            ; Decrement Y Register
// 16. STY $2004      ; Store Y Register at address $2004
// 17. NOP            ; No Operation
// 18. BRK            ; Force Break

uint8_t program[] = {
    0xA9, 0xFF,       // LDA #$FF
    0xA8,             // TAY
    0x29, 0x0F,       // AND #$0F
    0x8D, 0x00, 0x20, // STA $2000
    0x98,             // TYA
    0x49, 0xF0,       // EOR #$F0
    0x8D, 0x01, 0x20, // STA $2001
    0xA9, 0x7F,       // LDA #$7F
    0x69, 0x7F,       // ADC #$7F
    0x8D, 0x02, 0x20, // STA $2002
    0xA9, 0x50,       // LDA #$50
    0x09, 0x0F,       // ORA #$0F
    0x8D, 0x03, 0x20, // STA $2003
    0xE9, 0x20,       // SBC #$20
    0x88,             // DEY
    0x8C, 0x04, 0x20, // STY $2004
    0xA2, 0x10,       // LDX #$10
    0xA0, 0x20,       // LDY #$20
    0x86, 0x00,       // STX $00
    0xEA,             // NOP
    0x00              // BRK
};

/**
 * @brief Loads an example program into CPU memory through the bus
 */
void load_example_program(cpu_6502_t *cpu)
{
    // Starting address for the program
    uint16_t addr = 0x8000;

    // Load program into memory through the bus
    for (uint8_t i = 0; i < sizeof(program); i++)
    {
        cpu_write(cpu, addr + i, program[i]);
    }

    // Set reset vector through the bus
    cpu_write(cpu, 0xFFFC, addr & 0xFF);        // Low byte
    cpu_write(cpu, 0xFFFD, (addr >> 8) & 0xFF); // High byte

    // Set Program Counter to reset vector
    cpu->reg.PC = addr;
}

int main(void)
{
    cpu_6502_t cpu;
    memory_t *ram;

    // Initialize CPU
    cpu_status_t status = cpu_init(&cpu);

    // Check for initialization errors
    if (status != CPU_SUCCESS)
    {
        printf("Failed to initialize CPU. Error code: %d\n", status);
        return EXIT_FAILURE;
    }

    // Create RAM
    ram = memory_create_ram(0x10000); // 64KB RAM

    // Check for memory creation errors
    if (!ram)
    {
        printf("Failed to create RAM\n");
        cpu_destroy(&cpu);
        return EXIT_FAILURE;
    }

    // Map RAM to entire address space
    bus_connect_device(cpu.bus, ram, 0x0000, 0xFFFF);

    // Enable debug mode
    cpu_set_debug_mode(&cpu, true);

    // Load the example program
    load_example_program(&cpu);

    // Print initial CPU state
    printf("Initial CPU State:\n");
    cpu_print_state(&cpu);

    // Execute the program instruction by instruction
    for (uint8_t i = 0; i < sizeof(program); i++)
    {
        status = cpu_execute_instruction(&cpu, NULL);

        // Check for errors
        if (status != CPU_SUCCESS)
        {
            printf("Error executing instruction %d. Status code: %d\n", 
                   i + 1, status);
            break;
        }

        // Print CPU state after each instruction
        cpu_print_state(&cpu);
    }

    // Print final memory state using bus read
    printf("\nFinal Memory State:\n");
    printf("$2000 (AND #$0F)   : 0x%02X\n", cpu_read(&cpu, 0x2000));
    printf("$2001 (EOR #$F0)   : 0x%02X\n", cpu_read(&cpu, 0x2001));
    printf("$2002 (ADC result) : 0x%02X\n", cpu_read(&cpu, 0x2002));
    printf("$2003 (ORA #$0F)   : 0x%02X\n", cpu_read(&cpu, 0x2003));
    printf("$2004 (Final Y)    : 0x%02X\n", cpu_read(&cpu, 0x2004));

    // Cleanup
    cpu_destroy(&cpu);
    memory_destroy(ram);

    return EXIT_SUCCESS;
}
