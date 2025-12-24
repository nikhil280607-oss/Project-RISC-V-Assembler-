# RISC-V Assembler (C++)

A two-pass assembler for the RISC-V ISA implemented in C++.  
It converts RISC-V assembly code into 32-bit hexadecimal machine code.

## Features
- Two-pass assembler (symbol table + machine code generation)
- Supports RISC-V instruction formats: R, I, S, SB, U, UJ
- Handles labels and branch/jump resolution
- Supports .text and .data segments
- Data directives: .byte, .half, .word, .dword, .asciz
- Register mapping (x0–x31 and common aliases)
- Address alignment and two’s complement immediate encoding

## Supported Instructions
- Arithmetic & logical: add, sub, and, or, xor, sll, srl, sra, slt
- Immediate instructions: addi, andi, ori
- Memory: lb, lh, lw, ld, sb, sh, sw, sd
- Control flow: beq, bne, blt, bge, jal, jalr
- Upper immediates: lui, auipc
- M-extension: mul, div, rem (and word variants)

## Project Files
- RISCV_assembler_final.cpp
- input1.asm, input2.asm
- output1.mc, output2.mc

## Build
```bash
g++ -std=c++17 RISCV_assembler_final.cpp -o riscv_assembler
./riscv_assembler input1.asm output1.mc
./riscv_assembler input2.asm output2.mc
