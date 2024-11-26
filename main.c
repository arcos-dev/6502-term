/*
 * main.c
 *
 * 6502 CPU Emulator - Main Interface and Core Functions
 *
 * Handles the initialization, user interface, threading, and core emulation
 * loop for the 6502 CPU emulator.
 *
 * Author: Anderson Costa
 * Version: 1.0.0
 * Created: 2024-11-07
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h> // For multithreading
#include <curses.h>  // For UI
#include <unistd.h>  // For usleep
#include <ctype.h>   // For isprint

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "main.h" // Main definitions

/******************************************************************************
 *                                Global State                                *
 ******************************************************************************/

/* UI Windows */
WINDOW *cpu_window;
WINDOW *serial_output_window;
WINDOW *serial_input_window;

/* Synchronization */
pthread_mutex_t lock;

/* Emulator Control Flags */
volatile bool emulator_running = true;
volatile bool emulator_paused = true; // Start in paused state
volatile bool emulator_exit = false;
volatile bool emulator_reset = false;
volatile bool step_mode = false;
volatile bool step_instruction = false;
volatile bool load_new_binary = false;
volatile bool adjust_clock_speed = false;
volatile bool display_help = false;
volatile bool input_paused = false;

/* Emulation State */
uint16_t instruction_history[INSTRUCTION_HISTORY_SIZE];
int history_index = 0;
int fps = DEFAULT_FPS;

/* Current Binary Configuration */
char current_binary_path_user[256] = "roms/hello.bin";

/* Set the load address for the functional test binary (0x0400 or 0x0600) */
uint16_t current_load_address_user = 0xC000;

#ifdef _WIN32
double get_current_time()
{
    LARGE_INTEGER frequency, current_time;
    static LARGE_INTEGER start_time;
    static bool initialized = false;

    if (!initialized)
    {
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start_time);
        initialized = true;
    }

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&current_time);

    return (double)(current_time.QuadPart - start_time.QuadPart) /
           (double)frequency.QuadPart;
}
#else
double get_current_time()
{
    struct timespec current_time;
    static struct timespec start_time;
    static bool initialized = false;

    if (!initialized)
    {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        initialized = true;
    }

    clock_gettime(CLOCK_MONOTONIC, &current_time);

    double elapsed_sec = (double)(current_time.tv_sec - start_time.tv_sec);
    double elapsed_nsec =
        (double)(current_time.tv_nsec - start_time.tv_nsec) / 1e9;

    return elapsed_sec + elapsed_nsec;
}
#endif

/******************************************************************************
 *                         Main Program Entry Point                           *
 ******************************************************************************/

int main()
{
    // Initialize curses mode for UI
    initscr();

    // Enable color functionality
    start_color();

    // Initialize color pairs
    init_pair(1, COLOR_WHITE, COLOR_BLACK); // Labels in light gray
    init_pair(2, COLOR_CYAN, COLOR_BLACK);  // Values in cyan
    init_pair(3, COLOR_WHITE, COLOR_BLACK); // Function key labels in white
    init_pair(4, COLOR_WHITE, COLOR_BLACK); // Function key descriptions

    cbreak();                               // Disable line buffering
    noecho();                               // Do not echo input
    keypad(stdscr, TRUE);                   // Enable function keys
    curs_set(0);                            // Hide the cursor

    // Setup windows for CPU, Serial Output, and Serial Input
    cpu_window = newwin(CPU_WINDOW_HEIGHT, CPU_WINDOW_WIDTH, 0, 0);
    serial_output_window =
        newwin(SERIAL_OUTPUT_WINDOW_HEIGHT, SERIAL_OUTPUT_WINDOW_WIDTH,
               CPU_WINDOW_HEIGHT, 0);
    serial_input_window =
        newwin(SERIAL_INPUT_WINDOW_HEIGHT, SERIAL_INPUT_WINDOW_WIDTH,
               CPU_WINDOW_HEIGHT + SERIAL_OUTPUT_WINDOW_HEIGHT, 0);

    // Configure input timeout for the serial input window
    wtimeout(serial_input_window, 100);
    keypad(serial_input_window, TRUE); // Enable function keys

    // Draw borders and titles for each window
    box(cpu_window, 0, 0);
    box(serial_output_window, 0, 0);
    box(serial_input_window, 0, 0);
    mvwprintw(cpu_window, 0, 2, " CPU State ");
    mvwprintw(serial_output_window, 0, 2, " Serial Output ");
    mvwprintw(serial_input_window, 0, 2, " Serial Input ");
    wrefresh(cpu_window);
    wrefresh(serial_output_window);
    wrefresh(serial_input_window);

    // Initialize synchronization mutex
    pthread_mutex_init(&lock, NULL);

    // Create the Bus
    bus_t *bus = bus_create();

    if (!bus)
    {
        fprintf(stderr, "Failed to create bus.\n");
        endwin();
        return EXIT_FAILURE;
    }

    // Create and Initialize the CPU
    cpu_6502_t *cpu = malloc(sizeof(cpu_6502_t));

    if (!cpu)
    {
        fprintf(stderr, "Failed to allocate memory for CPU.\n");
        bus_destroy(bus);
        endwin();
        return EXIT_FAILURE;
    }

    // Initialize the CPU
    if (cpu_init(cpu) != CPU_SUCCESS)
    {
        fprintf(stderr, "Failed to initialize CPU.\n");
        free(cpu);
        bus_destroy(bus);
        endwin();
        return EXIT_FAILURE;
    }

    // Initialize CPU queues
    queue_init(&cpu->input_queue);
    queue_init(&cpu->output_queue);

    // Assign the Bus to the CPU
    cpu->bus = bus;

    // Create the Monitored RAM Device
    memory_t *monitored_ram =
        memory_create_monitored_ram(0x10000, cpu); // 64KB Monitored RAM

    if (!monitored_ram)
    {
        fprintf(stderr, "Failed to create monitored RAM.\n");
        cleanup(cpu, monitored_ram, bus);
        endwin();
        return EXIT_FAILURE;
    }

    /* Connect the Monitored RAM to the Bus
       Map over the full address space */
    bus_connect_device(bus, monitored_ram, 0x0000, 0xFFFF);

    // Initialize Memory to Zero
    monitored_ram_t *ram_context = (monitored_ram_t *)(monitored_ram->context);
    memset(ram_context->data, 0, ram_context->size);

    // Load the binary
    if (load_binary(cpu, current_binary_path_user, current_load_address_user) !=
        0)
    {
        fprintf(stderr, "Failed to load functional test binary.\n");
        cleanup(cpu, monitored_ram, bus);
        endwin();
        return EXIT_FAILURE;
    }

    // Start Threads for Interface Rendering, Emulation Loop, and Serial I/O
    pthread_t interface_thread, emulation_thread, input_thread, output_thread;
    pthread_create(&interface_thread, NULL, render_interface, cpu);
    pthread_create(&emulation_thread, NULL, emulator_loop, cpu);
    pthread_create(&input_thread, NULL, serial_input_thread, cpu);
    pthread_create(&output_thread, NULL, serial_output_thread, cpu);

    // Create Threads for Interrupt Injection
    pthread_t irq_thread, nmi_thread;
    pthread_create(&irq_thread, NULL, inject_IRQ_thread, cpu);
    pthread_create(&nmi_thread, NULL, inject_NMI_thread, cpu);

    // Wait for Threads to Finish
    pthread_join(interface_thread, NULL);
    pthread_join(emulation_thread, NULL);
    pthread_join(input_thread, NULL);
    pthread_join(output_thread, NULL);
    pthread_join(irq_thread, NULL);
    pthread_join(nmi_thread, NULL);

    // Clean Up Resources
    cleanup(cpu, monitored_ram, bus);

    return EXIT_SUCCESS;
}

