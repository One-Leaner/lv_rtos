#include "usart.h"

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    {
        // pRxBuffPtr参数内部调用时，应该对其进行了递增处理，与传入的起始地址不一样了，需要偏移回去
        uart2.api->rxIT(uart2.ctrl, huart->pRxBuffPtr - Size, huart->RxXferSize);
        uart2.api->set_rx_size(uart2.ctrl, Size);
        uart2.api->set_rxflag(uart2.ctrl, 1);
    }
    else if (huart->Instance == USART3)
    {
        uart3.api->rxIT(uart3.ctrl, huart->pRxBuffPtr - Size, huart->RxXferSize);
        uart3.api->set_rx_size(uart3.ctrl, Size);
        uart3.api->set_rxflag(uart3.ctrl, 1);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM7)
    {
        HAL_IncTick();
    }
}