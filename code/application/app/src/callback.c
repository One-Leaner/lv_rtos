#include "spi.h"
#include "usart.h"
#include "esp8266.h"
#include "touch.h"
#include "w25qxx.h"
#include "ui.h"
#include "lvgl.h"
#include "lcd.h"

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        lcd.api->set_cs(lcd.ctrl, GPIO_PIN_SET);
        // lcd.api->notify(lcd.ctrl);// 高频中断中导致系统卡顿
    }
}

void HAL_SPI_TxHalfCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        if (ui.api->get_flush(ui.ctrl))
        {
            lv_display_flush_ready(lv_display_get_default());
            ui.api->set_flush(ui.ctrl, 0);
        }
    }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    UNUSED(hspi);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    UNUSED(huart);
}

// 接收不定长数据
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

// 周期更新中断回调（rtos中，用钩子函数足够了）
#if 0
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        touch.api->scan(touch.ctrl);
        lv_tick_inc(1);
    }
}
#endif