/******************************************************************************
 *                          Core Emulator Functions                           *
 ******************************************************************************/

/**
 * @brief Load a binary file into the CPU memory, set reset vector, and reset
 * CPU.
 *
 * @param cpu Pointer to the CPU structure.
 * @param path Path to the binary file.
 * @param load_address Address where the binary will be loaded.
 * @return int 0 on success, -1 on failure.
 */
int load_binary(cpu_6502_t *cpu, const char *path, uint16_t load_address)
{
    // Attempt to load the program into memory starting at the address
    if (cpu_load_program(cpu, path, load_address) != CPU_SUCCESS)
    {
        fprintf(stderr, "Failed to load binary %s.\n", path);
        return -1; // Indicate failure
    }

    // Set the reset vector to the start of the program
    cpu_write(cpu, 0xFFFC, load_address & 0xFF);        // Low byte
    cpu_write(cpu, 0xFFFD, (load_address >> 8) & 0xFF); // High byte

    // Reset the CPU to initialize the PC from the reset vector
    cpu_reset(cpu);

    // Clear the CPU's output queue
    queue_clear(&cpu->output_queue);

    // Verify if the PC is valid after reset
    if (cpu->reg.PC != load_address)
    {
        fprintf(stderr, "PC did not initialize correctly to reset vector\n");
        return -1;
    }

    return 0; // Indicate success
}

/**
 * @brief Display the CPU state in the main window.
 *
 * @param cpu Pointer to the CPU structure.
 */
