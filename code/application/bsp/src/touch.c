#include "touch.h"
#include "matrix.h"
#include "debug.h"
#include "spi.h"
#include "lcd.h"
#include "w25qxx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ff.h"
#include <stdlib.h>

struct TOUCH_CTRL
{
    uint8_t cmd_x;
    uint8_t cmd_y;

    const SPI_API_t *spix_api;
    const LCD_t *lcdx;
    const W25QXX_t *flash;

    const GPIO_Pin_t io_irq; // 定时扫描该引脚电平
    const GPIO_Pin_t io_cs;  // 选中引脚

    volatile uint8_t touch_state; // 触摸标志
    volatile uint8_t adjust_flag; // 校准完成标志
    volatile uint8_t sw;          // 模块开关

    const uint16_t error;
    float adjust_value[2][3]; // 校准参数

    uint16_t scan_time_ms;
    uint16_t crr_time_ms;

    void (*read_adc)(struct TOUCH_CTRL *, uint8_t *, uint16_t *);
    uint8_t (*read_adc_xy)(struct TOUCH_CTRL *, uint16_t *, uint16_t *);
    void (*reset)(struct TOUCH_CTRL *);
    void (*set_cs)(struct TOUCH_CTRL *, GPIO_PinState);
};

#if 0
static uint16_t adc_x[4][4];
static uint16_t adc_y[4][4];
static uint16_t scr_x[4][4];
static uint16_t scr_y[4][4];
static float radio_x[4] = {0.05f, 0.35f, 0.65f, 0.95f};
static float radio_y[4] = {0.05f, 0.35f, 0.65f, 0.95f};
#endif

static void touch_init(TOUCH_CTRL_t *ctrl, uint16_t scan_time_ms)
{
    ctrl->scan_time_ms = scan_time_ms;
    ctrl->crr_time_ms = 0;
    ctrl->touch_state = 0;
    ctrl->adjust_flag = 0;
    ctrl->sw = 1;

    ctrl->reset(ctrl);
}

static void touch_reset(TOUCH_CTRL_t *ctrl)
{
    // 防止卡死
    uint8_t tx_data[] = {0xD0, 0x00, 0x00};
    uint8_t rx_data[3];
    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->writeRead(tx_data, rx_data, 3);
    ctrl->set_cs(ctrl, GPIO_PIN_SET);
}

