#include "usart.h"
// #include "fifo.h"
#include "debug.h"
#include <stdio.h>
#include <stdarg.h>

#define DEBUG_TX_BUF_SIZE 128
#define DEBUG_RX_BUF_SIZE 128

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
    va_list args;                                                            // 创建参数列表
    va_start(args, fmt);                                                     // 初始化arg,使其指向第一个可变参数
    int len = vsnprintf((char *)ctrl->tx_buf, ctrl->tx_buf_size, fmt, args); // 将可变参数打印到字符串对应的格式化字符位置，并保存在字符串string中(这里sprintf不可用)
    va_end(args);                                                            // 释放参数列表

    if (len > 0)
    {
        while (ctrl->uartx->api->get_state(ctrl->uartx->ctrl) == HAL_UART_STATE_BUSY_TX)
            ;
        ctrl->uartx->api->write(ctrl->uartx->ctrl, ctrl->tx_buf, len);
    }
}

// 非阻塞scanf
static uint8_t debug_scanf(DEBUG_CTRL_t *ctrl, const char *fmt, ...)
{
    if (!ctrl->uartx->api->get_rxflag(ctrl->uartx->ctrl))
        return 0;
    ctrl->uartx->api->set_rxflag(ctrl->uartx->ctrl, 0);
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    uint32_t rx_size = ctrl->uartx->api->get_rx_size(ctrl->uartx->ctrl);
    if (rx_size < ctrl->rx_buf_size)
        ctrl->rx_buf[rx_size] = '\0'; // 末尾添加\0，方便后续解析
    else
        ctrl->rx_buf[ctrl->rx_buf_size - 1] = '\0'; // 末尾添加\0

    uart2.api->write(ctrl->uartx->ctrl, (uint8_t *)("\r\n"), sizeof("\r\n")); // 回显换行，也可以不用
    va_list args;
    va_start(args, fmt);
    vsscanf((char *)ctrl->rx_buf, fmt, args); // 输入函数是以\n（换行回车为结束标志的！与scanf一样）
    va_end(args);

    return 1;
}

// scanf重定向
static int __io_getchar()
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

// 重写_read函数，scanf底层会调用这个函数，
// 通过其调用__io_getchar，然而其内部传入的参数len的默认值是1024，不好直接使用
int _read(int file, char *ptr, int len)
{
    UNUSED(file);
    UNUSED(len);
    int DataIdx = 0;
    uint8_t ch;

    while (DataIdx < DEBUG_RX_BUF_SIZE) // 默认的scanf函数就是阻塞的
    {
        ch = __io_getchar();
        ptr[DataIdx++] = ch;
        if (ch == '\n')
            break;
    }

    return DataIdx;
}

// printf重定向（gcc使用的是newlib库，调用的是这个接口，而不是fputc，scanf也是一样的）
int __io_putchar(int ch)
{
    // UNUSED(f);
    while (uart2.api->get_state(uart2.ctrl) == HAL_UART_STATE_BUSY_TX)
        ;
    uart2.api->write(uart2.ctrl, (uint8_t *)&ch, 1);
    return ch;
}

////////////////////////////////////////////////////////////////////////////

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