void print_cpu_state(cpu_6502_t *cpu)
{
    pthread_mutex_lock(&lock); // Lock for safe window update
    werase(cpu_window);
    box(cpu_window, 0, 0);

    // Title: "CPU State" in white bold at position (0, 2)
    wattron(cpu_window, COLOR_PAIR(1) | A_BOLD);
    mvwprintw(cpu_window, 0, 2, " CPU State ");
    wattroff(cpu_window, COLOR_PAIR(1) | A_BOLD);

    // Line 1: PC, SP, Cycles, Stack, and History labels
    // Labels: in light gray
    wattron(cpu_window, COLOR_PAIR(1) | A_DIM);
    mvwprintw(cpu_window, 1, 2, "PC: ");
    mvwprintw(cpu_window, 1, 18, "SP: ");
    mvwprintw(cpu_window, 1, 34, "Cycles: ");
    mvwprintw(cpu_window, 1, 59, "Stack:     History:");
    wattroff(cpu_window, COLOR_PAIR(1) | A_DIM);

    // Values: in cyan
    wattron(cpu_window, COLOR_PAIR(2));
    mvwprintw(cpu_window, 1, 6, "0x%04X", cpu->reg.PC);
    mvwprintw(cpu_window, 1, 22, "0x%02X", cpu->reg.SP);
    mvwprintw(cpu_window, 1, 42, "%llu", cpu->clock.cycle_count);
    wattroff(cpu_window, COLOR_PAIR(2));

    // Line 2: A, X, Y labels
    wattron(cpu_window, COLOR_PAIR(1) | A_DIM);
    mvwprintw(cpu_window, 2, 2, "A:  ");
    mvwprintw(cpu_window, 2, 18, "X:  ");
    mvwprintw(cpu_window, 2, 34, "Y:  ");
    wattroff(cpu_window, COLOR_PAIR(1) | A_DIM);

    // Line 2: A, X, Y values
    wattron(cpu_window, COLOR_PAIR(2));
    mvwprintw(cpu_window, 2, 6, "0x%02X", cpu->reg.A);
    mvwprintw(cpu_window, 2, 22, "0x%02X", cpu->reg.X);
    mvwprintw(cpu_window, 2, 38, "0x%02X", cpu->reg.Y);
    wattroff(cpu_window, COLOR_PAIR(2));

    // Stack and History values starting from line 2
    for (int i = 0; i < INSTRUCTION_HISTORY_SIZE; i++)
    {
        int line = 2 + i;
        // Stack value
        uint8_t stack_value =
            cpu_read(cpu, 0x0100 + ((cpu->reg.SP + i + 1) & 0xFF));
        wattron(cpu_window, COLOR_PAIR(2));
        mvwprintw(cpu_window, line, 59, "%d: $%02X", i + 1, stack_value);
        wattroff(cpu_window, COLOR_PAIR(2));

        // History value
        int idx = (history_index - i - 1 + INSTRUCTION_HISTORY_SIZE) %
                  INSTRUCTION_HISTORY_SIZE;
        uint16_t history_pc = instruction_history[idx];
        wattron(cpu_window, COLOR_PAIR(2));
        mvwprintw(cpu_window, line, 70, "%d: $%04X", i + 1, history_pc);
        wattroff(cpu_window, COLOR_PAIR(2));
    }

    // Line 3: Flags labels
    wattron(cpu_window, COLOR_PAIR(1) | A_DIM);
    mvwprintw(cpu_window, 3, 2, "Flags: N V - B D I Z C");
    wattroff(cpu_window, COLOR_PAIR(1) | A_DIM);

    // Line 4: Flags values
    wattron(cpu_window, COLOR_PAIR(2));
    mvwprintw(cpu_window, 4, 2, "       %d %d %d %d %d %d %d %d",
              (cpu->reg.P & (1 << FLAG_NEGATIVE)) ? 1 : 0,
              (cpu->reg.P & (1 << FLAG_OVERFLOW)) ? 1 : 0,
              (cpu->reg.P & (1 << FLAG_UNUSED)) ? 1 : 0,
              (cpu->reg.P & (1 << FLAG_BREAK)) ? 1 : 0,
              (cpu->reg.P & (1 << FLAG_DECIMAL)) ? 1 : 0,
              (cpu->reg.P & (1 << FLAG_INTERRUPT)) ? 1 : 0,
              (cpu->reg.P & (1 << FLAG_ZERO)) ? 1 : 0,
              (cpu->reg.P & (1 << FLAG_CARRY)) ? 1 : 0);
    wattroff(cpu_window, COLOR_PAIR(2));

    // Line 5: I/O ports and next instruction
    uint8_t input_port = cpu_read(cpu, INPUT_ADDR);
    uint8_t output_port = cpu_read(cpu, OUTPUT_ADDR);
    uint8_t opcode = cpu_read(cpu, cpu->reg.PC);
    const char *mnemonic = opcode_to_mnemonic(opcode);

    // Labels: in light gray
    wattron(cpu_window, COLOR_PAIR(1) | A_DIM);
    mvwprintw(cpu_window, 5, 2, "I/O In: ");
    mvwprintw(cpu_window, 5, 15, "Out: ");
    mvwprintw(cpu_window, 5, 25, "Next Instr: ");
    wattroff(cpu_window, COLOR_PAIR(1) | A_DIM);

    // Values: in cyan
    wattron(cpu_window, COLOR_PAIR(2));
    mvwprintw(cpu_window, 5, 10, "$%02X", input_port);
    mvwprintw(cpu_window, 5, 20, "$%02X", output_port);
    mvwprintw(cpu_window, 5, 37, "%s", mnemonic);
    wattroff(cpu_window, COLOR_PAIR(2));

    // Line 6: Display Performance, Render Time, and Actual FPS
    wattron(cpu_window, COLOR_PAIR(1) | A_DIM);
    mvwprintw(cpu_window, 6, 2, "Performance: ");
    mvwprintw(cpu_window, 6, 25, "Render Time: ");
    mvwprintw(cpu_window, 6, 48, "FPS: ");
    wattroff(cpu_window, COLOR_PAIR(1) | A_DIM);

    // Display percentage and other values
    wattron(cpu_window, COLOR_PAIR(2));
    mvwprintw(cpu_window, 6, 15, "%.1f%%", cpu->performance_percent);
    mvwprintw(cpu_window, 6, 38, "%.3f ms", cpu->render_time * 1000);
    mvwprintw(cpu_window, 6, 53, "%.1f", cpu->actual_fps);
    wattroff(cpu_window, COLOR_PAIR(2));

    // Line 7: Emulator status
    wattron(cpu_window, COLOR_PAIR(1) | A_DIM);
    mvwprintw(cpu_window, 7, 2, "Emulator Status: ");
    wattroff(cpu_window, COLOR_PAIR(1) | A_DIM);

    wattron(cpu_window, COLOR_PAIR(2));
    mvwprintw(cpu_window, 7, 19, "%s", emulator_paused ? "Paused" : "Running");
    wattroff(cpu_window, COLOR_PAIR(2));

    // Line 8: Function keys
    /* Distribute the function keys evenly across the 80 columns */
    int current_x = 2;      // Starting x position
    int line_y = 8;         // Starting y position

    // Space between function keys
    const int spacing[] = {9, 14, 10, 7, 11, 7, 9, 10};

    struct // Define keys and their descriptions
    {
        char *key;
        char *description;
    } func_keys[] = {{"F1:", "Help" }, {"F2:",  "Run/Pause"}, {"F3:", "Load"},
                     {"F4:", "Hz"   }, {"F5:",  "Reset"    }, {"F6:", "PC"  },
                     {"F7:", "Step" }, {"F10:", "Quit"     }};

    int num_keys = sizeof(func_keys) / sizeof(func_keys[0]);

    for (int i = 0; i < num_keys; i++)
    {
        // Function Key Label in bold white
        wattron(cpu_window, COLOR_PAIR(3) | A_BOLD);
        mvwprintw(cpu_window, line_y, current_x, "%s", func_keys[i].key);
        wattroff(cpu_window, COLOR_PAIR(3) | A_BOLD);

        // Function Key Description in light gray
        wattron(cpu_window, COLOR_PAIR(4) | A_DIM);
        mvwprintw(cpu_window, line_y, current_x + strlen(func_keys[i].key) + 1,
                  "%s", func_keys[i].description);
        wattroff(cpu_window, COLOR_PAIR(4) | A_DIM);

        current_x += spacing[i];
    }

    // Refresh the window to apply changes
    wrefresh(cpu_window);
    pthread_mutex_unlock(&lock); // Unlock after update
}