static void touch_read_adc(TOUCH_CTRL_t *ctrl, uint8_t *cmd, uint16_t *axis)
{
    uint16_t buf[5];
    uint8_t tx_data[3] = {*cmd, 0x00, 0x00};
    uint8_t rx_data[3];

    for (uint8_t i = 0; i < 5; i++)
    {
        ctrl->set_cs(ctrl, GPIO_PIN_RESET);

        ctrl->spix_api->writeRead(tx_data, rx_data, 3);
        // printf("rx_data[0] = 0x%02X, rx_data[1] = 0x%02X\n", rx_data[0], rx_data[1]);
        buf[i] = (rx_data[1] << 8) | rx_data[2];
        buf[i] &= ~(1 << 15);
        buf[i] >>= 3;

        ctrl->set_cs(ctrl, GPIO_PIN_SET);
    }

    // 排序
    for (uint16_t i = 0; i < 5; i++)
    {
        for (uint16_t j = i; j < 5; j++)
        {
            if (buf[i] > buf[j])
            {
                uint16_t temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }
    // 中值滤波
    *axis = (buf[1] + buf[2] + buf[3]) / 3;
}

static uint8_t touch_read_adc_xy(TOUCH_CTRL_t *ctrl, uint16_t *x, uint16_t *y) // 连续读取两次
{
    uint16_t x0, x1, y0, y1;
    ctrl->read_adc(ctrl, &ctrl->cmd_x, &x0);
    ctrl->read_adc(ctrl, &ctrl->cmd_y, &y0);
    ctrl->read_adc(ctrl, &ctrl->cmd_x, &x1);
    ctrl->read_adc(ctrl, &ctrl->cmd_y, &y1);

    if ((abs(x0 - x1) < ctrl->error) && (abs(y0 - y1) < ctrl->error))
    {
        *x = (x0 + x1) / 2;
        *y = (y0 + y1) / 2;
        return 1;
    }
    return 0;
}

static uint8_t touch_adjust(TOUCH_CTRL_t *ctrl) // 校准
{
    // uint8_t adjust_ok = 0;
    // ctrl->flash->api->read(ctrl->flash->ctrl, 0, &adjust_ok, 1);
    // if (adjust_ok == 0xAA)
    // {
    //     ctrl->flash->api->read(ctrl->flash->ctrl,
    //                            1,
    //                            (uint8_t *)ctrl->adjust_value[0],
    //                            sizeof(ctrl->adjust_value));
    //     ctrl->adjust_flag = 1;
    //     return 1;
    // }

    FIL file;
    char line[32];
    float *ptr = ctrl->adjust_value[0];

    if (f_open(&file, "0:touch.txt", FA_READ) == FR_OK)
    {
        for (uint8_t i = 0; i < 6; i++)
        {
            if (f_gets(line, sizeof(line), &file) != NULL)
            {
                sscanf(line, "%f", ptr + i);
            }
        }
        f_close(&file);
        ctrl->adjust_flag = 1;
        return 1;
    }

    // 保存当前方向
    uint8_t dir = ctrl->lcdx->api->get_dir(ctrl->lcdx->ctrl);
    ctrl->lcdx->api->set_dir(ctrl->lcdx->ctrl, 1); // 默认1为校准初始方向

    // 依次绘制n个点
    uint16_t x, y;
    float axis[3][5];

    const float rate = 0.05f;
    uint16_t w = ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl);
    uint16_t h = ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl);
    float point[2][5] = {
        {
            w * rate,
            w * (1 - rate),
            w * rate,
            w * (1 - rate),
            w * 0.5f,
        },
        {
            h * rate,
            h * rate,
            h * (1 - rate),
            h * (1 - rate),
            h * 0.5f,
        },
    };

    for (uint8_t p = 0; p < 5; p++)
    {
        for (int8_t i = -5; i < 6; i++) // 绘制十字
        {
            ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, point[0][p] + i, point[1][p], 0xffff);
            ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, point[0][p], point[1][p] + i, 0xffff);
        }

        while (1)
        {
            while (!ctrl->touch_state) // 等待按下
            {
                vTaskDelay(10);
            }
            vTaskDelay(10);

            if (ctrl->read_adc_xy(ctrl, &x, &y))
            {
                axis[0][p] = 4095.f - x;
                axis[1][p] = 4095.f - y;
                axis[2][p] = 1.f;
                break;
            }
        }

        while (ctrl->touch_state) // 等待松开
        {
            vTaskDelay(10);
        }
        vTaskDelay(10);

        for (int8_t i = -5; i < 6; i++) // 清除十字
        {
            ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, point[0][p] + i, point[1][p], RED);
            ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, point[0][p], point[1][p] + i, RED);
        }
    }

    // 最小二乘法拟合
    float axis_T[5][3];
    float temp0[3][3];
    float temp0_inv[3][3];
    float temp1[2][3];
    matrix_api.transpose(axis[0], 3, 5, axis_T[0]);
    matrix_api.multiply(axis[0], 3, 5, axis_T[0], 3, temp0[0]);
    matrix_api.inverse_lu(temp0[0], 3, temp0_inv[0]);
    matrix_api.multiply(point[0], 2, 5, axis_T[0], 3, temp1[0]);
    matrix_api.multiply(temp1[0], 2, 3, temp0_inv[0], 3, ctrl->adjust_value[0]);

    // matrix_api.printf(ctrl->adjust_value[0], 2, 3);

    // adjust_ok = 0xAA; // 已校准标志
    // ctrl->flash->api->erase_sector(ctrl->flash->ctrl, 0);
    // ctrl->flash->api->write(ctrl->flash->ctrl, 0, &adjust_ok, 1);
    // ctrl->flash->api->write(ctrl->flash->ctrl, 1, (uint8_t *)ctrl->adjust_value[0], sizeof(ctrl->adjust_value));
    // ctrl->adjust_flag = 1;

    ptr = ctrl->adjust_value[0];
    if (f_open(&file, "0:touch.txt", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK)
    {
        for (uint8_t i = 0; i < 6; i++)
        {
            int len = sprintf(line, "%f\n", *(ptr + i));
            UINT bw;
            f_write(&file, line, len, &bw);
        }
        f_close(&file);
        ctrl->adjust_flag = 1;
    }

    // 恢复方向
    ctrl->lcdx->api->set_dir(ctrl->lcdx->ctrl, dir);

    return 1;
}

