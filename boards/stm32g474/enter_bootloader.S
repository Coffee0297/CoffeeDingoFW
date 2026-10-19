.section .text.Reset_Handler, "ax"  // Place in the .text.Reset_Handler section
.global Reset_Handler
.type Reset_Handler, %function

Reset_Handler:
LDR R0, =0x2001FFF0 // End of SRAM
LDR R1, =0xDEADBEEF // magic value
LDR R2, [R0, #0]
MOV R3, #0           // Prepare 0 to clear the magic value
STR R3, [R0, #0]     // Invalidate the magic value
CMP R2, R1
BEQ UseDFU
   // bootloader not needed, jump back to normal ChibiOS startup
   // register jump: a narrow B cannot reach _crt0_entry in a large image
   LDR R0, =_crt0_entry
   BX  R0

UseDFU:
LDR R0, =0x40021060 // RCC_APB2ENR
LDR R1, =0x00000001 // ENABLE SYSCFG CLOCK
STR R1, [R0, #0]
LDR R0, =0x40010000 // SYSCFG_MEMRMP
LDR R1, =0x00000001 // MAP ROM AT ZERO
STR R1, [R0, #0]
LDR R0, =0x1FFF0000 // ROM BASE
LDR R3, [R0, #0]     // Load new stack pointer into R3 (low register)
MOV SP, R3           // Move R3 to SP (high register)
LDR R0,[R0, #4] // PC @ +4
BX R0 // jump to bootloader