#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu_6502.h"

/* Internal Helper Functions */

/* Initialize Breakpoints */
void breakpoint_init(breakpoint_t *bp)
{
    if (!bp)
        return;
    bp->count = 0;
}

/* Add Breakpoint */
bool breakpoint_add(breakpoint_t *bp, uint16_t addr)
{
    if (!bp || bp->count >= MAX_BREAKPOINTS)
        return false;

    bp->addresses[bp->count++] = addr;
    return true;
}

/* Check Breakpoint */
bool breakpoint_check(breakpoint_t *bp, uint16_t addr)
{
    if (!bp)
        return false;
    for (int i = 0; i < bp->count; i++)
    {
        if (bp->addresses[i] == addr)
            return true;
    }
    return false;
}

/* Fetch a byte from memory and increment PC */
static inline uint8_t fetch_byte(cpu_6502_t *cpu)
{
    uint8_t byte = cpu_read(cpu, cpu->reg.PC);
    cpu->reg.PC++;
    return byte;
}

/* Fetch a word (two bytes) from memory (little endian) */
static inline uint16_t fetch_word(cpu_6502_t *cpu)
{
    // Fetch low byte, increment PC
    uint8_t low = cpu_read(cpu, cpu->reg.PC);
    cpu->reg.PC = (cpu->reg.PC + 1) & 0xFFFF;

    // Fetch high byte, increment PC
    uint8_t high = cpu_read(cpu, cpu->reg.PC);
    cpu->reg.PC = (cpu->reg.PC + 1) & 0xFFFF;

    // Combine high and low bytes into a 16-bit address
    uint16_t address = ((uint16_t)high << 8) | (uint16_t)low;
    return address;
}

/* Push a byte onto the stack */
static inline void push_byte(cpu_6502_t *cpu, uint8_t value)
{
    cpu_write(cpu, 0x0100 + cpu->reg.SP, value);
    cpu->reg.SP--;
}

/* Pull a byte from the stack */
static inline uint8_t pull_byte(cpu_6502_t *cpu)
{
    cpu->reg.SP++;
    return cpu_read(cpu, 0x0100 + cpu->reg.SP);
}

/* Push a word onto the stack (high byte first) */
static inline void push_word(cpu_6502_t *cpu, uint16_t value)
{
    push_byte(cpu, (value >> 8) & 0xFF); // High byte
    push_byte(cpu, value & 0xFF);        // Low byte
}

/* Pull a word from the stack (low byte first) */
static inline uint16_t pull_word(cpu_6502_t *cpu)
{
    uint8_t low = pull_byte(cpu);
    uint8_t high = pull_byte(cpu);
    return low | (high << 8);
}

/* Addressing Mode Functions */

/* Immediate Addressing */
static effective_address_t addr_immediate(cpu_6502_t *cpu)
{
    uint16_t addr = cpu->reg.PC++;
    return (effective_address_t){addr, false};
}

/* Zero Page Addressing */
static effective_address_t addr_zero_page(cpu_6502_t *cpu)
{
    uint8_t addr = fetch_byte(cpu);
    return (effective_address_t){addr, false};
}

/* Zero Page,X Addressing */
static effective_address_t addr_zero_page_x(cpu_6502_t *cpu)
{
    uint8_t addr = (fetch_byte(cpu) + cpu->reg.X) & 0xFF;
    return (effective_address_t){addr, false};
}

/* Zero Page,Y Addressing */
static effective_address_t addr_zero_page_y(cpu_6502_t *cpu)
{
    uint8_t addr = (fetch_byte(cpu) + cpu->reg.Y) & 0xFF;
    return (effective_address_t){addr, false};
}

/* Absolute Addressing */
static effective_address_t addr_absolute(cpu_6502_t *cpu)
{
    uint16_t addr = fetch_word(cpu);
    return (effective_address_t){addr, false};
}

/* Absolute,X Addressing */
static effective_address_t addr_absolute_x(cpu_6502_t *cpu)
{
    uint16_t base_address = fetch_word(cpu);
    uint16_t effective_address = base_address + cpu->reg.X;
    bool page_crossed = (base_address & 0xFF00) != (effective_address & 0xFF00);
    return (effective_address_t){effective_address, page_crossed};
}

/* Absolute,Y Addressing */
static effective_address_t addr_absolute_y(cpu_6502_t *cpu)
{
    uint16_t base_address = fetch_word(cpu);
    uint16_t effective_address = base_address + cpu->reg.Y;
    bool page_crossed = (base_address & 0xFF00) != (effective_address & 0xFF00);
    return (effective_address_t){effective_address, page_crossed};
}

/* Indirect Addressing (for JMP) */
static effective_address_t addr_indirect(cpu_6502_t *cpu)
{
    uint16_t ptr = fetch_word(cpu);
    uint8_t low = cpu_read(cpu, ptr);
    uint8_t high = cpu_read(cpu, (ptr & 0xFF00) |
                                     ((ptr + 1) & 0x00FF)); // Simulate 6502 bug
    uint16_t addr = ((uint16_t)high << 8) | low;
    return (effective_address_t){addr, false};
}

/* Indexed Indirect (Indirect,X) Addressing */
static effective_address_t addr_indirect_x(cpu_6502_t *cpu)
{
    uint8_t base = (fetch_byte(cpu) + cpu->reg.X) & 0xFF;
    uint8_t low = cpu_read(cpu, base);
    uint8_t high = cpu_read(cpu, (base + 1) & 0xFF);
    uint16_t addr = ((uint16_t)high << 8) | low;
    return (effective_address_t){addr, false};
}

/* Indirect Indexed (Indirect),Y Addressing */
static effective_address_t addr_indirect_y(cpu_6502_t *cpu)
{
    uint8_t base = fetch_byte(cpu);
    uint8_t low = cpu_read(cpu, base);
    uint8_t high = cpu_read(cpu, (base + 1) & 0xFF);
    uint16_t base_address = ((uint16_t)high << 8) | low;
    uint16_t effective_address = base_address + cpu->reg.Y;
    bool page_crossed = (base_address & 0xFF00) != (effective_address & 0xFF00);
    return (effective_address_t){effective_address, page_crossed};
}

/* Relative Addressing */
static effective_address_t addr_relative(cpu_6502_t *cpu)
{
    int8_t offset = (int8_t)fetch_byte(cpu);
    uint16_t effective_address = cpu->reg.PC + offset;
    bool page_crossed = (cpu->reg.PC & 0xFF00) != (effective_address & 0xFF00);
    return (effective_address_t){effective_address, page_crossed};
}

/* Instruction Implementations */

