; -----------------------------------------------------------------------------
; Program: counter.asm
; Description: A simple counting program for the 6502 CPU, which counts from
;              0 to 9 and sends each number via the serial port at $D012.
;              Includes a delay subroutine to control the output speed.
;
; Start Address: $C000
;
; Compilation Instructions:
;   Step 1: Assemble the source code into an object file
;       ca65 counter.asm -o counter.o
;
;   Step 2: Link the object file with a configuration file to create a binary
;       ld65 -C memory.cfg counter.o -o counter.bin
;
; Requirements:
; - 6502 Assembler (ca65)
; - 6502 Linker (ld65)
; - Configuration file (counter.cfg) defining memory layout
;
; -----------------------------------------------------------------------------

    ; Define zero-page variables
    .segment "ZEROPAGE"
temp:    .res 1                    ; Reserve 1 byte for temporary storage

    ; Define code segment
    .segment "CODE"
    .org $C000                   ; Set program start address to $C000

Start:
    LDX #0                       ; Initialize X register to 0 (start counting)

Loop:
    LDA #$30                     ; Load ASCII base for '0' into A
    CLC                          ; Clear carry to avoid unintended addition carry
    STX temp                     ; Store X in temp
    ADC temp                     ; Add temp (X) to A, creating ASCII value of
                                 ; current number (0-9)

    STA $D012                    ; Send the ASCII character via serial port

    JSR Delay                    ; Call delay subroutine to control output speed

    INX                          ; Increment X register to go to the next number
    CPX #10                      ; Compare X with 10 (decimal)
    BNE Loop                     ; If X is not equal to 10, repeat the loop

    LDX #0                       ; Reset X register to 0 (restart counting)
    JMP Loop                     ; Jump back to the start of the loop to repeat
                                 ; counting

; Delay subroutine
Delay:
    ; Save X register before delay
    TXA                         ; Transfer X to A
    PHA                         ; Push A (original X) onto stack

    ; Implement delay using nested loops
    LDX #$FF                    ; Outer loop counter
OuterLoop:
    LDY #$FF                    ; Inner loop counter
InnerLoop:
    DEY                         ; Decrement Y
    BNE InnerLoop               ; Repeat inner loop until Y == 0
    DEX                         ; Decrement X
    BNE OuterLoop               ; Repeat outer loop until X == 0

    ; Restore X register after delay
    PLA                         ; Pull A (original X) from stack
    TAX                         ; Transfer A back to X

    RTS                         ; Return from subroutine
