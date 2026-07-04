#include "stm32f4xx_hal.h"

void delay_init()
{
    // 使能 DWT 计数器
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    while (DWT->CYCCNT - start < us * 168)
        ;
}
