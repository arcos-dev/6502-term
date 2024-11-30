#ifndef MAIN_H
#define MAIN_H

/*
 * main.h
 *
 * Header file for the 6502 CPU Emulator's main interface and core functions.
 *
 * This file contains macro definitions, function prototypes, and external
 * variable declarations used in main.c.
 *
 * Author: Anderson Costa
 * Version: 1.0.0
 * Created: 2024-11-07
 */

/******************************************************************************
 *                            Core Header Includes                            *
 ******************************************************************************/

#include "bus.h"       // Bus system
#include "cpu_6502.h"  // CPU emulation
#include "memory.h"    // Memory management
#include "monitored.h" // Monitored memory
#include "queue.h"     // Input/output queues

/******************************************************************************
 *                             Macro Definitions                              *
 ******************************************************************************/

/* Window dimensions and layout configuration */
#define CPU_WINDOW_HEIGHT 10
#define CPU_WINDOW_WIDTH 80

#define SERIAL_OUTPUT_WINDOW_HEIGHT 10
#define SERIAL_OUTPUT_WINDOW_WIDTH 80

#define SERIAL_INPUT_WINDOW_HEIGHT 5
#define SERIAL_INPUT_WINDOW_WIDTH 80

/* Emulation parameters */
#define DEFAULT_FPS 10
#define INSTRUCTION_HISTORY_SIZE 5
#define INPUT_MAX_LINES 3
#define INPUT_MAX_COLS 78

/* Key Definitions */
#ifndef KEY_ESC
#define KEY_ESC 27
#endif

/******************************************************************************
 *                               External Variables                           *
 ******************************************************************************/

/* UI Windows */
extern WINDOW *cpu_window;
extern WINDOW *serial_output_window;
extern WINDOW *serial_input_window;

/* Emulator Control Flags */
extern volatile bool emulator_running;
extern volatile bool emulator_paused; // Start in paused state
extern volatile bool emulator_exit;
extern volatile bool emulator_reset;
extern volatile bool step_mode;
extern volatile bool step_instruction;
extern volatile bool load_new_binary;
extern volatile bool adjust_clock_speed;
extern volatile bool display_help;
extern volatile bool input_paused;

/* Emulation State */
extern uint16_t instruction_history[INSTRUCTION_HISTORY_SIZE];
extern int history_index;
extern int fps;

/* Current Binary Configuration */
extern char current_binary_path_user[256];
extern uint16_t current_load_address_user;

typedef enum
{
    ALPHANUMERIC = 1,
    NUMERIC,
    HEXADECIMAL,
    FLOATING_POINT
} InputType;

/******************************************************************************
 *                             Opcodes Constants                              *
 ******************************************************************************/

/**
 * @brief Array mapping 6502 opcodes to their corresponding mnemonics.
 *
 * Each index in the array corresponds to an opcode value (0x00 to 0xFF).
 * Unofficial or undefined opcodes are marked as "UNKNOWN".
 */
