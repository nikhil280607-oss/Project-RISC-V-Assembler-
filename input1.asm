# Start of Code Segment
.text
    addi x10, x0, 100       
    lui x1, 0x10000     

start_loop:
    sub x10, x10, x11      
    slt x12, x10, x0
    bne x12, x0, exit       

    lw x5, ,x1,0    
    jal x1, start_loop     

exit:
    sd x10, x1,8          

# Start of Data Segment
.data
my_data:
    .word 123
    .byte 10
my_string:
    .asciz "RISCV"