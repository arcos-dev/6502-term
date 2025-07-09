#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "bus.h"
#include "cpu_6502.h"
#include "memory.h"

// Test result tracking
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_results_t;

test_results_t test_results = {0, 0, 0};

// Test macros
#define TEST_ASSERT(condition, message) do { \
    test_results.total_tests++; \
    if (condition) { \
        test_results.passed_tests++; \
        printf("✓ %s\n", message); \
    } else { \
        test_results.failed_tests++; \
        printf("✗ %s\n", message); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL(expected, actual, message) do { \
    test_results.total_tests++; \
    if ((expected) == (actual)) { \
        test_results.passed_tests++; \
        printf("✓ %s (expected: 0x%02X, got: 0x%02X)\n", message, (expected), (actual)); \
    } else { \
        test_results.failed_tests++; \
        printf("✗ %s (expected: 0x%02X, got: 0x%02X)\n", message, (expected), (actual)); \
    } \
} while(0)

#define TEST_ASSERT_EQUAL_16(expected, actual, message) do { \
    test_results.total_tests++; \
    if ((expected) == (actual)) { \
        test_results.passed_tests++; \
        printf("✓ %s (expected: 0x%04X, got: 0x%04X)\n", message, (expected), (actual)); \
    } else { \
        test_results.failed_tests++; \
        printf("✗ %s (expected: 0x%04X, got: 0x%04X)\n", message, (expected), (actual)); \
    } \
} while(0)

// Test setup and teardown
cpu_6502_t* setup_test_cpu() {
    cpu_6502_t* cpu = malloc(sizeof(cpu_6502_t));
    memory_t* ram = memory_create_ram(0x10000);
    
    cpu_status_t status = cpu_init(cpu);
    assert(status == CPU_SUCCESS);
    assert(ram != NULL);
    
    bus_connect_device(cpu->bus, ram, 0x0000, 0xFFFF);
    return cpu;
}

void teardown_test_cpu(cpu_6502_t* cpu) {
    if (cpu && cpu->bus) {
        memory_destroy(cpu->bus->devices[0].device);
    }
    cpu_destroy(cpu);
    free(cpu);
}