/**
 * @brief Thread function for handling serial input and key events.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *serial_input_thread(void *arg)
{
    int ch;
    int input_x = 1, input_y = 1; // Start position after the border
    int current_line = 0, current_col = 0;
    char input_buffer[INPUT_MAX_LINES][INPUT_MAX_COLS + 1] = {0};
    cpu_6502_t *cpu = (cpu_6502_t *)arg;

    // Main input loop
    while (!emulator_exit)
    {
        if (input_paused)
        {
            usleep(10000); // Sleep to reduce CPU usage
            continue;
        }

        pthread_mutex_lock(&lock);
        ch = wgetch(serial_input_window); // Non-blocking input with timeout
        pthread_mutex_unlock(&lock);

        if (ch != ERR)
        {
            if (ch == KEY_F(1))
            {
                input_paused = true;
                display_help_menu();
                input_paused = false;
            }
            else if (ch == KEY_F(2))
            {
                // Toggle pause/resume
                emulator_paused = !emulator_paused;
                step_mode = false;
            }
            else if (ch == KEY_F(3))
            {
                // Load new binary
                input_paused = true;
                prompt_load_binary(cpu);
                input_paused = false;
            }
            else if (ch == KEY_F(4))
            {
                // Adjust clock speed
                input_paused = true;
                prompt_adjust_clock(cpu);
                input_paused = false;
            }
            else if (ch == KEY_F(5))
            {
                // Reset the program
                emulator_reset = true;
            }
            else if (ch == KEY_F(6))
            {
                // Set PC value
                input_paused = true;
                prompt_set_pc(cpu);
                input_paused = false;
            }
            else if (ch == KEY_F(7))
            {
                // Step mode
                emulator_paused = true;
                step_mode = true;
                step_instruction = true;
            }
            else if (ch == KEY_F(10))
            {
                emulator_exit = true; // Exit emulator
            }
            else if (ch == '\n' || ch == '\r')
            {
                // Send the input buffer content to the CPU input queue
                for (int i = 0; i <= current_line; i++)
                {
                    // Enqueue each character in the line
                    for (int j = 0; j < (int)strlen(input_buffer[i]); j++)
                    {
                        queue_enqueue(&cpu->input_queue,
                                      (uint8_t)input_buffer[i][j]);
                    }

                    // Send newline between lines
                    if (i < current_line)
                    {
                        queue_enqueue(&cpu->input_queue,
                                      (uint8_t)'\n'); // Newline between lines
                    }
                }

                // Send \r\n to the CPU input queue
                queue_enqueue(&cpu->input_queue, (uint8_t)'\r');
                queue_enqueue(&cpu->input_queue, (uint8_t)'\n');

                // Clear the input buffer and window
                memset(input_buffer, 0, sizeof(input_buffer));

                pthread_mutex_lock(&lock);
                werase(serial_input_window);
                box(serial_input_window, 0, 0);
                mvwprintw(serial_input_window, 0, 2, " Serial Input ");
                wrefresh(serial_input_window);
                pthread_mutex_unlock(&lock);

                input_x = 1;
                input_y = 1;
                current_line = 0;
                current_col = 0;
            }
            else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
            {
                // Handle backspace
                if (current_col > 0)
                {
                    current_col--;
                    input_x--;
                    input_buffer[current_line][current_col] = '\0';

                    pthread_mutex_lock(&lock);
                    mvwaddch(serial_input_window, input_y, input_x, ' ');
                    wmove(serial_input_window, input_y, input_x);
                    wrefresh(serial_input_window);
                    pthread_mutex_unlock(&lock);
                }
                else if (current_line > 0)
                {
                    current_line--;
                    current_col = strlen(input_buffer[current_line]);
                    input_y--;
                    input_x = current_col + 1;

                    pthread_mutex_lock(&lock);
                    wmove(serial_input_window, input_y, input_x);
                    wrefresh(serial_input_window);
                    pthread_mutex_unlock(&lock);
                }
            }
            else if (isprint(ch))
            {
                // Add character to input buffer
                if (current_col < INPUT_MAX_COLS)
                {
                    input_buffer[current_line][current_col++] = ch;
                    input_buffer[current_line][current_col] =
                        '\0'; // Null-terminate

                    pthread_mutex_lock(&lock);
                    mvwaddch(serial_input_window, input_y, input_x++, ch);
                    wrefresh(serial_input_window);
                    pthread_mutex_unlock(&lock);
                }
                else
                {
                    // Move to next line
                    if (current_line < INPUT_MAX_LINES - 1)
                    {
                        current_line++;
                        current_col = 0;
                        input_y++;
                        input_x = 1;
                        input_buffer[current_line][current_col++] = ch;
                        input_buffer[current_line][current_col] =
                            '\0'; // Null-terminate

                        pthread_mutex_lock(&lock);
                        mvwaddch(serial_input_window, input_y, input_x++, ch);
                        wrefresh(serial_input_window);
                        pthread_mutex_unlock(&lock);
                    }
                    else
                    {
                        // Scroll up if max lines exceeded
                        for (int i = 0; i < INPUT_MAX_LINES - 1; i++)
                        {
                            strcpy(input_buffer[i], input_buffer[i + 1]);
                        }

                        memset(input_buffer[INPUT_MAX_LINES - 1], 0,
                               INPUT_MAX_COLS + 1);
                        current_col = 0;

                        pthread_mutex_lock(&lock);
                        werase(serial_input_window);
                        box(serial_input_window, 0, 0);
                        mvwprintw(serial_input_window, 0, 2, " Serial Input ");

                        for (int i = 0; i < INPUT_MAX_LINES; i++)
                        {
                            mvwprintw(serial_input_window, i + 1, 1, "%s",
                                      input_buffer[i]);
                        }

                        wrefresh(serial_input_window);
                        pthread_mutex_unlock(&lock);

                        current_line = INPUT_MAX_LINES - 1;
                        input_x = 1 + strlen(input_buffer[current_line]);

                        pthread_mutex_lock(&lock);
                        wmove(serial_input_window, input_y, input_x);
                        pthread_mutex_unlock(&lock);
                    }
                }
            }
        }

        usleep(10000); // Sleep for 10ms to prevent high CPU usage
    }

    // Add a return statement at the end of the function to ensure all paths
    // return a value
    return NULL;
}

/**
 * @brief Thread function for handling serial output.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *serial_output_thread(void *arg)
{
    uint8_t byte;
    int output_x = 1, output_y = 1; // Start position after the border
    int max_x = SERIAL_OUTPUT_WINDOW_WIDTH - 2;
    int max_y = SERIAL_OUTPUT_WINDOW_HEIGHT - 2;
    cpu_6502_t *cpu = (cpu_6502_t *)arg;

    // Main output loop
    while (!emulator_exit)
    {
        // Dequeue bytes from the output queue and display them
        while (queue_dequeue(&cpu->output_queue, &byte))
        {
            pthread_mutex_lock(&lock);

            // Print the character
            mvwaddch(serial_output_window, output_y, output_x++, byte);

            // Handle newline and window scrolling
            if (byte == '\n' || output_x > max_x)
            {
                output_x = 1;
                output_y++;

                // Scroll the window content up
                if (output_y > max_y)
                {
                    wscrl(serial_output_window, 1);
                    output_y = max_y;
                    // Clear the line where new text will be added
                    mvwhline(serial_output_window, output_y, 1, ' ', max_x);
                    // Redraw the border and title after scrolling
                    box(serial_output_window, 0, 0);
                    mvwprintw(serial_output_window, 0, 2, " Serial Output ");
                }
            }

            wrefresh(serial_output_window);
            pthread_mutex_unlock(&lock);
        }

        usleep(10000); // Sleep for 10ms
    }

    return NULL;
}

/**
 * @brief Inject an IRQ interrupt into the CPU.
 *
 * @param cpu Pointer to the CPU structure.
 */