/* ADC (Add with Carry) with Decimal Mode */
static void instr_adc(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint8_t value = cpu_read(cpu, ea.address);
    uint16_t sum;

    if (get_flag(cpu, FLAG_DECIMAL))
    {
        /* Decimal mode */
        uint8_t al =
            (cpu->reg.A & 0x0F) + (value & 0x0F) + get_flag(cpu, FLAG_CARRY);
        uint8_t ah = (cpu->reg.A >> 4) + (value >> 4);

        if (al > 9)
        {
            al -= 10;
            ah += 1;
        }

        if (ah > 9)
        {
            ah -= 10;
            set_flag(cpu, FLAG_CARRY, true);
        }
        else
        {
            set_flag(cpu, FLAG_CARRY, false);
        }

        cpu->reg.A = (ah << 4) | (al & 0x0F);
        update_zero_and_negative_flags(cpu, cpu->reg.A);
    }
    else
    {
        /* Binary mode */
        sum = cpu->reg.A + value + get_flag(cpu, FLAG_CARRY);
        set_flag(cpu, FLAG_CARRY, sum > 0xFF);
        uint8_t result = sum & 0xFF;
        set_flag(cpu, FLAG_OVERFLOW,
                 (~(cpu->reg.A ^ value) & (cpu->reg.A ^ result) & 0x80) != 0);
        cpu->reg.A = result;
        update_zero_and_negative_flags(cpu, cpu->reg.A);
    }

    /* Add extra cycle if page boundary crossed (if applicable) */
    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* AND (Logical AND) */
static void instr_and(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    cpu->reg.A &= cpu_read(cpu, ea.address);
    update_zero_and_negative_flags(cpu, cpu->reg.A);

    /* Add extra cycle if page boundary crossed (if applicable) */
    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* ASL (Arithmetic Shift Left) Memory Mode */
static void instr_asl(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    /* Get the effective address */
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    /* Read the value from memory */
    uint8_t value = cpu_read(cpu, addr);

    /* Perform the shift left */
    set_flag(cpu, FLAG_CARRY, (value & 0x80) != 0);
    value <<= 1;

    /* Write the result back to memory */
    cpu_write(cpu, addr, value);

    /* Update Zero and Negative flags */
    update_zero_and_negative_flags(cpu, value);

    /* Add extra cycle if page boundary crossed (if applicable) */
    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* ASL Accumulator */
static void instr_asl_accumulator(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode; // Suppress unused parameter warning
    set_flag(cpu, FLAG_CARRY, (cpu->reg.A & 0x80) != 0);
    cpu->reg.A <<= 1;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
}

/* BCC (Branch if Carry Clear) */
static void instr_bcc(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    /* Get the effective address and page crossing info */
    effective_address_t ea = mode(cpu);

    /* Check if the Carry flag is clear */
    if (!get_flag(cpu, FLAG_CARRY))
    {
        cpu->reg.PC = ea.address;

        /* Add extra cycles */
        cpu->clock.cycle_count += 1;

        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
    }
}

/* BCS (Branch if Carry Set) */
static void instr_bcs(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);

    if (get_flag(cpu, FLAG_CARRY))
    {
        cpu->reg.PC = ea.address;

        cpu->clock.cycle_count += 1;
        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
    }
}

/* BEQ (Branch if Equal) */
static void instr_beq(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);

    if (get_flag(cpu, FLAG_ZERO))
    {
        cpu->reg.PC = ea.address;

        cpu->clock.cycle_count += 1;
        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
    }
}

/* BIT (Bit Test) */
static void instr_bit(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    uint8_t result = cpu->reg.A & value;

    set_flag(cpu, FLAG_ZERO, (result == 0));
    set_flag(cpu, FLAG_OVERFLOW, (value & 0x40) != 0);
    set_flag(cpu, FLAG_NEGATIVE, (value & 0x80) != 0);
}

/* BMI (Branch if Minus) */
static void instr_bmi(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);

    if (get_flag(cpu, FLAG_NEGATIVE))
    {
        cpu->reg.PC = ea.address;

        cpu->clock.cycle_count += 1;
        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
    }
}

/* BNE (Branch if Not Equal) */
static void instr_bne(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    if (!get_flag(cpu, FLAG_ZERO))
    {
        cpu->clock.cycle_count += 1;
        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
        cpu->reg.PC = ea.address;
    }
}

/* BPL (Branch if Positive) */
static void instr_bpl(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);

    if (!get_flag(cpu, FLAG_NEGATIVE))
    {
        cpu->reg.PC = ea.address;

        cpu->clock.cycle_count += 1;
        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
    }
}

/* BRK (Force Interrupt) */
static void instr_brk(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.PC++;
    push_word(cpu, cpu->reg.PC);
    set_flag(cpu, FLAG_BREAK, true);
    push_byte(cpu, cpu->reg.P | 0x10); // Set Break flag
    set_flag(cpu, FLAG_INTERRUPT, true);
    cpu->reg.PC = cpu_read(cpu, 0xFFFE) | (cpu_read(cpu, 0xFFFF) << 8);
}

/* BVC (Branch if Overflow Clear) */
static void instr_bvc(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);

    if (!get_flag(cpu, FLAG_OVERFLOW))
    {
        cpu->reg.PC = ea.address;

        cpu->clock.cycle_count += 1;
        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
    }
}

/* BVS (Branch if Overflow Set) */
static void instr_bvs(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);

    if (get_flag(cpu, FLAG_OVERFLOW))
    {
        cpu->reg.PC = ea.address;

        cpu->clock.cycle_count += 1;
        if (ea.page_crossed)
        {
            cpu->clock.cycle_count += 1;
        }
    }
}

/* CLC (Clear Carry Flag) */
static void instr_clc(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_CARRY, false);
}

/* CLD (Clear Decimal Mode) */
static void instr_cld(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_DECIMAL, false);
}

/* CLI (Clear Interrupt Disable) */
static void instr_cli(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_INTERRUPT, false);
}

/* CLV (Clear Overflow Flag) */
static void instr_clv(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_OVERFLOW, false);
}