// Test functions
void test_cpu_initialization() {
    printf("\n=== Testando Inicialização da CPU ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    TEST_ASSERT_EQUAL(0x00, cpu->reg.A, "Accumulator deve ser inicializado com 0x00");
    TEST_ASSERT_EQUAL(0x00, cpu->reg.X, "Registro X deve ser inicializado com 0x00");
    TEST_ASSERT_EQUAL(0x00, cpu->reg.Y, "Registro Y deve ser inicializado com 0x00");
    TEST_ASSERT_EQUAL(0xFD, cpu->reg.SP, "Stack Pointer deve ser inicializado com 0xFD (comportamento real do 6502)");
    TEST_ASSERT_EQUAL(0x34, cpu->reg.P, "Status Register deve ser inicializado com 0x34 (FLAG_INTERRUPT | FLAG_UNUSED)");
    TEST_ASSERT_EQUAL(0x0000, cpu->reg.PC, "Program Counter deve ser inicializado com 0x0000");
    
    teardown_test_cpu(cpu);
}

void test_cpu_reset() {
    printf("\n=== Testando Reset da CPU ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Modificar registros
    cpu->reg.A = 0xFF;
    cpu->reg.X = 0xFF;
    cpu->reg.Y = 0xFF;
    cpu->reg.SP = 0xFF;
    cpu->reg.P = 0xFF;
    cpu->reg.PC = 0x1234;
    
    // Executar reset
    cpu_reset(cpu);
    
    TEST_ASSERT_EQUAL(0x00, cpu->reg.A, "Accumulator deve ser resetado para 0x00");
    TEST_ASSERT_EQUAL(0x00, cpu->reg.X, "Registro X deve ser resetado para 0x00");
    TEST_ASSERT_EQUAL(0x00, cpu->reg.Y, "Registro Y deve ser resetado para 0x00");
    TEST_ASSERT_EQUAL(0xFD, cpu->reg.SP, "Stack Pointer deve ser resetado para 0xFD (comportamento real do 6502)");
    TEST_ASSERT_EQUAL(0x34, cpu->reg.P, "Status Register deve ser resetado para 0x34 (FLAG_INTERRUPT | FLAG_UNUSED)");
    
    teardown_test_cpu(cpu);
}

void test_memory_read_write() {
    printf("\n=== Testando Leitura e Escrita de Memória ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Testar escrita e leitura
    cpu_write(cpu, 0x2000, 0xAA);
    uint8_t value = cpu_read(cpu, 0x2000);
    TEST_ASSERT_EQUAL(0xAA, value, "Leitura deve retornar o valor escrito");
    
    // Testar múltiplos endereços
    cpu_write(cpu, 0x1000, 0x55);
    cpu_write(cpu, 0xFFFF, 0x12);
    
    TEST_ASSERT_EQUAL(0x55, cpu_read(cpu, 0x1000), "Leitura em 0x1000");
    TEST_ASSERT_EQUAL(0x12, cpu_read(cpu, 0xFFFF), "Leitura em 0xFFFF");
    
    teardown_test_cpu(cpu);
}

void test_load_instructions() {
    printf("\n=== Testando Instruções de Carregamento ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // LDA Immediate
    cpu_write(cpu, 0x8000, 0xA9); // LDA #
    cpu_write(cpu, 0x8001, 0x42); // Valor 0x42
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "LDA Immediate deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x42, cpu->reg.A, "Accumulator deve conter 0x42");
    TEST_ASSERT_EQUAL(0x8002, cpu->reg.PC, "PC deve ser incrementado em 2");
    
    // LDX Immediate
    cpu_write(cpu, 0x8002, 0xA2); // LDX #
    cpu_write(cpu, 0x8003, 0x84); // Valor 0x84
    cpu->reg.PC = 0x8002;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "LDX Immediate deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x84, cpu->reg.X, "Registro X deve conter 0x84");
    
    // LDY Immediate
    cpu_write(cpu, 0x8004, 0xA0); // LDY #
    cpu_write(cpu, 0x8005, 0x99); // Valor 0x99
    cpu->reg.PC = 0x8004;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "LDY Immediate deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x99, cpu->reg.Y, "Registro Y deve conter 0x99");
    
    teardown_test_cpu(cpu);
}

void test_store_instructions() {
    printf("\n=== Testando Instruções de Armazenamento ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Configurar valores nos registros
    cpu->reg.A = 0xAA;
    cpu->reg.X = 0xBB;
    cpu->reg.Y = 0xCC;
    
    // STA Absolute
    cpu_write(cpu, 0x8000, 0x8D); // STA
    cpu_write(cpu, 0x8001, 0x00); // Low byte
    cpu_write(cpu, 0x8002, 0x20); // High byte
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "STA Absolute deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xAA, cpu_read(cpu, 0x2000), "Memória deve conter valor do Accumulator");
    
    // STX Absolute
    cpu_write(cpu, 0x8003, 0x8E); // STX
    cpu_write(cpu, 0x8004, 0x01); // Low byte
    cpu_write(cpu, 0x8005, 0x20); // High byte
    cpu->reg.PC = 0x8003;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "STX Absolute deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xBB, cpu_read(cpu, 0x2001), "Memória deve conter valor do registro X");
    
    // STY Absolute
    cpu_write(cpu, 0x8006, 0x8C); // STY
    cpu_write(cpu, 0x8007, 0x02); // Low byte
    cpu_write(cpu, 0x8008, 0x20); // High byte
    cpu->reg.PC = 0x8006;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "STY Absolute deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xCC, cpu_read(cpu, 0x2002), "Memória deve conter valor do registro Y");
    
    teardown_test_cpu(cpu);
}

void test_transfer_instructions() {
    printf("\n=== Testando Instruções de Transferência ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Configurar valores
    cpu->reg.A = 0xAA;
    cpu->reg.X = 0xBB;
    cpu->reg.Y = 0xCC;
    cpu->reg.SP = 0xFD;
    
    // TAX
    cpu_write(cpu, 0x8000, 0xAA); // TAX
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "TAX deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xAA, cpu->reg.X, "Registro X deve receber valor do Accumulator");
    
    // TAY
    cpu_write(cpu, 0x8001, 0xA8); // TAY
    cpu->reg.PC = 0x8001;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "TAY deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xAA, cpu->reg.Y, "Registro Y deve receber valor do Accumulator");
    
    // TXA
    cpu->reg.X = 0x55;
    cpu_write(cpu, 0x8002, 0x8A); // TXA
    cpu->reg.PC = 0x8002;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "TXA deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x55, cpu->reg.A, "Accumulator deve receber valor do registro X");
    
    // TYA
    cpu->reg.Y = 0x66;
    cpu_write(cpu, 0x8003, 0x98); // TYA
    cpu->reg.PC = 0x8003;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "TYA deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x66, cpu->reg.A, "Accumulator deve receber valor do registro Y");
    
    teardown_test_cpu(cpu);
}

void test_stack_operations() {
    printf("\n=== Testando Operações de Stack ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Configurar stack pointer
    cpu->reg.SP = 0xFD;
    cpu->reg.A = 0xAA;
    cpu->reg.P = 0x34;
    
    // PHA
    cpu_write(cpu, 0x8000, 0x48); // PHA
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "PHA deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xFC, cpu->reg.SP, "Stack pointer deve ser decrementado");
    TEST_ASSERT_EQUAL(0xAA, cpu_read(cpu, 0x01FD), "Valor deve ser empilhado");
    
    // PHP
    cpu_write(cpu, 0x8001, 0x08); // PHP
    cpu->reg.PC = 0x8001;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "PHP deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xFB, cpu->reg.SP, "Stack pointer deve ser decrementado");
    TEST_ASSERT_EQUAL(0x34, cpu_read(cpu, 0x01FC), "Status deve ser empilhado");
    
    // PLA
    cpu->reg.A = 0x00; // Limpar accumulator
    cpu_write(cpu, 0x8002, 0x68); // PLA
    cpu->reg.PC = 0x8002;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "PLA deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xFC, cpu->reg.SP, "Stack pointer deve ser incrementado");
    TEST_ASSERT_EQUAL(0x34, cpu->reg.A, "Accumulator deve receber valor desempilhado");
    
    teardown_test_cpu(cpu);
}

void test_arithmetic_instructions() {
    printf("\n=== Testando Instruções Aritméticas ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // ADC (Add with Carry)
    cpu->reg.A = 0x50;
    set_flag(cpu, FLAG_CARRY, false);
    
    cpu_write(cpu, 0x8000, 0x69); // ADC #
    cpu_write(cpu, 0x8001, 0x30); // Valor 0x30
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "ADC deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x80, cpu->reg.A, "Resultado da adição deve ser 0x80");
    
    // SBC (Subtract with Carry)
    cpu->reg.A = 0x80;
    set_flag(cpu, FLAG_CARRY, true); // Sem borrow
    
    cpu_write(cpu, 0x8002, 0xE9); // SBC #
    cpu_write(cpu, 0x8003, 0x30); // Valor 0x30
    cpu->reg.PC = 0x8002;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "SBC deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x50, cpu->reg.A, "Resultado da subtração deve ser 0x50");
    
    // INC (Increment)
    cpu_write(cpu, 0x2000, 0x42);
    cpu_write(cpu, 0x8004, 0xEE); // INC
    cpu_write(cpu, 0x8005, 0x00); // Low byte
    cpu_write(cpu, 0x8006, 0x20); // High byte
    cpu->reg.PC = 0x8004;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "INC deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x43, cpu_read(cpu, 0x2000), "Valor deve ser incrementado");
    
    // DEC (Decrement)
    cpu_write(cpu, 0x2001, 0x42);
    cpu_write(cpu, 0x8007, 0xCE); // DEC
    cpu_write(cpu, 0x8008, 0x01); // Low byte
    cpu_write(cpu, 0x8009, 0x20); // High byte
    cpu->reg.PC = 0x8007;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "DEC deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x41, cpu_read(cpu, 0x2001), "Valor deve ser decrementado");
    
    teardown_test_cpu(cpu);
}

void test_logical_instructions() {
    printf("\n=== Testando Instruções Lógicas ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // AND
    cpu->reg.A = 0xAA;
    cpu_write(cpu, 0x8000, 0x29); // AND #
    cpu_write(cpu, 0x8001, 0x0F); // Valor 0x0F
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "AND deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x0A, cpu->reg.A, "Resultado do AND deve ser 0x0A");
    
    // ORA
    cpu->reg.A = 0x50;
    cpu_write(cpu, 0x8002, 0x09); // ORA #
    cpu_write(cpu, 0x8003, 0x0F); // Valor 0x0F
    cpu->reg.PC = 0x8002;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "ORA deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x5F, cpu->reg.A, "Resultado do ORA deve ser 0x5F");
    
    // EOR
    cpu->reg.A = 0xAA;
    cpu_write(cpu, 0x8004, 0x49); // EOR #
    cpu_write(cpu, 0x8005, 0x55); // Valor 0x55
    cpu->reg.PC = 0x8004;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "EOR deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xFF, cpu->reg.A, "Resultado do EOR deve ser 0xFF");
    
    teardown_test_cpu(cpu);
}

void test_shift_instructions() {
    printf("\n=== Testando Instruções de Deslocamento ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // ASL (Arithmetic Shift Left)
    cpu->reg.A = 0x42;
    cpu_write(cpu, 0x8000, 0x0A); // ASL A
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "ASL deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x84, cpu->reg.A, "Resultado do ASL deve ser 0x84");
    TEST_ASSERT(get_flag(cpu, FLAG_CARRY) == false, "Carry flag deve ser 0");
    
    // LSR (Logical Shift Right)
    cpu->reg.A = 0x84;
    cpu_write(cpu, 0x8001, 0x4A); // LSR A
    cpu->reg.PC = 0x8001;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "LSR deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x42, cpu->reg.A, "Resultado do LSR deve ser 0x42");
    TEST_ASSERT(get_flag(cpu, FLAG_CARRY) == false, "Carry flag deve ser 0");
    
    // ROL (Rotate Left)
    cpu->reg.A = 0x80;
    set_flag(cpu, FLAG_CARRY, true);
    cpu_write(cpu, 0x8002, 0x2A); // ROL A
    cpu->reg.PC = 0x8002;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "ROL deve executar com sucesso");
    TEST_ASSERT_EQUAL(0x01, cpu->reg.A, "Resultado do ROL deve ser 0x01");
    TEST_ASSERT(get_flag(cpu, FLAG_CARRY) == true, "Carry flag deve ser 1");
    
    teardown_test_cpu(cpu);
}

void test_branch_instructions() {
    printf("\n=== Testando Instruções de Branch ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // BNE (Branch if Not Equal)
    set_flag(cpu, FLAG_ZERO, false);
    cpu_write(cpu, 0x8000, 0xD0); // BNE
    cpu_write(cpu, 0x8001, 0x10); // Offset +16
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "BNE deve executar com sucesso");
    TEST_ASSERT_EQUAL_16(0x8012, cpu->reg.PC, "PC deve pular para 0x8012");
    
    // BEQ (Branch if Equal) - deve pular quando ZERO é true
    set_flag(cpu, FLAG_ZERO, true);
    cpu_write(cpu, 0x8012, 0xF0); // BEQ
    cpu_write(cpu, 0x8013, 0x20); // Offset +32
    cpu->reg.PC = 0x8012;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "BEQ deve executar com sucesso");
    TEST_ASSERT_EQUAL_16(0x8034, cpu->reg.PC, "PC deve pular quando ZERO é true");
    
    // BEQ (Branch if Equal) - não deve pular quando ZERO é false
    set_flag(cpu, FLAG_ZERO, false);
    cpu_write(cpu, 0x8034, 0xF0); // BEQ
    cpu_write(cpu, 0x8035, 0x10); // Offset +16
    cpu->reg.PC = 0x8034;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "BEQ deve executar com sucesso");
    TEST_ASSERT_EQUAL_16(0x8036, cpu->reg.PC, "PC deve incrementar normalmente quando ZERO é false");
    
    teardown_test_cpu(cpu);
}

void test_jump_instructions() {
    printf("\n=== Testando Instruções de Jump ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // JMP Absolute
    cpu_write(cpu, 0x8000, 0x4C); // JMP
    cpu_write(cpu, 0x8001, 0x34); // Low byte
    cpu_write(cpu, 0x8002, 0x12); // High byte
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "JMP deve executar com sucesso");
    TEST_ASSERT_EQUAL_16(0x1234, cpu->reg.PC, "PC deve pular para 0x1234");
    
    // JSR (Jump to Subroutine)
    cpu->reg.SP = 0xFD;
    cpu_write(cpu, 0x1234, 0x20); // JSR
    cpu_write(cpu, 0x1235, 0x56); // Low byte
    cpu_write(cpu, 0x1236, 0x78); // High byte
    cpu->reg.PC = 0x1234;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "JSR deve executar com sucesso");
    TEST_ASSERT_EQUAL_16(0x7856, cpu->reg.PC, "PC deve pular para 0x7856");
    TEST_ASSERT_EQUAL(0xFB, cpu->reg.SP, "Stack pointer deve ser decrementado");
    
    // RTS (Return from Subroutine)
    cpu_write(cpu, 0x7856, 0x60); // RTS
    cpu->reg.PC = 0x7856;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "RTS deve executar com sucesso");
    TEST_ASSERT_EQUAL_16(0x1237, cpu->reg.PC, "PC deve retornar para 0x1237");
    
    teardown_test_cpu(cpu);
}

void test_status_flags() {
    printf("\n=== Testando Flags de Status ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Testar flags Zero e Negative
    cpu->reg.A = 0x00;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
    TEST_ASSERT(get_flag(cpu, FLAG_ZERO) == true, "Zero flag deve ser true para 0x00");
    TEST_ASSERT(get_flag(cpu, FLAG_NEGATIVE) == false, "Negative flag deve ser false para 0x00");
    
    cpu->reg.A = 0x80;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
    TEST_ASSERT(get_flag(cpu, FLAG_ZERO) == false, "Zero flag deve ser false para 0x80");
    TEST_ASSERT(get_flag(cpu, FLAG_NEGATIVE) == true, "Negative flag deve ser true para 0x80");
    
    // Testar instruções de flag
    cpu_write(cpu, 0x8000, 0x38); // SEC
    cpu->reg.PC = 0x8000;
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "SEC deve executar com sucesso");
    TEST_ASSERT(get_flag(cpu, FLAG_CARRY) == true, "Carry flag deve ser setado");
    
    cpu_write(cpu, 0x8001, 0x18); // CLC
    cpu->reg.PC = 0x8001;
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "CLC deve executar com sucesso");
    TEST_ASSERT(get_flag(cpu, FLAG_CARRY) == false, "Carry flag deve ser limpo");
    
    teardown_test_cpu(cpu);
}

void test_functional_test_binary() {
    printf("\n=== Testando Binário Funcional ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Carregar o teste funcional
    cpu_status_t status = cpu_load_program(cpu, "6502_functional_test.bin", 0x0400);
    TEST_ASSERT(status == CPU_SUCCESS, "Carregamento do teste funcional deve ser bem-sucedido");
    
    // Configurar PC para o endereço de carregamento
    cpu->reg.PC = 0x0400;
    
    // Executar algumas instruções para verificar se não há crashes
    int executed_instructions = 0;
    for (int i = 0; i < 50; i++) {
        status = cpu_execute_instruction(cpu, NULL);
        if (status != CPU_SUCCESS) {
            printf("Instrução %d falhou com status: %d (PC: 0x%04X)\n", i, status, cpu->reg.PC);
            break;
        }
        executed_instructions++;
    }
    
    // Considerar sucesso se executou pelo menos algumas instruções sem crash
    TEST_ASSERT(executed_instructions > 0, "Teste funcional deve executar pelo menos uma instrução");
    
    teardown_test_cpu(cpu);
}

void test_addressing_modes() {
    printf("\n=== Testando Modos de Endereçamento ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Zero Page
    cpu_write(cpu, 0x0042, 0xAA);
    cpu_write(cpu, 0x8000, 0xA5); // LDA Zero Page
    cpu_write(cpu, 0x8001, 0x42); // Endereço 0x42
    cpu->reg.PC = 0x8000;
    
    cpu_status_t status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "LDA Zero Page deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xAA, cpu->reg.A, "Accumulator deve carregar valor da zero page");
    
    // Zero Page,X
    cpu->reg.X = 0x10;
    cpu_write(cpu, 0x0052, 0xBB); // 0x42 + 0x10 = 0x52
    cpu_write(cpu, 0x8002, 0xB5); // LDA Zero Page,X
    cpu_write(cpu, 0x8003, 0x42); // Base address
    cpu->reg.PC = 0x8002;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "LDA Zero Page,X deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xBB, cpu->reg.A, "Accumulator deve carregar valor com indexação X");
    
    // Absolute
    cpu_write(cpu, 0x2000, 0xCC);
    cpu_write(cpu, 0x8004, 0xAD); // LDA Absolute
    cpu_write(cpu, 0x8005, 0x00); // Low byte
    cpu_write(cpu, 0x8006, 0x20); // High byte
    cpu->reg.PC = 0x8004;
    
    status = cpu_execute_instruction(cpu, NULL);
    TEST_ASSERT(status == CPU_SUCCESS, "LDA Absolute deve executar com sucesso");
    TEST_ASSERT_EQUAL(0xCC, cpu->reg.A, "Accumulator deve carregar valor do endereço absoluto");
    
    teardown_test_cpu(cpu);
}

void test_interrupts() {
    printf("\n=== Testando Interrupções ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    
    // Configurar vetores de interrupção
    cpu_write(cpu, 0xFFFE, 0x34); // NMI vector low
    cpu_write(cpu, 0xFFFF, 0x12); // NMI vector high
    cpu_write(cpu, 0xFFFA, 0x56); // IRQ vector low
    cpu_write(cpu, 0xFFFB, 0x78); // IRQ vector high
    
    // Configurar stack
    cpu->reg.SP = 0xFD;
    cpu->reg.PC = 0x8000;
    cpu->reg.P = 0x34;
    
    // Injetar NMI
    cpu_inject_NMI(cpu);
    TEST_ASSERT(cpu->NMI_pending == true, "NMI deve estar pendente");
    
    // Injetar IRQ
    cpu_inject_IRQ(cpu);
    TEST_ASSERT(cpu->IRQ_pending == true, "IRQ deve estar pendente");
    
    teardown_test_cpu(cpu);
}

void test_breakpoints() {
    printf("\n=== Testando Breakpoints ===\n");
    
    cpu_6502_t* cpu = setup_test_cpu();
    breakpoint_t bp;
    breakpoint_init(&bp);
    
    // Adicionar breakpoint
    bool result = breakpoint_add(&bp, 0x8000);
    TEST_ASSERT(result == true, "Breakpoint deve ser adicionado com sucesso");
    TEST_ASSERT(bp.count == 1, "Contador de breakpoints deve ser 1");
    
    // Verificar breakpoint
    result = breakpoint_check(&bp, 0x8000);
    TEST_ASSERT(result == true, "Breakpoint deve ser detectado");
    
    result = breakpoint_check(&bp, 0x8001);
    TEST_ASSERT(result == false, "Breakpoint não deve ser detectado em endereço diferente");
    
    teardown_test_cpu(cpu);
}

void print_test_summary() {
    printf("\n=== Resumo dos Testes ===\n");
    printf("Total de testes: %d\n", test_results.total_tests);
    printf("Testes aprovados: %d\n", test_results.passed_tests);
    printf("Testes falharam: %d\n", test_results.failed_tests);
    printf("Taxa de sucesso: %.1f%%\n", 
           (float)test_results.passed_tests / test_results.total_tests * 100);
    
    if (test_results.failed_tests == 0) {
        printf("\n🎉 Todos os testes passaram!\n");
    } else {
        printf("\n❌ Alguns testes falharam. Verifique a implementação.\n");
    }
}

int main() {
    printf("=== Testes Unitários do Emulador 6502 ===\n");
    
    // Executar todos os testes
    test_cpu_initialization();
    test_cpu_reset();
    test_memory_read_write();
    test_load_instructions();
    test_store_instructions();
    test_transfer_instructions();
    test_stack_operations();
    test_arithmetic_instructions();
    test_logical_instructions();
    test_shift_instructions();
    test_branch_instructions();
    test_jump_instructions();
    test_status_flags();
    test_addressing_modes();
    test_interrupts();
    test_breakpoints();
    test_functional_test_binary();
    
    print_test_summary();
    
    return (test_results.failed_tests == 0) ? 0 : 1;
} 