const char *opcode_mnemonics[256] = {
    [0x00] = "BRK",
    [0x01] = "ORA (Indirect,X)",
    [0x05] = "ORA Zero Page",
    [0x06] = "ASL Zero Page",
    [0x08] = "PHP",
    [0x09] = "ORA Immediate",
    [0x0A] = "ASL Accumulator",
    [0x0D] = "ORA Absolute",
    [0x0E] = "ASL Absolute",
    [0x10] = "BPL Relative",
    [0x11] = "ORA (Indirect),Y",
    [0x15] = "ORA Zero Page,X",
    [0x16] = "ASL Zero Page,X",
    [0x18] = "CLC",
    [0x19] = "ORA Absolute,Y",
    [0x1D] = "ORA Absolute,X",
    [0x1E] = "ASL Absolute,X",
    [0x20] = "JSR Absolute",
    [0x21] = "AND (Indirect,X)",
    [0x24] = "BIT Zero Page",
    [0x25] = "AND Zero Page",
    [0x26] = "ROL Zero Page",
    [0x28] = "PLP",
    [0x29] = "AND Immediate",
    [0x2A] = "ROL Accumulator",
    [0x2C] = "BIT Absolute",
    [0x2D] = "AND Absolute",
    [0x2E] = "ROL Absolute",
    [0x30] = "BMI Relative",
    [0x31] = "AND (Indirect),Y",
    [0x35] = "AND Zero Page,X",
    [0x36] = "ROL Zero Page,X",
    [0x38] = "SEC",
    [0x39] = "AND Absolute,Y",
    [0x3D] = "AND Absolute,X",
    [0x3E] = "ROL Absolute,X",
    [0x40] = "RTI",
    [0x41] = "EOR (Indirect,X)",
    [0x45] = "EOR Zero Page",
    [0x46] = "LSR Zero Page",
    [0x48] = "PHA",
    [0x49] = "EOR Immediate",
    [0x4A] = "LSR Accumulator",
    [0x4C] = "JMP Absolute",
    [0x4D] = "EOR Absolute",
    [0x4E] = "LSR Absolute",
    [0x50] = "BVC Relative",
    [0x51] = "EOR (Indirect),Y",
    [0x55] = "EOR Zero Page,X",
    [0x56] = "LSR Zero Page,X",
    [0x58] = "CLI",
    [0x59] = "EOR Absolute,Y",
    [0x5D] = "EOR Absolute,X",
    [0x5E] = "LSR Absolute,X",
    [0x60] = "RTS",
    [0x61] = "ADC (Indirect,X)",
    [0x65] = "ADC Zero Page",
    [0x66] = "ROR Zero Page",
    [0x68] = "PLA",
    [0x69] = "ADC Immediate",
    [0x6A] = "ROR Accumulator",
    [0x6C] = "JMP Indirect",
    [0x6D] = "ADC Absolute",
    [0x6E] = "ROR Absolute",
    [0x70] = "BVS Relative",
    [0x71] = "ADC (Indirect),Y",
    [0x75] = "ADC Zero Page,X",
    [0x76] = "ROR Zero Page,X",
    [0x78] = "SEI",
    [0x79] = "ADC Absolute,Y",
    [0x7D] = "ADC Absolute,X",
    [0x7E] = "ROR Absolute,X",
    [0x81] = "STA (Indirect,X)",
    [0x84] = "STY Zero Page",
    [0x85] = "STA Zero Page",
    [0x86] = "STX Zero Page",
    [0x88] = "DEY",
    [0x8A] = "TXA",
    [0x8C] = "STY Absolute",
    [0x8D] = "STA Absolute",
    [0x8E] = "STX Absolute",
    [0x90] = "BCC Relative",
    [0x91] = "STA (Indirect),Y",
    [0x94] = "STY Zero Page,X",
    [0x95] = "STA Zero Page,X",
    [0x96] = "STX Zero Page,Y",
    [0x98] = "TYA",
    [0x99] = "STA Absolute,Y",
    [0x9A] = "TXS",
    [0x9D] = "STA Absolute,X",
    [0xA0] = "LDY Immediate",
    [0xA1] = "LDA (Indirect,X)",
    [0xA2] = "LDX Immediate",
    [0xA4] = "LDY Zero Page",
    [0xA5] = "LDA Zero Page",
    [0xA6] = "LDX Zero Page",
    [0xA8] = "TAY",
    [0xA9] = "LDA Immediate",
    [0xAA] = "TAX",
    [0xAC] = "LDY Absolute",
    [0xAD] = "LDA Absolute",
    [0xAE] = "LDX Absolute",
    [0xB0] = "BCS Relative",
    [0xB1] = "LDA (Indirect),Y",
    [0xB4] = "LDY Zero Page,X",
    [0xB5] = "LDA Zero Page,X",
    [0xB6] = "LDX Zero Page,Y",
    [0xB8] = "CLV",
    [0xB9] = "LDA Absolute,Y",
    [0xBA] = "TSX",
    [0xBC] = "LDY Absolute,X",
    [0xBD] = "LDA Absolute,X",
    [0xBE] = "LDX Absolute,Y",
    [0xC0] = "CPY Immediate",
    [0xC1] = "CMP (Indirect,X)",
    [0xC4] = "CPY Zero Page",
    [0xC5] = "CMP Zero Page",
    [0xC6] = "DEC Zero Page",
    [0xC8] = "INY",
    [0xC9] = "CMP Immediate",
    [0xCA] = "DEX",
    [0xCC] = "CPY Absolute",
    [0xCD] = "CMP Absolute",
    [0xCE] = "DEC Absolute",
    [0xD0] = "BNE Relative",
    [0xD1] = "CMP (Indirect),Y",
    [0xD5] = "CMP Zero Page,X",
    [0xD6] = "DEC Zero Page,X",
    [0xD8] = "CLD",
    [0xD9] = "CMP Absolute,Y",
    [0xDD] = "CMP Absolute,X",
    [0xDE] = "DEC Absolute,X",
    [0xE0] = "CPX Immediate",
    [0xE1] = "SBC (Indirect,X)",
    [0xE4] = "CPX Zero Page",
    [0xE5] = "SBC Zero Page",
    [0xE6] = "INC Zero Page",
    [0xE8] = "INX",
    [0xE9] = "SBC Immediate",
    [0xEA] = "NOP",
    [0xEC] = "CPX Absolute",
    [0xED] = "SBC Absolute",
    [0xEE] = "INC Absolute",
    [0xF0] = "BEQ Relative",
    [0xF1] = "SBC (Indirect),Y",
    [0xF5] = "SBC Zero Page,X",
    [0xF6] = "INC Zero Page,X",
    [0xF8] = "SED",
    [0xF9] = "SBC Absolute,Y",
    [0xFD] = "SBC Absolute,X",
    [0xFE] = "INC Absolute,X",
    // Fill the remaining opcodes with "UNKNOWN"
};

/**
 * @brief Retrieve the mnemonic for a given opcode.
 *
 * @param opcode The opcode to translate.
 * @return The mnemonic string corresponding to the opcode.
 */