static uint8_t touch_get_xy(TOUCH_CTRL_t *ctrl, uint16_t *x, uint16_t *y)
{
    if (!ctrl->adjust_flag)
        return 0;

    if (!ctrl->touch_state)
        return 0;

    uint16_t x0, y0;
    if (!ctrl->read_adc_xy(ctrl, &x0, &y0))
        return 0;

    x0 = 4095.f - x0;
    y0 = 4095.f - y0;

    *x = ctrl->adjust_value[0][0] * x0 + ctrl->adjust_value[0][1] * y0 + ctrl->adjust_value[0][2];
    *y = ctrl->adjust_value[1][0] * x0 + ctrl->adjust_value[1][1] * y0 + ctrl->adjust_value[1][2];

    uint16_t temp = *x;
    switch (ctrl->lcdx->api->get_dir(ctrl->lcdx->ctrl))
    {
    case 0:
        *x = ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl) - 1 - *y;
        *y = temp;
        break;
    case 1: // 默认1为校准初始方向
        break;
    case 2:
        *x = *y;
        *y = ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl) - 1 - temp;
        break;
    case 3:
        *x = ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl) - 1 - *x;
        *y = ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl) - 1 - *y;
        break;
    default:
        break;
    }

    // 可能会有负值或者超出屏幕的值
    // *x = (int16_t)(*x) < 0 ? 0 : (*x >= ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl) ? ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl) - 1 : *x);
    // *y = (int16_t)(*y) < 0 ? 0 : (*y >= ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl) ? ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl) - 1 : *y);
    if (*x >= ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl))
        return 0;
    if (*y >= ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl))
        return 0;

    // drv_uart7.printf("x=%d,y=%d, xr=%d,yr=%d\n", x, y, x0, y0);

    return 1;
}

#if 0
static uint8_t touch_adjust2(TOUCH_CTRL_t *ctrl)
{
    uint16_t w = ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl);
    uint16_t h = ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl);

    for (uint8_t row = 0; row < 4; row++)
    {
        for (uint8_t col = 0; col < 4; col++)
        {
            scr_x[row][col] = radio_x[col] * w;
            scr_y[row][col] = radio_y[row] * h;

            for (int8_t i = -5; i < 6; i++) // 绘制十字
            {
                ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, scr_x[row][col] + i, scr_y[row][col], 0xe007);
                ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, scr_x[row][col], scr_y[row][col] + i, 0xe007);
            }

            while (1)
            {
                while (!ctrl->touch_state) // 等待按下
                    ;
                HAL_Delay(10);

                uint16_t x_adc, y_adc;
                if (ctrl->read_adc_xy(ctrl, &x_adc, &y_adc))
                {
                    adc_x[row][col] = 4095.f - x_adc;
                    adc_y[row][col] = 4095.f - y_adc;
                    break;
                }
            }

            while (ctrl->touch_state) // 等待松开
                ;
            HAL_Delay(10);

            for (int8_t i = -5; i < 6; i++) // 清除十字
            {
                ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, scr_x[row][col] + i, scr_y[row][col], RED);
                ctrl->lcdx->api->draw_point(ctrl->lcdx->ctrl, scr_x[row][col], scr_y[row][col] + i, RED);
            }
        }
    }

    ctrl->adjust_flag = 1;

    return 1;
}

