#include "usart.h"
#include "fifo.h"
#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

#define DEBUG_TX_BUF_SIZE 128
#define DEBUG_RX_BUF_SIZE 256

static uint8_t debug_tx_buf[DEBUG_TX_BUF_SIZE];
static uint8_t debug_rx_buf[DEBUG_RX_BUF_SIZE];

struct DEBUG_CTRL
{
    const UART_t *uartx;
    uint8_t *const tx_buf;
    const uint32_t tx_buf_size;
    uint8_t *const rx_buf;
    const uint32_t rx_buf_size;
};

static void debug_init(DEBUG_CTRL_t *ctrl)
{
    ctrl->uartx->api->rxIT(ctrl->uartx->ctrl, ctrl->rx_buf, ctrl->rx_buf_size);
}

static void debug_printf(DEBUG_CTRL_t *ctrl, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf((char *)ctrl->tx_buf, ctrl->tx_buf_size, fmt, args);
    va_end(args);

    if (len > 0)
    {
        while (ctrl->uartx->api->get_state(ctrl->uartx->ctrl) == HAL_UART_STATE_BUSY_TX)
            ;
        ctrl->uartx->api->write(ctrl->uartx->ctrl, ctrl->tx_buf, len);
    }
}

static uint8_t debug_scanf(DEBUG_CTRL_t *ctrl, const char *fmt, ...)
{
    if (!ctrl->uartx->api->get_rxflag(ctrl->uartx->ctrl))
        return 0;
    ctrl->uartx->api->set_rxflag(ctrl->uartx->ctrl, 0);

    uint32_t rx_size = ctrl->uartx->api->get_rx_size(ctrl->uartx->ctrl);
    if (rx_size < ctrl->rx_buf_size)
        ctrl->rx_buf[rx_size] = '\0';
    else
        ctrl->rx_buf[ctrl->rx_buf_size - 1] = '\0';

    ctrl->uartx->api->write(ctrl->uartx->ctrl, (uint8_t *)"\r\n", 2);

    va_list args;
    va_start(args, fmt);
    vsscanf((char *)ctrl->rx_buf, fmt, args);
    va_end(args);

    return 1;
}

static int __io_getchar(void)
{
    static int i = 0;
    uint8_t ch;

    while (!uart2.api->get_rxflag(uart2.ctrl))
        ;

    ch = debug_rx_buf[i++];
    if (ch == '\n' || i >= DEBUG_RX_BUF_SIZE)
    {
        uart2.api->set_rxflag(uart2.ctrl, 0);
        i = 0;
    }

    return ch;
}

int _read(int file, char *ptr, int len)
{
    UNUSED(file);
    UNUSED(len);

    int DataIdx = 0;
    uint8_t ch;

    while (DataIdx < DEBUG_RX_BUF_SIZE)
    {
        ch = __io_getchar();
        ptr[DataIdx++] = ch;
        if (ch == '\n')
            break;
    }

    return DataIdx;
}

int __io_putchar(int ch)
{
    while (uart2.api->get_state(uart2.ctrl) == HAL_UART_STATE_BUSY_TX)
        ;
    uart2.api->write(uart2.ctrl, (uint8_t *)&ch, 1);
    return ch;
}

static DEBUG_CTRL_t debug_ctrl = {
    .uartx = &uart2,
    .tx_buf = debug_tx_buf,
    .tx_buf_size = sizeof(debug_tx_buf),
    .rx_buf = debug_rx_buf,
    .rx_buf_size = sizeof(debug_rx_buf),
};

static DEBUG_API_t debug_api = {
    .init = debug_init,
    .printf = debug_printf,
    .scanf = debug_scanf,
};

const DEBUG_t debug = {
    .ctrl = &debug_ctrl,
    .api = &debug_api,
};