/* CMP (Compare Accumulator) */
static void instr_cmp(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    uint8_t result = cpu->reg.A - value;

    set_flag(cpu, FLAG_CARRY, cpu->reg.A >= value);
    update_zero_and_negative_flags(cpu, result);

    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* CPX (Compare X Register) */
static void instr_cpx(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    uint8_t result = cpu->reg.X - value;

    set_flag(cpu, FLAG_CARRY, cpu->reg.X >= value);
    update_zero_and_negative_flags(cpu, result);
}

/* CPY (Compare Y Register) */
static void instr_cpy(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    uint8_t result = cpu->reg.Y - value;

    set_flag(cpu, FLAG_CARRY, cpu->reg.Y >= value);
    update_zero_and_negative_flags(cpu, result);
}

/* DEC (Decrement Memory) */
static void instr_dec(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    value--;
    cpu_write(cpu, addr, value);

    update_zero_and_negative_flags(cpu, value);
}

/* DEX (Decrement X Register) */
static void instr_dex(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.X--;
    update_zero_and_negative_flags(cpu, cpu->reg.X);
}

/* DEY (Decrement Y Register) */
static void instr_dey(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.Y--;
    update_zero_and_negative_flags(cpu, cpu->reg.Y);
}

/* EOR (Exclusive OR) */
static void instr_eor(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    cpu->reg.A ^= value;

    update_zero_and_negative_flags(cpu, cpu->reg.A);

    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* INC (Increment Memory) */
static void instr_inc(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    value++;
    cpu_write(cpu, addr, value);

    update_zero_and_negative_flags(cpu, value);
}

/* INX (Increment X Register) */
static void instr_inx(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.X++;
    update_zero_and_negative_flags(cpu, cpu->reg.X);
}

/* INY (Increment Y Register) */
static void instr_iny(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.Y++;
    update_zero_and_negative_flags(cpu, cpu->reg.Y);
}

/* JMP (Jump) */
static void instr_jmp(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    cpu->reg.PC = ea.address;
}

/* JSR (Jump to Subroutine) */
static void instr_jsr(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    /* Push return address onto the stack */
    push_word(cpu, cpu->reg.PC - 1);

    /* Jump to subroutine */
    cpu->reg.PC = addr;
}

/* LDA (Load Accumulator) */
static void instr_lda(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    cpu->reg.A = cpu_read(cpu, ea.address);
    update_zero_and_negative_flags(cpu, cpu->reg.A);

    /* Cycle adjustment */
    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* LDX (Load X Register) */
static void instr_ldx(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    cpu->reg.X = cpu_read(cpu, addr);
    update_zero_and_negative_flags(cpu, cpu->reg.X);

    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* LDY (Load Y Register) */
static void instr_ldy(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    cpu->reg.Y = cpu_read(cpu, addr);
    update_zero_and_negative_flags(cpu, cpu->reg.Y);

    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* LSR (Logical Shift Right) Memory Mode */
static void instr_lsr(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);

    set_flag(cpu, FLAG_CARRY, (value & 0x01) != 0);
    value >>= 1;

    cpu_write(cpu, addr, value);

    update_zero_and_negative_flags(cpu, value);
}

/* LSR Accumulator */
static void instr_lsr_accumulator(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_CARRY, cpu->reg.A & 0x01);
    cpu->reg.A >>= 1;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
}

/* NOP (No Operation) */
static void instr_nop(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)cpu;
    (void)mode;
    /* Do nothing */
}

/* ORA (Logical Inclusive OR) */
static void instr_ora(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    cpu->reg.A |= value;

    update_zero_and_negative_flags(cpu, cpu->reg.A);

    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* PHA (Push Accumulator) */
static void instr_pha(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    push_byte(cpu, cpu->reg.A);
}

/* PHP (Push Processor Status) */
static void instr_php(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    push_byte(cpu, cpu->reg.P | 0x30); // Set Break and Unused flags
}

/* PLA (Pull Accumulator) */
static void instr_pla(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.A = pull_byte(cpu);
    update_zero_and_negative_flags(cpu, cpu->reg.A);
}

/* PLP (Pull Processor Status) */
static void instr_plp(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.P = (pull_byte(cpu) & 0xEF) | 0x20; // Clear Break flag, set Unused
}

/* ROL (Rotate Left) Memory Mode */
static void instr_rol(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    uint8_t carry_in = get_flag(cpu, FLAG_CARRY);

    set_flag(cpu, FLAG_CARRY, (value & 0x80) != 0);
    value = (value << 1) | carry_in;

    cpu_write(cpu, addr, value);

    update_zero_and_negative_flags(cpu, value);
}

/* ROL Accumulator */
static void instr_rol_accumulator(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    uint8_t carry = get_flag(cpu, FLAG_CARRY) ? 1 : 0;
    set_flag(cpu, FLAG_CARRY, (cpu->reg.A & 0x80) != 0);
    cpu->reg.A = (cpu->reg.A << 1) | carry;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
}

/* ROR (Rotate Right) Memory Mode */
static void instr_ror(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    uint8_t value = cpu_read(cpu, addr);
    uint8_t carry_in = get_flag(cpu, FLAG_CARRY) << 7;

    set_flag(cpu, FLAG_CARRY, (value & 0x01) != 0);
    value = (value >> 1) | carry_in;

    cpu_write(cpu, addr, value);

    update_zero_and_negative_flags(cpu, value);
}

/* ROR Accumulator */
static void instr_ror_accumulator(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    uint8_t carry = get_flag(cpu, FLAG_CARRY) ? 0x80 : 0x00;
    set_flag(cpu, FLAG_CARRY, cpu->reg.A & 0x01);
    cpu->reg.A = (cpu->reg.A >> 1) | carry;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
}

/* RTI (Return from Interrupt) */
static void instr_rti(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.P = pull_byte(cpu);
    cpu->reg.PC = pull_word(cpu);
}

/* RTS (Return from Subroutine) */
static void instr_rts(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.PC = pull_word(cpu) + 1;
}

/* SBC (Subtract with Carry) */
static void instr_sbc(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;
    uint8_t value = cpu_read(cpu, addr);
    uint8_t carry =
        get_flag(cpu, FLAG_CARRY) ? 0 : 1; // Inverted for subtraction

    uint16_t diff;

    if (get_flag(cpu, FLAG_DECIMAL))
    {
        // Implement Decimal mode subtraction here
        // For brevity, the BCD mode implementation is omitted
    }
    else
    {
        diff = cpu->reg.A - value - carry;
        set_flag(cpu, FLAG_CARRY, diff < 0x100);
        uint8_t result = diff & 0xFF;
        set_flag(cpu, FLAG_OVERFLOW,
                 ((cpu->reg.A ^ value) & (cpu->reg.A ^ result) & 0x80) != 0);
        cpu->reg.A = result;
        update_zero_and_negative_flags(cpu, cpu->reg.A);
    }

    if (ea.page_crossed)
    {
        cpu->clock.cycle_count += 1;
    }
}

/* SEC (Set Carry Flag) */
static void instr_sec(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_CARRY, true);
}

/* SED (Set Decimal Flag) */
static void instr_sed(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_DECIMAL, true);
}

/* SEI (Set Interrupt Disable) */
static void instr_sei(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    set_flag(cpu, FLAG_INTERRUPT, true);
}

/* STA (Store Accumulator) */
static void instr_sta(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    cpu_write(cpu, addr, cpu->reg.A);
}

/* STX (Store X Register) */
static void instr_stx(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    cpu_write(cpu, addr, cpu->reg.X);
}

/* STY (Store Y Register) */
static void instr_sty(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    effective_address_t ea = mode(cpu);
    uint16_t addr = ea.address;

    cpu_write(cpu, addr, cpu->reg.Y);
}

/* TAX (Transfer Accumulator to X) */
static void instr_tax(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.X = cpu->reg.A;
    update_zero_and_negative_flags(cpu, cpu->reg.X);
}

/* TAY (Transfer Accumulator to Y) */
static void instr_tay(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.Y = cpu->reg.A;
    update_zero_and_negative_flags(cpu, cpu->reg.Y);
}

/* TSX (Transfer Stack Pointer to X) */
static void instr_tsx(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.X = cpu->reg.SP;
    update_zero_and_negative_flags(cpu, cpu->reg.X);
}

/* TXA (Transfer X to Accumulator) */
static void instr_txa(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.A = cpu->reg.X;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
}

/* TXS (Transfer X to Stack Pointer) */
static void instr_txs(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.SP = cpu->reg.X;
}

/* TYA (Transfer Y to Accumulator) */
static void instr_tya(cpu_6502_t *cpu, addressing_mode_func_t mode)
{
    (void)mode;
    cpu->reg.A = cpu->reg.Y;
    update_zero_and_negative_flags(cpu, cpu->reg.A);
}

/* Opcode Table Entry */
typedef struct
{
    uint8_t opcode;
    const char *mnemonic;
    instruction_func_t execute;
    addressing_mode_func_t addr_mode;
    uint8_t cycles;
    uint8_t bytes;
} opcode_entry_t;

/* Opcode Table */
static opcode_entry_t opcode_table[256];

/* Initialize Opcode Table with All Opcodes */
static void initialize_opcode_table()
{
    // Initialize all opcodes to an undefined instruction handler
    for (int i = 0; i < 256; i++)
    {
        opcode_table[i].opcode = i;
        opcode_table[i].mnemonic = "???";
        opcode_table[i].execute = NULL;
        opcode_table[i].addr_mode = NULL;
        opcode_table[i].cycles = 0;
        opcode_table[i].bytes = 1;
    }

    /* ADC (Add with Carry) */
    opcode_table[0x69] =
        (opcode_entry_t){0x69, "ADC", instr_adc, addr_immediate, 2, 2};
    opcode_table[0x65] =
        (opcode_entry_t){0x65, "ADC", instr_adc, addr_zero_page, 3, 2};
    opcode_table[0x75] =
        (opcode_entry_t){0x75, "ADC", instr_adc, addr_zero_page_x, 4, 2};
    opcode_table[0x6D] =
        (opcode_entry_t){0x6D, "ADC", instr_adc, addr_absolute, 4, 3};
    opcode_table[0x7D] =
        (opcode_entry_t){0x7D, "ADC", instr_adc, addr_absolute_x, 4, 3};
    opcode_table[0x79] =
        (opcode_entry_t){0x79, "ADC", instr_adc, addr_absolute_y, 4, 3};
    opcode_table[0x61] =
        (opcode_entry_t){0x61, "ADC", instr_adc, addr_indirect_x, 6, 2};
    opcode_table[0x71] =
        (opcode_entry_t){0x71, "ADC", instr_adc, addr_indirect_y, 5, 2};

    /* AND (Logical AND) */
    opcode_table[0x29] =
        (opcode_entry_t){0x29, "AND", instr_and, addr_immediate, 2, 2};
    opcode_table[0x25] =
        (opcode_entry_t){0x25, "AND", instr_and, addr_zero_page, 3, 2};
    opcode_table[0x35] =
        (opcode_entry_t){0x35, "AND", instr_and, addr_zero_page_x, 4, 2};
    opcode_table[0x2D] =
        (opcode_entry_t){0x2D, "AND", instr_and, addr_absolute, 4, 3};
    opcode_table[0x3D] =
        (opcode_entry_t){0x3D, "AND", instr_and, addr_absolute_x, 4, 3};
    opcode_table[0x39] =
        (opcode_entry_t){0x39, "AND", instr_and, addr_absolute_y, 4, 3};
    opcode_table[0x21] =
        (opcode_entry_t){0x21, "AND", instr_and, addr_indirect_x, 6, 2};
    opcode_table[0x31] =
        (opcode_entry_t){0x31, "AND", instr_and, addr_indirect_y, 5, 2};

    /* ASL (Arithmetic Shift Left) */
    opcode_table[0x0A] =
        (opcode_entry_t){0x0A, "ASL", instr_asl_accumulator, NULL, 2, 1};
    opcode_table[0x06] =
        (opcode_entry_t){0x06, "ASL", instr_asl, addr_zero_page, 5, 2};
    opcode_table[0x16] =
        (opcode_entry_t){0x16, "ASL", instr_asl, addr_zero_page_x, 6, 2};
    opcode_table[0x0E] =
        (opcode_entry_t){0x0E, "ASL", instr_asl, addr_absolute, 6, 3};
    opcode_table[0x1E] =
        (opcode_entry_t){0x1E, "ASL", instr_asl, addr_absolute_x, 7, 3};

    /* BCC (Branch if Carry Clear) */
    opcode_table[0x90] =
        (opcode_entry_t){0x90, "BCC", instr_bcc, addr_relative, 2, 2};

    /* BCS (Branch if Carry Set) */
    opcode_table[0xB0] =
        (opcode_entry_t){0xB0, "BCS", instr_bcs, addr_relative, 2, 2};

    /* BEQ (Branch if Equal) */
    opcode_table[0xF0] =
        (opcode_entry_t){0xF0, "BEQ", instr_beq, addr_relative, 2, 2};

    /* BIT (Bit Test) */
    opcode_table[0x24] =
        (opcode_entry_t){0x24, "BIT", instr_bit, addr_zero_page, 3, 2};
    opcode_table[0x2C] =
        (opcode_entry_t){0x2C, "BIT", instr_bit, addr_absolute, 4, 3};

    /* BMI (Branch if Minus) */
    opcode_table[0x30] =
        (opcode_entry_t){0x30, "BMI", instr_bmi, addr_relative, 2, 2};

    /* BNE (Branch if Not Equal) */
    opcode_table[0xD0] =
        (opcode_entry_t){0xD0, "BNE", instr_bne, addr_relative, 2, 2};

    /* BPL (Branch if Positive) */
    opcode_table[0x10] =
        (opcode_entry_t){0x10, "BPL", instr_bpl, addr_relative, 2, 2};

    /* BRK (Force Interrupt) */
    opcode_table[0x00] = (opcode_entry_t){0x00, "BRK", instr_brk, NULL, 7, 1};

    /* BVC (Branch if Overflow Clear) */
    opcode_table[0x50] =
        (opcode_entry_t){0x50, "BVC", instr_bvc, addr_relative, 2, 2};

    /* BVS (Branch if Overflow Set) */
    opcode_table[0x70] =
        (opcode_entry_t){0x70, "BVS", instr_bvs, addr_relative, 2, 2};

    /* CLC (Clear Carry Flag) */
    opcode_table[0x18] = (opcode_entry_t){0x18, "CLC", instr_clc, NULL, 2, 1};

    /* CLD (Clear Decimal Flag) */
    opcode_table[0xD8] = (opcode_entry_t){0xD8, "CLD", instr_cld, NULL, 2, 1};

    /* CLI (Clear Interrupt Disable) */
    opcode_table[0x58] = (opcode_entry_t){0x58, "CLI", instr_cli, NULL, 2, 1};

    /* CLV (Clear Overflow Flag) */
    opcode_table[0xB8] = (opcode_entry_t){0xB8, "CLV", instr_clv, NULL, 2, 1};

    /* CMP (Compare Accumulator) */
    opcode_table[0xC9] =
        (opcode_entry_t){0xC9, "CMP", instr_cmp, addr_immediate, 2, 2};
    opcode_table[0xC5] =
        (opcode_entry_t){0xC5, "CMP", instr_cmp, addr_zero_page, 3, 2};
    opcode_table[0xD5] =
        (opcode_entry_t){0xD5, "CMP", instr_cmp, addr_zero_page_x, 3, 2};
    opcode_table[0xCD] =
        (opcode_entry_t){0xCD, "CMP", instr_cmp, addr_absolute, 4, 3};
    opcode_table[0xDD] =
        (opcode_entry_t){0xDD, "CMP", instr_cmp, addr_absolute_x, 4, 3};
    opcode_table[0xD9] =
        (opcode_entry_t){0xD9, "CMP", instr_cmp, addr_absolute_y, 4, 3};
    opcode_table[0xC1] =
        (opcode_entry_t){0xC1, "CMP", instr_cmp, addr_indirect_x, 6, 2};
    opcode_table[0xD1] =
        (opcode_entry_t){0xD1, "CMP", instr_cmp, addr_indirect_y, 5, 2};

    /* CPX (Compare X Register) */
    opcode_table[0xE0] =
        (opcode_entry_t){0xE0, "CPX", instr_cpx, addr_immediate, 2, 2};
    opcode_table[0xE4] =
        (opcode_entry_t){0xE4, "CPX", instr_cpx, addr_zero_page, 3, 2};
    opcode_table[0xEC] =
        (opcode_entry_t){0xEC, "CPX", instr_cpx, addr_absolute, 4, 3};

    /* CPY (Compare Y Register) */
    opcode_table[0xC0] =
        (opcode_entry_t){0xC0, "CPY", instr_cpy, addr_immediate, 2, 2};
    opcode_table[0xC4] =
        (opcode_entry_t){0xC4, "CPY", instr_cpy, addr_zero_page, 3, 2};
    opcode_table[0xCC] =
        (opcode_entry_t){0xCC, "CPY", instr_cpy, addr_absolute, 4, 3};

    /* DEC (Decrement Memory) */
    opcode_table[0xC6] =
        (opcode_entry_t){0xC6, "DEC", instr_dec, addr_zero_page, 5, 2};
    opcode_table[0xD6] =
        (opcode_entry_t){0xD6, "DEC", instr_dec, addr_zero_page_x, 5, 2};
    opcode_table[0xCE] =
        (opcode_entry_t){0xCE, "DEC", instr_dec, addr_absolute, 6, 3};
    opcode_table[0xDE] =
        (opcode_entry_t){0xDE, "DEC", instr_dec, addr_absolute_x, 7, 3};

    /* DEX (Decrement X Register) */
    opcode_table[0xCA] = (opcode_entry_t){0xCA, "DEX", instr_dex, NULL, 2, 1};

    /* DEY (Decrement Y Register) */
    opcode_table[0x88] = (opcode_entry_t){0x88, "DEY", instr_dey, NULL, 2, 1};

    /* EOR (Exclusive OR) */
    opcode_table[0x49] =
        (opcode_entry_t){0x49, "EOR", instr_eor, addr_immediate, 2, 2};
    opcode_table[0x45] =
        (opcode_entry_t){0x45, "EOR", instr_eor, addr_zero_page, 3, 2};
    opcode_table[0x55] =
        (opcode_entry_t){0x55, "EOR", instr_eor, addr_zero_page_x, 4, 2};
    opcode_table[0x4D] =
        (opcode_entry_t){0x4D, "EOR", instr_eor, addr_absolute, 4, 3};
    opcode_table[0x5D] =
        (opcode_entry_t){0x5D, "EOR", instr_eor, addr_absolute_x, 4, 3};
    opcode_table[0x59] =
        (opcode_entry_t){0x59, "EOR", instr_eor, addr_absolute_y, 4, 3};
    opcode_table[0x41] =
        (opcode_entry_t){0x41, "EOR", instr_eor, addr_indirect_x, 6, 2};
    opcode_table[0x51] =
        (opcode_entry_t){0x51, "EOR", instr_eor, addr_indirect_y, 5, 2};

    /* INC (Increment Memory) */
    opcode_table[0xE6] =
        (opcode_entry_t){0xE6, "INC", instr_inc, addr_zero_page, 5, 2};
    opcode_table[0xF6] =
        (opcode_entry_t){0xF6, "INC", instr_inc, addr_zero_page_x, 5, 2};
    opcode_table[0xEE] =
        (opcode_entry_t){0xEE, "INC", instr_inc, addr_absolute, 7, 3};
    opcode_table[0xFE] =
        (opcode_entry_t){0xFE, "INC", instr_inc, addr_absolute_x, 7, 3};

    /* INX (Increment X Register) */
    opcode_table[0xE8] = (opcode_entry_t){0xE8, "INX", instr_inx, NULL, 2, 1};

    /* INY (Increment Y Register) */
    opcode_table[0xC8] = (opcode_entry_t){0xC8, "INY", instr_iny, NULL, 2, 1};

    /* JMP (Jump) */
    opcode_table[0x4C] =
        (opcode_entry_t){0x4C, "JMP", instr_jmp, addr_absolute, 3, 3};
    opcode_table[0x6C] =
        (opcode_entry_t){0x6C, "JMP", instr_jmp, addr_indirect, 5, 3};

    /* JSR (Jump to Subroutine) */
    opcode_table[0x20] =
        (opcode_entry_t){0x20, "JSR", instr_jsr, addr_absolute, 6, 3};

    /* LDA (Load Accumulator) */
    opcode_table[0xA9] =
        (opcode_entry_t){0xA9, "LDA", instr_lda, addr_immediate, 2, 2};
    opcode_table[0xA5] =
        (opcode_entry_t){0xA5, "LDA", instr_lda, addr_zero_page, 3, 2};
    opcode_table[0xB5] =
        (opcode_entry_t){0xB5, "LDA", instr_lda, addr_zero_page_x, 4, 2};
    opcode_table[0xAD] =
        (opcode_entry_t){0xAD, "LDA", instr_lda, addr_absolute, 4, 3};
    opcode_table[0xBD] =
        (opcode_entry_t){0xBD, "LDA", instr_lda, addr_absolute_x, 4, 3};
    opcode_table[0xB9] =
        (opcode_entry_t){0xB9, "LDA", instr_lda, addr_absolute_y, 4, 3};
    opcode_table[0xA1] =
        (opcode_entry_t){0xA1, "LDA", instr_lda, addr_indirect_x, 6, 2};
    opcode_table[0xB1] =
        (opcode_entry_t){0xB1, "LDA", instr_lda, addr_indirect_y, 5, 2};

    /* LDX (Load X Register) */
    opcode_table[0xA2] =
        (opcode_entry_t){0xA2, "LDX", instr_ldx, addr_immediate, 2, 2};
    opcode_table[0xA6] =
        (opcode_entry_t){0xA6, "LDX", instr_ldx, addr_zero_page, 3, 2};
    opcode_table[0xB6] =
        (opcode_entry_t){0xB6, "LDX", instr_ldx, addr_zero_page_y, 4, 2};
    opcode_table[0xAE] =
        (opcode_entry_t){0xAE, "LDX", instr_ldx, addr_absolute, 4, 3};
    opcode_table[0xBE] =
        (opcode_entry_t){0xBE, "LDX", instr_ldx, addr_absolute_y, 4, 3};

    /* LDY (Load Y Register) */
    opcode_table[0xA0] =
        (opcode_entry_t){0xA0, "LDY", instr_ldy, addr_immediate, 2, 2};
    opcode_table[0xA4] =
        (opcode_entry_t){0xA4, "LDY", instr_ldy, addr_zero_page, 3, 2};
    opcode_table[0xB4] =
        (opcode_entry_t){0xB4, "LDY", instr_ldy, addr_zero_page_x, 4, 2};
    opcode_table[0xAC] =
        (opcode_entry_t){0xAC, "LDY", instr_ldy, addr_absolute, 4, 3};
    opcode_table[0xBC] =
        (opcode_entry_t){0xBC, "LDY", instr_ldy, addr_absolute_x, 4, 3};

    /* LSR (Logical Shift Right) */
    opcode_table[0x4A] =
        (opcode_entry_t){0x4A, "LSR", instr_lsr_accumulator, NULL, 2, 1};
    opcode_table[0x46] =
        (opcode_entry_t){0x46, "LSR", instr_lsr, addr_zero_page, 5, 2};
    opcode_table[0x56] =
        (opcode_entry_t){0x56, "LSR", instr_lsr, addr_zero_page_x, 6, 2};
    opcode_table[0x4E] =
        (opcode_entry_t){0x4E, "LSR", instr_lsr, addr_absolute, 7, 3};
    opcode_table[0x5E] =
        (opcode_entry_t){0x5E, "LSR", instr_lsr, addr_absolute_x, 7, 3};

    /* NOP (No Operation) */
    opcode_table[0xEA] = (opcode_entry_t){0xEA, "NOP", instr_nop, NULL, 2, 1};

    /* ORA (Logical Inclusive OR) */
    opcode_table[0x09] =
        (opcode_entry_t){0x09, "ORA", instr_ora, addr_immediate, 2, 2};
    opcode_table[0x05] =
        (opcode_entry_t){0x05, "ORA", instr_ora, addr_zero_page, 3, 2};
    opcode_table[0x15] =
        (opcode_entry_t){0x15, "ORA", instr_ora, addr_zero_page_x, 4, 2};
    opcode_table[0x0D] =
        (opcode_entry_t){0x0D, "ORA", instr_ora, addr_absolute, 4, 3};
    opcode_table[0x1D] =
        (opcode_entry_t){0x1D, "ORA", instr_ora, addr_absolute_x, 4, 3};
    opcode_table[0x19] =
        (opcode_entry_t){0x19, "ORA", instr_ora, addr_absolute_y, 4, 3};
    opcode_table[0x01] =
        (opcode_entry_t){0x01, "ORA", instr_ora, addr_indirect_x, 6, 2};
    opcode_table[0x11] =
        (opcode_entry_t){0x11, "ORA", instr_ora, addr_indirect_y, 5, 2};

    /* PHA (Push Accumulator) */
    opcode_table[0x48] = (opcode_entry_t){0x48, "PHA", instr_pha, NULL, 3, 1};

    /* PHP (Push Processor Status) */
    opcode_table[0x08] = (opcode_entry_t){0x08, "PHP", instr_php, NULL, 3, 1};

    /* PLA (Pull Accumulator) */
    opcode_table[0x68] = (opcode_entry_t){0x68, "PLA", instr_pla, NULL, 4, 1};

    /* PLP (Pull Processor Status) */
    opcode_table[0x28] = (opcode_entry_t){0x28, "PLP", instr_plp, NULL, 4, 1};

    /* ROL (Rotate Left) */
    opcode_table[0x2A] =
        (opcode_entry_t){0x2A, "ROL", instr_rol_accumulator, NULL, 2, 1};
    opcode_table[0x26] =
        (opcode_entry_t){0x26, "ROL", instr_rol, addr_zero_page, 5, 2};
    opcode_table[0x36] =
        (opcode_entry_t){0x36, "ROL", instr_rol, addr_zero_page_x, 6, 2};
    opcode_table[0x2E] =
        (opcode_entry_t){0x2E, "ROL", instr_rol, addr_absolute, 7, 3};
    opcode_table[0x3E] =
        (opcode_entry_t){0x3E, "ROL", instr_rol, addr_absolute_x, 7, 3};

    /* ROR (Rotate Right) */
    opcode_table[0x6A] =
        (opcode_entry_t){0x6A, "ROR", instr_ror_accumulator, NULL, 2, 1};
    opcode_table[0x66] =
        (opcode_entry_t){0x66, "ROR", instr_ror, addr_zero_page, 5, 2};
    opcode_table[0x76] =
        (opcode_entry_t){0x76, "ROR", instr_ror, addr_zero_page_x, 6, 2};
    opcode_table[0x6E] =
        (opcode_entry_t){0x6E, "ROR", instr_ror, addr_absolute, 7, 3};
    opcode_table[0x7E] =
        (opcode_entry_t){0x7E, "ROR", instr_ror, addr_absolute_x, 7, 3};

    /* RTI (Return from Interrupt) */
    opcode_table[0x40] = (opcode_entry_t){0x40, "RTI", instr_rti, NULL, 6, 1};

    /* RTS (Return from Subroutine) */
    opcode_table[0x60] = (opcode_entry_t){0x60, "RTS", instr_rts, NULL, 6, 1};

    /* SBC (Subtract with Carry) */
    opcode_table[0xE9] =
        (opcode_entry_t){0xE9, "SBC", instr_sbc, addr_immediate, 2, 2};
    opcode_table[0xEB] =
        (opcode_entry_t){0xEB, "SBC", instr_sbc, addr_immediate, 2, 2};
    opcode_table[0xE5] =
        (opcode_entry_t){0xE5, "SBC", instr_sbc, addr_zero_page, 3, 2};
    opcode_table[0xF5] =
        (opcode_entry_t){0xF5, "SBC", instr_sbc, addr_zero_page_x, 3, 2};
    opcode_table[0xED] =
        (opcode_entry_t){0xED, "SBC", instr_sbc, addr_absolute, 4, 3};
    opcode_table[0xFD] =
        (opcode_entry_t){0xFD, "SBC", instr_sbc, addr_absolute_x, 4, 3};
    opcode_table[0xF9] =
        (opcode_entry_t){0xF9, "SBC", instr_sbc, addr_absolute_y, 4, 3};
    opcode_table[0xE1] =
        (opcode_entry_t){0xE1, "SBC", instr_sbc, addr_indirect_x, 6, 2};
    opcode_table[0xF1] =
        (opcode_entry_t){0xF1, "SBC", instr_sbc, addr_indirect_y, 5, 2};

    /* SEC (Set Carry Flag) */
    opcode_table[0x38] = (opcode_entry_t){0x38, "SEC", instr_sec, NULL, 2, 1};

    /* SED (Set Decimal Flag) */
    opcode_table[0xF8] = (opcode_entry_t){0xF8, "SED", instr_sed, NULL, 2, 1};

    /* SEI (Set Interrupt Disable) */
    opcode_table[0x78] = (opcode_entry_t){0x78, "SEI", instr_sei, NULL, 2, 1};

    /* STA (Store Accumulator) */
    opcode_table[0x85] =
        (opcode_entry_t){0x85, "STA", instr_sta, addr_zero_page, 3, 2};
    opcode_table[0x95] =
        (opcode_entry_t){0x95, "STA", instr_sta, addr_zero_page_x, 4, 2};
    opcode_table[0x8D] =
        (opcode_entry_t){0x8D, "STA", instr_sta, addr_absolute, 4, 3};
    opcode_table[0x9D] =
        (opcode_entry_t){0x9D, "STA", instr_sta, addr_absolute_x, 5, 3};
    opcode_table[0x99] =
        (opcode_entry_t){0x99, "STA", instr_sta, addr_absolute_y, 5, 3};
    opcode_table[0x81] =
        (opcode_entry_t){0x81, "STA", instr_sta, addr_indirect_x, 6, 2};
    opcode_table[0x91] =
        (opcode_entry_t){0x91, "STA", instr_sta, addr_indirect_y, 6, 2};

    /* STX (Store X Register) */
    opcode_table[0x86] =
        (opcode_entry_t){0x86, "STX", instr_stx, addr_zero_page, 3, 2};
    opcode_table[0x96] =
        (opcode_entry_t){0x96, "STX", instr_stx, addr_zero_page_y, 4, 2};
    opcode_table[0x8E] =
        (opcode_entry_t){0x8E, "STX", instr_stx, addr_absolute, 4, 3};

    /* STY (Store Y Register) */
    opcode_table[0x84] =
        (opcode_entry_t){0x84, "STY", instr_sty, addr_zero_page, 3, 2};
    opcode_table[0x94] =
        (opcode_entry_t){0x94, "STY", instr_sty, addr_zero_page_x, 4, 2};
    opcode_table[0x8C] =
        (opcode_entry_t){0x8C, "STY", instr_sty, addr_absolute, 4, 3};

    /* TAX (Transfer Accumulator to X) */
    opcode_table[0xAA] = (opcode_entry_t){0xAA, "TAX", instr_tax, NULL, 2, 1};

    /* TAY (Transfer Accumulator to Y) */
    opcode_table[0xA8] = (opcode_entry_t){0xA8, "TAY", instr_tay, NULL, 2, 1};

    /* TSX (Transfer Stack Pointer to X) */
    opcode_table[0xBA] = (opcode_entry_t){0xBA, "TSX", instr_tsx, NULL, 2, 1};

    /* TXA (Transfer X to Accumulator) */
    opcode_table[0x8A] = (opcode_entry_t){0x8A, "TXA", instr_txa, NULL, 2, 1};

    /* TXS (Transfer X to Stack Pointer) */
    opcode_table[0x9A] = (opcode_entry_t){0x9A, "TXS", instr_txs, NULL, 2, 1};

    /* TYA (Transfer Y to Accumulator) */
    opcode_table[0x98] = (opcode_entry_t){0x98, "TYA", instr_tya, NULL, 2, 1};
}

/* CPU Interface Implementations */

/* Initialize the CPU */
cpu_status_t cpu_init(cpu_6502_t *cpu)
{
    if (!cpu)
        return CPU_ERROR_INVALID_ARGUMENT;

    // Initialize registers
    cpu->reg.A = 0x00;
    cpu->reg.X = 0x00;
    cpu->reg.Y = 0x00;
    cpu->reg.PC = 0x0000;
    cpu->reg.SP = 0xFD;  // Stack pointer starts at 0xFD (real 6502 behavior)
    cpu->reg.P = 0x34;

    // Initialize clock (default 1 MHz)
    if (clock_init(&cpu->clock, 1e6) != 0) // 1 MHz
    {
        return CPU_ERROR_INVALID_ARGUMENT;
    }

    // Initialize bus
    cpu->bus = bus_create();
    if (!cpu->bus)
    {
        return CPU_ERROR_INVALID_ARGUMENT;
    }

    // Initialize I/O queues
    queue_init(&cpu->input_queue);
    queue_init(&cpu->output_queue);

    // Initialize mutexes and condition variables
    if (pthread_mutex_init(&cpu->input_queue_mutex, NULL) != 0 ||
        pthread_mutex_init(&cpu->output_queue_mutex, NULL) != 0 ||
        pthread_mutex_init(&cpu->interrupt_mutex, NULL) != 0 ||
        pthread_mutex_init(&cpu->pause_mutex, NULL) != 0)
    {
        fprintf(stderr, "Failed to initialize mutexes.\n");
        return CPU_ERROR_INVALID_ARGUMENT;
    }

    if (pthread_cond_init(&cpu->pause_cond, NULL) != 0)
    {
        fprintf(stderr, "Failed to initialize condition variables.\n");
        return CPU_ERROR_INVALID_ARGUMENT;
    }

    // Initialize interrupt flags
    cpu->IRQ_pending = false;
    cpu->NMI_pending = false;

    // Initialize pause flag
    cpu->paused = false;

    // Initialize debug mode
    cpu->debug_mode = false;

    // Initialize opcode table
    initialize_opcode_table();

    // Initialize performance metrics
    cpu->performance_percent = 0.0;
    cpu->render_time = 0.0;
    cpu->actual_fps = 0.0;

    return CPU_SUCCESS;
}

/* Read a byte from memory */
uint8_t cpu_read(cpu_6502_t *cpu, uint16_t addr)
{
    if (addr == INPUT_ADDR)
    {
        uint8_t data;
        pthread_mutex_lock(&cpu->input_queue_mutex);
        if (queue_dequeue(&cpu->input_queue, &data))
        {
            pthread_mutex_unlock(&cpu->input_queue_mutex);
            return data;
        }
        pthread_mutex_unlock(&cpu->input_queue_mutex);
        return 0x00; // No data available
    }

    return bus_read(cpu->bus, addr);
}

/* Write a byte to memory */
void cpu_write(cpu_6502_t *cpu, uint16_t addr, uint8_t data)
{
    if (addr == OUTPUT_ADDR)
    {
        pthread_mutex_lock(&cpu->output_queue_mutex);
        queue_enqueue(&cpu->output_queue, data);
        pthread_mutex_unlock(&cpu->output_queue_mutex);
    }
    else
    {
        bus_write(cpu->bus, addr, data);
    }
}

/* Destroy the CPU and Free Resources */
void cpu_destroy(cpu_6502_t *cpu)
{
    if (cpu)
    {
        // Destroy queues
        queue_destroy(&cpu->input_queue);
        queue_destroy(&cpu->output_queue);

        // Destroy mutexes and condition variables
        pthread_mutex_destroy(&cpu->input_queue_mutex);
        pthread_mutex_destroy(&cpu->output_queue_mutex);
        pthread_mutex_destroy(&cpu->interrupt_mutex);
        pthread_mutex_destroy(&cpu->pause_mutex);
        pthread_cond_destroy(&cpu->pause_cond);

        // Destroy clock
        clock_destroy(&cpu->clock);

        // Destroy bus
        bus_destroy(cpu->bus);
        cpu->bus = NULL;
    }
}

/* Reset the CPU */
void cpu_reset(cpu_6502_t *cpu)
{
    if (!cpu)
        return;

    // Reset registers
    cpu->reg.A = 0x00;
    cpu->reg.X = 0x00;
    cpu->reg.Y = 0x00;
    cpu->reg.SP = 0xFD;
    cpu->reg.P = 0x34;

    // Set Program Counter to reset vector
    uint8_t low = cpu_read(cpu, 0xFFFC);
    uint8_t high = cpu_read(cpu, 0xFFFD);
    cpu->reg.PC = ((uint16_t)high << 8) | (uint16_t)low;

    // Reset clock cycle count
    cpu->clock.cycle_count = 0;

    // Clear interrupt flags
    pthread_mutex_lock(&cpu->interrupt_mutex);
    cpu->IRQ_pending = false;
    cpu->NMI_pending = false;
    pthread_mutex_unlock(&cpu->interrupt_mutex);

    // Clear pause flag and wake up any waiting threads
    pthread_mutex_lock(&cpu->pause_mutex);
    cpu->paused = false;
    pthread_cond_broadcast(&cpu->pause_cond);
    pthread_mutex_unlock(&cpu->pause_mutex);
}

/* Load a program into memory at a specific address */
cpu_status_t cpu_load_program(cpu_6502_t *cpu, const char *filename,
                              uint16_t addr)
{
    if (!filename || !cpu)
        return CPU_ERROR_INVALID_ARGUMENT;

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror("Error opening program file");
        return CPU_ERROR_FILE_NOT_FOUND;
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size_long = ftell(file);
    rewind(file);

    if (file_size_long == -1L)
    {
        perror("Error determining file size");
        fclose(file);
        return CPU_ERROR_READ_FAILED;
    }

    size_t file_size = (size_t)file_size_long;
    if (file_size == 0)
    {
        fprintf(stderr, "cpu_load_program: File %s is empty.\n", filename);
        fclose(file);
        return CPU_ERROR_READ_FAILED;
    }

    uint8_t *buffer = malloc(file_size);
    if (!buffer)
    {
        fclose(file);
        return CPU_ERROR_MEMORY_OVERFLOW;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size)
    {
        perror("Error reading program file");
        free(buffer);
        return CPU_ERROR_READ_FAILED;
    }

    // Load the buffer into memory starting at addr
    for (size_t i = 0; i < bytes_read; i++)
    {
        cpu_write(cpu, addr + (uint16_t)i, buffer[i]);
    }

    // Set the Reset Vector to the Start Address
    cpu_write(cpu, 0xFFFC, addr & 0xFF);        // Low byte of start address
    cpu_write(cpu, 0xFFFD, (addr >> 8) & 0xFF); // High byte of start address

    free(buffer);

    return CPU_SUCCESS;
}

/* Execute a single CPU instruction with breakpoint checking and interrupt
 * handling */
cpu_status_t cpu_execute_instruction(cpu_6502_t *cpu, breakpoint_t *bp)
{
    if (!cpu)
        return CPU_ERROR_INVALID_ARGUMENT;

    /* Pause Handling */
    pthread_mutex_lock(&cpu->pause_mutex);

    while (cpu->paused)
    {
        pthread_cond_wait(&cpu->pause_cond, &cpu->pause_mutex);
    }

    pthread_mutex_unlock(&cpu->pause_mutex);

    /* Interrupt Handling */
    pthread_mutex_lock(&cpu->interrupt_mutex);
    bool handle_nmi = cpu->NMI_pending;
    bool handle_irq = cpu->IRQ_pending && !get_flag(cpu, FLAG_INTERRUPT);

    if (handle_nmi)
    {
        cpu->NMI_pending = false;
    }

    if (handle_irq)
    {
        cpu->IRQ_pending = false;
    }

    pthread_mutex_unlock(&cpu->interrupt_mutex);

    if (handle_nmi || handle_irq)
    {
        /* Push PC and P to stack */
        push_word(cpu, cpu->reg.PC);
        push_byte(cpu, cpu->reg.P);

        /* Set Interrupt Disable flag */
        set_flag(cpu, FLAG_INTERRUPT, handle_irq ? true : false);

        /* Set PC to interrupt vector */
        if (handle_nmi)
        {
            uint16_t vector =
                cpu_read(cpu, 0xFFFA) | (cpu_read(cpu, 0xFFFB) << 8);
            cpu->reg.PC = vector;
        }
        else if (handle_irq)
        {
            uint16_t vector =
                cpu_read(cpu, 0xFFFE) | (cpu_read(cpu, 0xFFFF) << 8);
            cpu->reg.PC = vector;
        }

        /* Increment cycle count for interrupt handling */
        cpu->clock.cycle_count += 7; // Typical cycles for interrupt
    }

    /* Wait for the next cycle */
    clock_wait_next_cycle(&cpu->clock);

    /* Fetch the next opcode */
    uint8_t opcode = fetch_byte(cpu);
    opcode_entry_t *op = &opcode_table[opcode];

    /* Debug Mode: Print PC and Opcode */
    if (cpu->debug_mode)
    {
        printf("PC: $%04X  Opcode: $%02X (%s)\n", cpu->reg.PC - 1, opcode,
               op->mnemonic ? op->mnemonic : "UNKNOWN");
    }

    /* Check for Breakpoint */
    if (bp && breakpoint_check(bp, cpu->reg.PC - 1))
    {
        printf("Breakpoint hit at PC: $%04X\n", cpu->reg.PC - 1);
        /* Optionally, pause execution or enter debug mode */
    }

    /* Execute the Instruction */
    if (op->execute)
    {
        if (op->addr_mode)
            op->execute(cpu, op->addr_mode);
        else
            op->execute(cpu, NULL);

        return CPU_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Invalid opcode 0x%02X at PC: $%04X\n", opcode,
                cpu->reg.PC - 1);
        return CPU_ERROR_INVALID_OPCODE;
    }
}

