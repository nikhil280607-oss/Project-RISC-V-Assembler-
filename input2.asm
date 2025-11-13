.data
    .dword  0x12345678ABCDDCBA 
    .asciz  "Test String."     
    .word   10, -5, 20         
    .byte   255                


.text
# Code Segment
# Addresses start at 0x0

# --- U-Format and I-Format Test (0x0 - 0x10) ---
# 0x0
start_pc:
        lui     x1, 0x10000         # x1 = 0x10000000 (Data segment base)
# 0x4
        addi    x5, x0, 10          # x5 = 10 (Loop counter)
# 0x8
        auipc   x2, 0               # x2 = PC (0x8) + 0
# 0xC
        jalr    x0, x2, 4           # PC = x2 + 4 = 0x8 + 4 = 0xC (Jumps over the next instruction) 
# Note: The above JALR is a pseudo-instruction for JUMP, but we test the full format.
# 0x10
        addi    x3, x0, -1          # x3 = -1 (Should be skipped by JALR)

# --- R-Format and S-Format Test (0x14 - 0x24) ---
# 0x14
calc_block:
        add     x6, x5, x5          # x6 = x5 * 2
# 0x18
        sub     x7, x0, x6          # x7 = -x6
# 0x1C
        sd      x7, x1,8          # M[x1+8] = x7. Write to msg_b location (0x10000008)

# --- SB-Format (Branch) Test (0x20 - 0x28) ---
# 0x20
        addi    x5, x5, -1          # Decrement loop counter
# 0x24
        bne     x5, x0, calc_block  # Backward branch test (0x24 -> 0x14). Offset is -16.

# --- UJ-Format (Jump) Test (0x28) ---
# 0x28
        jal     x0, data_init_fail  # Forward jump test. Skip to error handling.

# --- Data Handling and Back Branch Test (0x2C) ---
# 0x2C
        lw      x8, x1, 0         # Load var_a (x8 = M[x1+0])

# --- Target for JAL (Error Check) ---
# 0x30
data_init_fail:
        rem     x10, x8, x5         # dummy op

# --- Back Branch Check (0x34) ---
# 0x34
        bge     x10, x0, done       # Forward branch test (0x34 -> 0x38). Offset is +4.

# 0x38
done:
        ori     x11, x0, 0xFF  