// 双线性插值
static uint8_t touch_get_xy2(TOUCH_CTRL_t *ctrl, uint16_t *x, uint16_t *y)
{
    if (!ctrl->touch_state)
        return 0;

    if (!ctrl->adjust_flag)
        return 0;

    uint16_t x_adc, y_adc;
    if (!ctrl->read_adc_xy(ctrl, &x_adc, &y_adc))
        return 0;

    x_adc = 4095.f - x_adc;
    y_adc = 4095.f - y_adc;

    // 线性插值
    uint8_t row = 0, col = 0;

    if (x_adc < adc_x[0][0])
        col = 0;
    else if (x_adc >= adc_x[0][3])
        col = 2;
    else
    {
        for (col = 0; col < 3; col++)
        {
            if (x_adc >= adc_x[0][col] && x_adc <= adc_x[0][col + 1])
                break;
        }
    }

    if (y_adc < adc_y[0][0])
        row = 0;
    else if (y_adc >= adc_y[3][0])
        row = 2;
    else
    {
        for (row = 0; row < 3; row++)
        {
            if (y_adc >= adc_y[row][0] && y_adc <= adc_y[row + 1][0])
                break;
        }
    }

    float x1 = adc_x[row][col], y1 = adc_y[row][col];
    float x2 = adc_x[row][col + 1], y2 = adc_y[row][col + 1];
    float x3 = adc_x[row + 1][col], y3 = adc_y[row + 1][col];
    float x4 = adc_x[row + 1][col + 1], y4 = adc_y[row + 1][col + 1];

    float sx1 = scr_x[row][col], sy1 = scr_y[row][col];
    float sx2 = scr_x[row][col + 1], sy2 = scr_y[row][col + 1];
    float sx3 = scr_x[row + 1][col], sy3 = scr_y[row + 1][col];
    float sx4 = scr_x[row + 1][col + 1], sy4 = scr_y[row + 1][col + 1];

    // X方向插值（顶部和底部）
    float x_top = sx1 + (sx2 - sx1) * (x_adc - x1) / (x2 - x1);
    float x_bottom = sx3 + (sx4 - sx3) * (x_adc - x3) / (x4 - x3);

    // Y方向插值（使用y_adc）
    float screen_x = x_top + (x_bottom - x_top) * (y_adc - y1) / (y3 - y1);

    // Y坐标的插值（先左右，再上下）
    float y_left = sy1 + (sy3 - sy1) * (y_adc - y1) / (y3 - y1);
    float y_right = sy2 + (sy4 - sy2) * (y_adc - y2) / (y4 - y2);
    float screen_y = y_left + (y_right - y_left) * (x_adc - x1) / (x2 - x1);

    *x = (uint16_t)(screen_x + 0.5f);
    *y = (uint16_t)(screen_y + 0.5f);
    *x = ((int16_t)*x < 0) ? 0 : (*x >= ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl) ? ctrl->lcdx->api->get_width(ctrl->lcdx->ctrl) - 1 : *x);
    *y = ((int16_t)*y < 0) ? 0 : (*y >= ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl) ? ctrl->lcdx->api->get_height(ctrl->lcdx->ctrl) - 1 : *y);

    ctrl->touch_state = 0;

    return 1;
}
#endif

static void touch_scan(TOUCH_CTRL_t *ctrl)
{
    if (!ctrl->sw)
        return;

    ctrl->crr_time_ms++;
    if (ctrl->crr_time_ms < ctrl->scan_time_ms)
        return;
    ctrl->crr_time_ms = 0;

    GPIO_PinState level;
    level = HAL_GPIO_ReadPin(ctrl->io_irq.port, ctrl->io_irq.pin);

    if (level == GPIO_PIN_RESET)
        ctrl->touch_state = 1;
    else
        ctrl->touch_state = 0;
}

static uint8_t touch_get_state(TOUCH_CTRL_t *ctrl)
{
    return ctrl->touch_state;
}

static uint8_t touch_get_switch(TOUCH_CTRL_t *ctrl)
{
    return ctrl->sw;
}

static void touch_set_switch(TOUCH_CTRL_t *ctrl, uint8_t sw)
{
    ctrl->sw = sw;
}

static void touch_set_cs(TOUCH_CTRL_t *ctrl, GPIO_PinState PinState)
{
    // HAL_GPIO_WritePin(ctrl->io_cs.port, ctrl->io_cs.pin, PinState);
    if (PinState != GPIO_PIN_RESET)
        ctrl->io_cs.port->BSRR = ctrl->io_cs.pin;
    else
        ctrl->io_cs.port->BSRR = (uint32_t)ctrl->io_cs.pin << 16U;
}

/////////////////////////////////////////////////////////////////////
static TOUCH_CTRL_t touch_ctrl = {
    .cmd_x = 0x90,
    .cmd_y = 0xD0,

    .spix_api = &spi2_api,
    .lcdx = &lcd,
    .flash = &w25qxx,

    .io_irq = {
        .port = GPIOC,
        .pin = GPIO_PIN_5,
    },
    .io_cs = {
        .port = GPIOB,
        .pin = GPIO_PIN_12,
    },

    .touch_state = 0,
    .adjust_flag = 0,
    .sw = 0,

    .scan_time_ms = 10,
    .crr_time_ms = 0,
    .error = 50,
    .adjust_value = {{0, 0, 0}, {0, 0, 0}},

    .read_adc = touch_read_adc,
    .read_adc_xy = touch_read_adc_xy,
    .reset = touch_reset,
    .set_cs = touch_set_cs,
};

static TOUCH_API_t touch_api = {
    .init = touch_init,
    .adjust = touch_adjust,
    .get_xy = touch_get_xy,
    .scan = touch_scan,
    .get_state = touch_get_state,
    .get_switch = touch_get_switch,
    .set_switch = touch_set_switch,
    .set_cs = touch_set_cs,
};

const TOUCH_t touch = {
    .ctrl = &touch_ctrl,
    .api = &touch_api,
};