/* Set Clock Frequency */
void cpu_set_clock_frequency(cpu_6502_t *cpu, double frequency)
{
    if (!cpu || frequency <= 0.0)
        return;

    pthread_mutex_lock(&cpu->pause_mutex);
    cpu->clock.frequency = frequency;
    cpu->clock.cycle_duration = 1.0 / frequency;
    clock_reset(&cpu->clock); // Reset the clock to apply new frequency
    pthread_mutex_unlock(&cpu->pause_mutex);
}

/* Print CPU State (for debugging purposes) */
void cpu_print_state(const cpu_6502_t *cpu)
{
    if (!cpu)
        return;

    printf(
        "A: 0x%02X  X: 0x%02X  Y: 0x%02X  PC: 0x%04X  SP: 0x%02X  P: 0x%02X\n",
        cpu->reg.A, cpu->reg.X, cpu->reg.Y, cpu->reg.PC, cpu->reg.SP,
        cpu->reg.P);
}

/* Enable or Disable Debug Mode */
void cpu_set_debug_mode(cpu_6502_t *cpu, bool enabled)
{
    if (!cpu)
        return;

    cpu->debug_mode = enabled;
}

/* Interrupt Handling Functions */

/* Inject an IRQ into the CPU */
void cpu_inject_IRQ(cpu_6502_t *cpu)
{
    if (!cpu)
        return;

    pthread_mutex_lock(&cpu->interrupt_mutex);
    cpu->IRQ_pending = true;
    pthread_mutex_unlock(&cpu->interrupt_mutex);
}