void inject_IRQ(cpu_6502_t *cpu)
{
    printf("Injecting IRQ...\n");
    cpu_inject_IRQ(cpu);
}

/**
 * @brief Inject an IRQ after a specific delay.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *inject_IRQ_thread(void *arg)
{
    cpu_6502_t *cpu = (cpu_6502_t *)arg;

    // Wait for 5 seconds before injecting IRQ
    sleep(5);

    inject_IRQ(cpu);

    return NULL;
}

/**
 * @brief Inject an NMI interrupt into the CPU.
 *
 * @param cpu Pointer to the CPU structure.
 */
void inject_NMI(cpu_6502_t *cpu)
{
    printf("Injecting NMI...\n");
    cpu_inject_NMI(cpu);
}

/**
 * @brief Inject an NMI after a specific delay.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *inject_NMI_thread(void *arg)
{
    cpu_6502_t *cpu = (cpu_6502_t *)arg;

    // Wait for 10 seconds before injecting NMI
    sleep(10);

    inject_NMI(cpu);

    return NULL;
}

/**
 * @brief Thread function for rendering the interface.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *render_interface(void *arg)
{
    cpu_6502_t *cpu = (cpu_6502_t *)arg;

    const int render_interval_ms = 100; // Update interval in milliseconds
    double last_render_time = get_current_time(); // Track last render time

    // Main rendering loop
    while (!emulator_exit)
    {
        double current_time = get_current_time();
        double elapsed_time =
            (current_time - last_render_time) * 1000.0; // Elapsed time in ms

        if (elapsed_time >= render_interval_ms)
        {
            last_render_time = current_time;

            // Measure render time
            double render_start = get_current_time();
            print_cpu_state(cpu); // Update CPU state display
            double render_end = get_current_time();

            // Update render time and FPS metrics
            cpu->render_time = render_end - render_start;
            cpu->actual_fps = 1000.0 / elapsed_time; // Approximate FPS
        }
        else
        {
            // Sleep for remaining time
            usleep((render_interval_ms - elapsed_time) * 1000);
        }
    }

    return NULL;
}

/**
 * @brief Main emulation loop.
 *
 * @param arg Pointer to the CPU structure.
 * @return NULL
 */
void *emulator_loop(void *arg)
{
    cpu_6502_t *cpu = (cpu_6502_t *)arg;

    uint64_t last_cycle_count =
        cpu->clock.cycle_count;            // Track last cycle count
    double last_time = get_current_time(); // Track last timestamp

    // Main emulation loop
    while (!emulator_exit)
    {
        // Check if the emulator needs to reset
        if (emulator_reset)
        {
            if (load_binary(cpu, current_binary_path_user,
                            current_load_address_user) != 0)
            {
                fprintf(stderr, "Failed to reload binary during reset.\n");
                emulator_exit = true;
                break;
            }

            // Reset timing variables
            last_cycle_count = cpu->clock.cycle_count;
            last_time = get_current_time();
            cpu->performance_percent = 0.0;

            // Update control variables
            emulator_reset = false;
            emulator_paused = true;
            step_mode = false;
        }

        // Execute instructions if not paused or in step mode
        if (!emulator_paused || (step_mode && step_instruction))
        {
            if (cpu_execute_instruction(cpu, NULL) != CPU_SUCCESS)
            {
                fprintf(stderr, "Error: Invalid opcode at 0x%04X\n",
                        cpu->reg.PC);
                emulator_exit = true;
                break;
            }

            // Update instruction history
            update_instruction_history(cpu->reg.PC);

            if (step_mode)
            {
                step_instruction = false; // Wait for the next step command
            }

            // Wait according to clock frequency
            clock_wait_next_cycle(&cpu->clock);

            // Performance calculation
            double current_time = get_current_time();

            if (current_time - last_time >= 1.0)
            {
                uint64_t cycles_executed =
                    cpu->clock.cycle_count - last_cycle_count;
                double elapsed_time = current_time - last_time;
                double expected_cycles = cpu->clock.frequency * elapsed_time;

                if (expected_cycles > 0.0)
                {
                    cpu->performance_percent =
                        (cycles_executed / expected_cycles) * 100.0;
                }
                else
                {
                    cpu->performance_percent = 0.0;
                }

                last_cycle_count = cpu->clock.cycle_count;
                last_time = current_time;
            }
        }

        // Small sleep to prevent high CPU usage
        usleep(100);
    }

    return NULL;
}