const char *opcode_to_mnemonic(uint8_t opcode)
{
    extern const char *opcode_mnemonics[256];
    return opcode_mnemonics[opcode] ? opcode_mnemonics[opcode] : "UNKNOWN";
}

/**
 * @brief Sleep for a number of milliseconds to maintain FPS.
 *
 * @param fps The number of frames per second.
 */
void sleep_for_fps(int fps);

/**
 * @brief Create a new window with a box and title.
 *
 * @param height Window height.
 * @param width Window width.
 * @param starty Starting y-coordinate.
 * @param startx Starting x-coordinate.
 * @param title Title of the window.
 * @return WINDOW* Pointer to the new window.
 */
WINDOW *create_window_with_box_and_title(int height, int width, int starty,
                                         int startx, const char *title);

/**
 * @brief Lock the interface mutex.
 */
void lock_interface(void);

/**
 * @brief Unlock the interface mutex.
 */
void unlock_interface(void);

/**
 * @brief General function to handle user input in prompt windows.
 *
 * @param prompt_win Pointer to the prompt window.
 * @param input_buffer Buffer to store user input.
 * @param buffer_size Size of the input buffer.
 * @param input_type Type of input expected (e.g., ALPHANUMERIC, NUMERIC).
 * @return int The last character entered (to detect if ESC was pressed).
 */
int handle_user_input(WINDOW *prompt_win, char *input_buffer,
                      size_t buffer_size, int input_type);

/**
 * @brief Clean up the prompt window and restore the main interface.
 *
 * @param prompt_win Pointer to the prompt window.
 */
void cleanup_prompt_window(WINDOW *prompt_win);

/**
 * @brief General function to display prompt windows.
 *
 * @param prompt_title Title of the prompt window.
 * @param prompt_message Message to display.
 * @param input_type Type of input expected.
 * @param input_buffer Buffer to store user input.
 * @param buffer_size Size of the input buffer.
 * @return int The last character entered (to detect if ESC was pressed).
 */
int display_prompt(const char *prompt_title, const char *prompt_message,
                   int input_type, char *input_buffer, size_t buffer_size);

/**
 * @brief Get the current time in seconds.
 *
 * @return double Current time in seconds.
 */
double get_current_time(void);

/**
 * @brief Display the CPU state in the CPU window.
 *
 * @param cpu Pointer to the CPU structure.
 */
void print_cpu_state(cpu_6502_t *cpu);

/**
 * @brief Thread function for handling serial input and key events.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *serial_input_thread(void *arg);

/**
 * @brief Thread function for handling serial output.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *serial_output_thread(void *arg);

/**
 * @brief Inject an IRQ interrupt into the CPU.
 *
 * @param cpu Pointer to the CPU structure.
 */
void inject_IRQ(cpu_6502_t *cpu);

/**
 * @brief Thread function for injecting IRQ after a delay.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *inject_IRQ_thread(void *arg);

/**
 * @brief Inject an NMI interrupt into the CPU.
 *
 * @param cpu Pointer to the CPU structure.
 */
void inject_NMI(cpu_6502_t *cpu);

/**
 * @brief Thread function for injecting NMI after a delay.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *inject_NMI_thread(void *arg);

/**
 * @brief Thread function for rendering the interface.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *render_interface(void *arg);

/**
 * @brief Main emulation loop.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *emulator_loop(void *arg);

/**
 * @brief Clean up resources and end curses mode.
 *
 * @param cpu Pointer to the CPU structure.
 * @param ram Pointer to the RAM memory.
 * @param bus Pointer to the Bus structure.
 */
void cleanup(cpu_6502_t *cpu, memory_t *ram, bus_t *bus);

/**
 * @brief Update the instruction history buffer.
 *
 * @param pc The current program counter.
 */
void update_instruction_history(uint16_t pc);

/**
 * @brief Load a binary file into the CPU memory, set reset vector, and reset
 * CPU.
 *
 * @param cpu Pointer to the CPU structure.
 * @param path Path to the binary file.
 * @param load_address Address where the binary will be loaded.
 * @return int 0 on success, -1 on failure.
 */
int load_binary(cpu_6502_t *cpu, const char *path, uint16_t load_address);

/**
 * @brief Prompt the user to enter the path of the binary to load.
 *
 * @param cpu Pointer to the CPU structure.
 */
void prompt_load_binary(cpu_6502_t *cpu);

/**
 * @brief Prompt the user to adjust the CPU clock speed.
 *
 * @param cpu Pointer to the CPU structure.
 */
void prompt_adjust_clock(cpu_6502_t *cpu);

/**
 * @brief Prompt the user to set the PC (Program Counter) value.
 *
 * @param cpu Pointer to the CPU structure.
 */
void prompt_set_pc(cpu_6502_t *cpu);

/**
 * @brief Display the help menu with key assignments.
 */
void display_help_menu(void);

/**
 * @brief Retrieve the mnemonic for a given opcode.
 *
 * @param opcode The opcode to translate.
 * @return The mnemonic string corresponding to the opcode.
 */
const char *opcode_to_mnemonic(uint8_t opcode);

#endif // MAIN_H