/* Inject an NMI into the CPU */
void cpu_inject_NMI(cpu_6502_t *cpu)
{
    if (!cpu)
        return;

    pthread_mutex_lock(&cpu->interrupt_mutex);
    cpu->NMI_pending = true;
    pthread_mutex_unlock(&cpu->interrupt_mutex);
}

/* Pause the CPU Execution */
void cpu_pause(cpu_6502_t *cpu)
{
    if (!cpu)
        return;

    pthread_mutex_lock(&cpu->pause_mutex);
    cpu->paused = true;
    pthread_mutex_unlock(&cpu->pause_mutex);
}

/* Resume the CPU Execution */
void cpu_resume(cpu_6502_t *cpu)
{
    if (!cpu)
        return;

    pthread_mutex_lock(&cpu->pause_mutex);
    cpu->paused = false;
    pthread_cond_broadcast(&cpu->pause_cond);
    pthread_mutex_unlock(&cpu->pause_mutex);
}

/* Example of a Function to Handle All Opcodes (Not Fully Implemented) */
void handle_all_opcodes(cpu_6502_t *cpu, breakpoint_t *bp)
{
    cpu_status_t status = CPU_SUCCESS; // Initialize status

    while (status == CPU_SUCCESS)
    {
        status = cpu_execute_instruction(cpu, bp);
        if (status != CPU_SUCCESS)
            break;
    }
}