/**
 * @brief Clean up resources and end curses mode.
 *
 * @param cpu Pointer to the CPU structure.
 * @param ram Pointer to the RAM memory.
 * @param bus Pointer to the Bus structure.
 */
void cleanup(cpu_6502_t *cpu, memory_t *ram, bus_t *bus)
{
    // Delete the windows and end curses mode
    delwin(cpu_window);
    delwin(serial_output_window);
    delwin(serial_input_window);
    endwin();

    // Destroy the CPU
    cpu_destroy(cpu);

    // Destroy the queues associated with the CPU
    queue_destroy(&cpu->input_queue);
    queue_destroy(&cpu->output_queue);

    // Destroy the monitored RAM
    memory_destroy_monitored_ram(ram);

    // Destroy the bus
    bus_destroy(bus);

    // Destroy the synchronization mutex
    pthread_mutex_destroy(&lock);
}

/**
 * @brief Update the instruction history buffer.
 *
 * @param pc The current program counter.
 */
void update_instruction_history(uint16_t pc)
{
    instruction_history[history_index] = pc;
    history_index = (history_index + 1) % INSTRUCTION_HISTORY_SIZE;
}

/**
 * @brief Prompt the user to enter the path of the binary to load.
 *
 * @param cpu Pointer to the CPU structure.
 */
