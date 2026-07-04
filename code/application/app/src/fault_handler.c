#include <stdio.h>
#include <stdint.h>

void HardFault_Handler_C(uint32_t *stack_frame, uint32_t lr_value)
{
    /* 步骤 2.1: 打印标题和基础信息 */
    printf("\n\n========================================\n");
    printf("        !! HARD FAULT DETECTED !!      \n");
    printf("========================================\n\n");

    printf("Exception frame is on %s Stack.\n",
           (lr_value & 0x4) ? "Process (PSP)" : "Main (MSP)");

    /* 步骤 2.2: 从堆栈帧中提取并打印自动保存的寄存器 */
    // 入栈顺序：R0, R1, R2, R3, R12, LR, PC, xPSR
    uint32_t r0 = stack_frame[0];
    uint32_t r1 = stack_frame[1];
    uint32_t r2 = stack_frame[2];
    uint32_t r3 = stack_frame[3];
    uint32_t r12 = stack_frame[4];
    uint32_t lr = stack_frame[5]; // 这是进入异常前的LR，非常重要！
    uint32_t pc = stack_frame[6]; // 这是进入异常前的PC，指出了出错指令的地址！
    uint32_t psr = stack_frame[7];

    printf("-- Stacked Registers --\n");
    printf("R0  : 0x%08lX\n", r0);
    printf("R1  : 0x%08lX\n", r1);
    printf("R2  : 0x%08lX\n", r2);
    printf("R3  : 0x%08lX\n", r3);
    printf("R12 : 0x%08lX\n", r12);
    printf("LR  : 0x%08lX (Return Address)\n", lr);
    printf("PC  : 0x%08lX (Faulting Instruction Address)\n", pc);
    printf("PSR : 0x%08lX\n\n", psr);

    /* 步骤 2.4: 故障发生后，在此停止CPU，以便调试器查看现场 */
    while (1)
        ;
}