void prompt_load_binary(cpu_6502_t *cpu)
{
    input_paused = true; // Pause input

    // Save and hide cursor
    int prev_cursor = curs_set(0);

    // Calculate window dimensions and position
    const int win_height = 8;
    const int win_width = 60;
    int win_start_y = (LINES - win_height) / 2;
    int win_start_x = ((COLS - win_width) / 2) - 15;

    // Create the prompt window
    WINDOW *prompt_win =
        newwin(win_height, win_width, win_start_y, win_start_x);

    if (prompt_win == NULL)
    {
        input_paused = false;
        curs_set(prev_cursor);
        return;
    }

    keypad(prompt_win, TRUE); // Enable special keys
    cbreak();
    noecho();
    box(prompt_win, 0, 0);

    bool cancelled = false; // Flag to track cancellation
    char path[256];
    int ch;

    do
    {
        // Clear the path buffer
        memset(path, 0, sizeof(path));
        int pos = 0;

        // Clear the window content
        werase(prompt_win);
        box(prompt_win, 0, 0);

        // Prompt for binary path
        mvwprintw(prompt_win, 1, 2, "Enter the path to the binary file:");
        mvwprintw(prompt_win, 2, 2, "> ");
        wrefresh(prompt_win);

        // Read path input
        while (1)
        {
            ch = wgetch(prompt_win);

            if (ch == KEY_ESC || ch == 27) // ESC key to cancel
            {
                cancelled = true;
                break;
            }
            else if (ch == '\n' || ch == '\r') // Enter key to submit
            {
                path[pos] = '\0';
                break;
            }
            else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
            {
                if (pos > 0)
                {
                    pos--;
                    path[pos] = '\0';
                    mvwaddch(prompt_win, 2, 4 + pos, ' ');
                    wmove(prompt_win, 2, 4 + pos);
                    wrefresh(prompt_win);
                }
            }
            else if (isprint(ch) && pos < (int)(sizeof(path) - 1))
            {
                path[pos++] = ch;
                mvwaddch(prompt_win, 2, 4 + pos - 1, ch);
                wrefresh(prompt_win);
            }
        }

        if (cancelled)
        {
            // User cancelled
            // Erase and refresh the window before deletion
            werase(prompt_win);
            wrefresh(prompt_win);
            delwin(prompt_win);
            curs_set(prev_cursor);
            input_paused = false;

            // Refresh main windows
            pthread_mutex_lock(&lock);
            touchwin(stdscr);
            wrefresh(stdscr);
            touchwin(cpu_window);
            wrefresh(cpu_window);
            touchwin(serial_output_window);
            wrefresh(serial_output_window);
            touchwin(serial_input_window);
            wrefresh(serial_input_window);
            pthread_mutex_unlock(&lock);

            return;
        }

        if (path[0] == '\0')
        {
            // Display error message for empty path
            mvwprintw(prompt_win, 4, 2, "Error: Binary path cannot be empty.");
            mvwprintw(prompt_win, 5, 2,
                      "Press any key to retry or ESC to cancel.");
            wrefresh(prompt_win);

            ch = wgetch(prompt_win);
            if (ch == KEY_ESC || ch == 27)
            {
                cancelled = true;
                break;
            }
        }
    } while (path[0] == '\0' && !cancelled);

    if (cancelled)
    {
        // User cancelled during path input
        // Erase and refresh the window before deletion
        werase(prompt_win);
        wrefresh(prompt_win);
        delwin(prompt_win);
        curs_set(prev_cursor);
        input_paused = false;

        // Refresh main windows
        pthread_mutex_lock(&lock);
        touchwin(stdscr);
        wrefresh(stdscr);
        touchwin(cpu_window);
        wrefresh(cpu_window);
        touchwin(serial_output_window);
        wrefresh(serial_output_window);
        touchwin(serial_input_window);
        wrefresh(serial_input_window);
        pthread_mutex_unlock(&lock);

        return;
    }

    // Prompt for load address
    // Clear the window content
    werase(prompt_win);
    box(prompt_win, 0, 0);
    mvwprintw(prompt_win, 1, 2, "Enter load address (e.g., C000):");
    mvwprintw(prompt_win, 2, 2, "> ");
    wrefresh(prompt_win);

    char address_input[16] = {0};
    int addr_pos = 0;
    uint16_t load_address = 0xC000; // Default address

    // Reset cancellation flag for the second input
    cancelled = false;

    // Read address input
    while (1)
    {
        ch = wgetch(prompt_win);

        if (ch == KEY_ESC || ch == 27) // ESC key to cancel
        {
            cancelled = true;
            break;
        }
        else if (ch == '\n' || ch == '\r') // Enter key to submit
        {
            address_input[addr_pos] = '\0';
            if (sscanf(address_input, "%hx", &load_address) != 1)
            {
                load_address = 0xC000; // Default to 0xC000 if parsing fails
            }
            break;
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
        {
            // Handle backspace
            if (addr_pos > 0)
            {
                addr_pos--;
                address_input[addr_pos] = '\0';
                mvwaddch(prompt_win, 2, 4 + addr_pos, ' ');
                wmove(prompt_win, 2, 4 + addr_pos);
                wrefresh(prompt_win);
            }
        }
        else if (isxdigit(ch) && addr_pos < (int)(sizeof(address_input) - 1))
        {
            address_input[addr_pos++] = ch;
            mvwaddch(prompt_win, 2, 4 + addr_pos - 1, ch);
            wrefresh(prompt_win);
        }
    }

    if (cancelled)
    {
        // User cancelled during address input
        // Erase and refresh the window before deletion
        werase(prompt_win);
        wrefresh(prompt_win);
        delwin(prompt_win);
        curs_set(prev_cursor);
        input_paused = false;

        // Refresh main windows
        pthread_mutex_lock(&lock);
        touchwin(stdscr);
        wrefresh(stdscr);
        touchwin(cpu_window);
        wrefresh(cpu_window);
        touchwin(serial_output_window);
        wrefresh(serial_output_window);
        touchwin(serial_input_window);
        wrefresh(serial_input_window);
        pthread_mutex_unlock(&lock);

        return;
    }

    // Try loading the binary file and reset the CPU
    pthread_mutex_lock(&lock);
    int load_result = load_binary(cpu, path, load_address);
    pthread_mutex_unlock(&lock);

    if (load_result != 0)
    {
        // Display error message for failed load
        mvwprintw(prompt_win, 4, 2, "Error: Failed to load the binary file.");
        mvwprintw(prompt_win, 5, 2, "Press any key to continue.");
        wrefresh(prompt_win);
        wgetch(prompt_win); // Wait for a key press before closing
    }
    else
    {
        // Update current binary path and load address
        strncpy(current_binary_path_user, path,
                sizeof(current_binary_path_user) - 1);
        current_load_address_user = load_address;
    }

    // Clean up and restore settings
    werase(prompt_win);
    wrefresh(prompt_win);
    delwin(prompt_win);
    curs_set(prev_cursor);
    nocbreak();
    echo();

    // Refresh main windows
    pthread_mutex_lock(&lock);
    touchwin(stdscr);
    wrefresh(stdscr);
    touchwin(cpu_window);
    wrefresh(cpu_window);
    touchwin(serial_output_window);
    wrefresh(serial_output_window);
    touchwin(serial_input_window);
    wrefresh(serial_input_window);
    pthread_mutex_unlock(&lock);

    input_paused = false;
}

/**
 * @brief Prompt the user to adjust the CPU clock speed.
 *
 * @param cpu Pointer to the CPU structure.
 */
void prompt_adjust_clock(cpu_6502_t *cpu)
{
    input_paused = true; // Pause input

    // Save and hide cursor
    int prev_cursor = curs_set(0);

    // Calculate window dimensions and position
    const int win_height = 8;
    const int win_width = 60;
    int win_start_y = (LINES - win_height) / 2;
    int win_start_x = ((COLS - win_width) / 2) - 15;

    // Create the prompt window
    WINDOW *prompt_win =
        newwin(win_height, win_width, win_start_y, win_start_x);
    if (prompt_win == NULL)
    {
        input_paused = false;
        curs_set(prev_cursor);
        return;
    }

    keypad(prompt_win, TRUE); // Enable special keys
    cbreak();
    noecho();
    box(prompt_win, 0, 0);

    // Prompt for new clock speed
    mvwprintw(prompt_win, 1, 2,
              "Enter new clock speed in Hz (e.g., 1000000 for 1 MHz):");
    mvwprintw(prompt_win, 2, 2, "> ");
    wrefresh(prompt_win);

    char input[256] = {0};
    int ch;
    int pos = 0;

    // Read input
    while (1)
    {
        ch = wgetch(prompt_win);

        if (ch == KEY_ESC || ch == 27)
        {
            break; // User cancelled
        }
        else if (ch == '\n' || ch == '\r')
        {
            input[pos] = '\0';
            double new_clock_speed = atof(input);
            if (new_clock_speed > 0)
            {
                pthread_mutex_lock(&lock);
                cpu_set_clock_frequency(cpu, new_clock_speed);
                pthread_mutex_unlock(&lock);
            }
            else
            {
                mvwprintw(prompt_win, 4, 2, "Invalid clock speed entered.");
                wrefresh(prompt_win);
                wgetch(prompt_win);
            }
            break;
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
        {
            // Handle backspace
            if (pos > 0)
            {
                pos--;
                input[pos] = '\0';
                mvwaddch(prompt_win, 2, 4 + pos, ' ');
                wmove(prompt_win, 2, 4 + pos);
                wrefresh(prompt_win);
            }
        }
        else if ((isdigit(ch) || ch == '.' || ch == 'e' || ch == 'E') &&
                 pos < (int)(sizeof(input) - 1))
        {
            // Add character to clock speed
            input[pos++] = ch;
            mvwaddch(prompt_win, 2, 4 + pos - 1, ch);
            wrefresh(prompt_win);
        }
    }

    // Clean up and restore settings
    delwin(prompt_win);
    curs_set(prev_cursor);
    nocbreak();
    echo();

    // Refresh main windows
    pthread_mutex_lock(&lock);
    touchwin(stdscr);
    wrefresh(stdscr);
    touchwin(cpu_window);
    wrefresh(cpu_window);
    touchwin(serial_output_window);
    wrefresh(serial_output_window);
    touchwin(serial_input_window);
    wrefresh(serial_input_window);
    pthread_mutex_unlock(&lock);

    input_paused = false;
}

/**
 * @brief Prompt the user to set the PC (Program Counter) value.
 *
 * @param cpu Pointer to the CPU structure.
 */
void prompt_set_pc(cpu_6502_t *cpu)
{
    input_paused = true; // Pause input

    // Save and hide cursor
    int prev_cursor = curs_set(0);

    // Calculate window dimensions and position
    const int win_height = 6;
    const int win_width = 58;
    int win_start_y = (LINES - win_height) / 2;
    int win_start_x = ((COLS - win_width) / 2) - 15;

    // Create the prompt window
    WINDOW *prompt_win =
        newwin(win_height, win_width, win_start_y, win_start_x);
    if (prompt_win == NULL)
    {
        input_paused = false;
        curs_set(prev_cursor);
        return;
    }

    keypad(prompt_win, TRUE); // Enable special keys
    cbreak();
    noecho();
    box(prompt_win, 0, 0);

    // Prompt for new PC value
    mvwprintw(prompt_win, 1, 2, "Enter the new PC value (e.g., 0400):");
    mvwprintw(prompt_win, 2, 2, "> ");
    wrefresh(prompt_win);

    char input[16] = {0};
    int ch;
    int pos = 0;
    uint16_t new_pc = cpu->reg.PC; // Default is current PC

    while (1)
    {
        ch = wgetch(prompt_win);

        if (ch == KEY_ESC || ch == 27)
        {
            break; // User cancelled
        }
        else if (ch == '\n' || ch == '\r')
        {
            input[pos] = '\0';
            if (sscanf(input, "%hx", &new_pc) != 1)
            {
                mvwprintw(prompt_win, 4, 2, "Invalid PC value.");
                wrefresh(prompt_win);
                wgetch(prompt_win);
                break;
            }
            else
            {
                pthread_mutex_lock(&lock);
                cpu->reg.PC = new_pc;
                pthread_mutex_unlock(&lock);
                break;
            }
        }
        else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')
        {
            // Handle backspace
            if (pos > 0)
            {
                pos--;
                input[pos] = '\0';
                mvwaddch(prompt_win, 2, 4 + pos, ' ');
                wmove(prompt_win, 2, 4 + pos);
                wrefresh(prompt_win);
            }
        }
        else if (isxdigit(ch) && pos < (int)(sizeof(input) - 1))
        {
            // Add character to address
            input[pos++] = ch;
            mvwaddch(prompt_win, 2, 4 + pos - 1, ch);
            wrefresh(prompt_win);
        }
    }

    // Clean up and restore settings
    delwin(prompt_win);
    curs_set(prev_cursor);
    nocbreak();
    echo();

    // Update main windows
    pthread_mutex_lock(&lock);
    touchwin(stdscr);
    wrefresh(stdscr);
    touchwin(cpu_window);
    wrefresh(cpu_window);
    touchwin(serial_output_window);
    wrefresh(serial_output_window);
    touchwin(serial_input_window);
    wrefresh(serial_input_window);
    pthread_mutex_unlock(&lock);

    input_paused = false;
}

/**
 * @brief Display the help menu with key assignments.
 */
void display_help_menu(void)
{
    input_paused = true; // Pause input

    // Get dimensions of the serial output and input region
    int start_y = CPU_WINDOW_HEIGHT;
    int region_height =
        SERIAL_OUTPUT_WINDOW_HEIGHT + SERIAL_INPUT_WINDOW_HEIGHT;

    // Ensure the width does not exceed the region width
    const int win_height = 13;
    const int win_width = 48;
    int win_start_y = start_y + (region_height - win_height) / 2;
    int win_start_x = ((COLS - win_width) / 2) - 15;

    // Create a help window
    WINDOW *help_win = newwin(win_height, win_width, win_start_y, win_start_x);
    keypad(help_win, TRUE);
    cbreak();
    noecho();
    box(help_win, 0, 0);
    mvwprintw(help_win, 1,  2, "Help Menu:");
    mvwprintw(help_win, 3,  2, "F1  - Help");
    mvwprintw(help_win, 4,  2, "F2  - Run/Pause");
    mvwprintw(help_win, 5,  2, "F3  - Load New Binary");
    mvwprintw(help_win, 6,  2, "F4  - Adjust CPU Clock Speed");
    mvwprintw(help_win, 7,  2, "F5  - Reset Emulator");
    mvwprintw(help_win, 8,  2, "F6  - Set PC Value");
    mvwprintw(help_win, 9,  2, "F7  - Step Instruction");
    mvwprintw(help_win, 10, 2, "F10 - Quit Emulator");
    mvwprintw(help_win, 11, 2, "Press ESC to return.");
    wrefresh(help_win);

    // Hide cursor during help display
    curs_set(0);

    int ch;

    // Wait for ESC key
    while ((ch = wgetch(help_win)) != KEY_ESC && ch != 27)
    {
        // Wait for ESC key
    }

    // Restore cursor visibility
    curs_set(1);
    delwin(help_win);

    // Restore terminal settings
    nocbreak();
    echo();

    // Redraw main windows
    pthread_mutex_lock(&lock);
    touchwin(stdscr);
    wrefresh(stdscr);
    touchwin(cpu_window);
    wrefresh(cpu_window);
    touchwin(serial_output_window);
    wrefresh(serial_output_window);
    touchwin(serial_input_window);
    wrefresh(serial_input_window);
    pthread_mutex_unlock(&lock);

    input_paused = false; // Resume input
}

/**
 * @brief Sleep for a number of milliseconds to maintain FPS.
 * @param fps The number of frames per second.
 */
void sleep_for_fps(int fps)
{
#ifdef _WIN32
    Sleep(1000 / fps);
#else
    struct timespec sleep_time;
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 1000000000 / fps;
    nanosleep(&sleep_time, NULL);
#endif
}
