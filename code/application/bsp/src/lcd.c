#include "lcd.h"
#include "ili9341_reg.h"
#include "tim.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdio.h>

// LCD重要参数集
typedef struct
{
    uint16_t width;  // LCD 宽度
    uint16_t height; // LCD 高度
    uint8_t id;      // LCD ID
    uint8_t dir;     // 横屏还是竖屏控制：0，竖屏；1，横屏。
    uint8_t wramcmd; // 开始写gram指令
    uint8_t rdamcmd; // 开始读gram指令
    uint8_t setxcmd; // 设置x坐标指令
    uint8_t setycmd; // 设置y坐标指令
} LCD_ARGS_t;

struct LCD_CTRL
{
    TaskHandle_t notify_handle; // 用于任务通知
    SPI_DATAWIDTH_t datawidth;

    uint8_t *const p_buf;
    const uint16_t p_buf_size;
    const uint16_t buf_size;

    const SPI_API_t *spix_api;
    const TIM_API_t *timx_api;

    const GPIO_Pin_t io_cs;
    const GPIO_Pin_t io_dc;
    const GPIO_Pin_t io_rst;

    const ILI9341_REG_DATA_t *reg_data;
    uint8_t const reg_size;
    LCD_ARGS_t lcd_args;
    volatile uint8_t sw; // 开关

    uint8_t light; // 亮度

    void (*set_windows)(LCD_CTRL_t *, LCD_AREA_t);
    void (*set_dir)(LCD_CTRL_t *, uint8_t);
    void (*reset)(LCD_CTRL_t *);
    void (*clear)(LCD_CTRL_t *, LCD_AREA_t, uint16_t);
    void (*write)(LCD_CTRL_t *, const uint8_t *, uint16_t, LCD_DATA_REG_SELECT_t, uint8_t);
    void (*set_dc)(LCD_CTRL_t *, GPIO_PinState);
    void (*set_rst)(LCD_CTRL_t *, GPIO_PinState);
    void (*set_cs)(LCD_CTRL_t *, GPIO_PinState);
    void (*set_datawidth)(LCD_CTRL_t *, SPI_DATAWIDTH_t);
};

static void lcd_init(LCD_CTRL_t *ctrl)
{
    ctrl->timx_api->set_duty(1, 100);
    // ctrl->timx_api->set_freq(2000);//共用定时器，内部不要调用函数设置频率
    ctrl->timx_api->pwm_start(1);

    ctrl->lcd_args.setxcmd = 0x2A;
    ctrl->lcd_args.setycmd = 0x2B;
    ctrl->lcd_args.wramcmd = 0x2C;
    ctrl->lcd_args.rdamcmd = 0x2E;

    //*************2.8inch ILI9341初始化**********//
    ctrl->reset(ctrl); // LCD 复位

    ctrl->set_datawidth(ctrl, SPI_DATAWIDTH_8BIT); // 设置数据宽度为8位，用来发送命令
    if (ctrl->reg_data != NULL)
    {
        for (uint8_t i = 0; i < ctrl->reg_size; i++)
        {
            ctrl->write(ctrl, &ctrl->reg_data[i].reg, 1, LCD_REG, 1);
            ctrl->write(ctrl, ctrl->reg_data[i].data, ctrl->reg_data[i].data_size, LCD_DATA, 1);
        }
    }

    uint8_t reg[] = {0x11, 0x29};

    ctrl->write(ctrl, &reg[0], 1, LCD_REG, 1); // Exit Sleep
    vTaskDelay(120);
    ctrl->set_dir(ctrl, USE_HORIZONTAL);       // 设置ili9341显示方向
    ctrl->write(ctrl, &reg[1], 1, LCD_REG, 1); // display on

    LCD_AREA_t area = {0, ctrl->lcd_args.width - 1, 0, ctrl->lcd_args.height - 1};
    ctrl->set_windows(ctrl, area);
    ctrl->clear(ctrl, area, RED); // 清全屏红色
    //*************2.8inch ILI9341初始化**********//

    ctrl->sw = 1;
}

static void lcd_set_windows(LCD_CTRL_t *ctrl, LCD_AREA_t area)
{
    if (area.x_end >= ctrl->lcd_args.width)
        area.x_end = ctrl->lcd_args.width - 1;
    if (area.y_end >= ctrl->lcd_args.height)
        area.y_end = ctrl->lcd_args.height - 1;

    uint8_t temp[8];

    temp[0] = (uint8_t)(area.x_start >> 8);
    temp[1] = (uint8_t)(area.x_start & 0xff);
    temp[2] = (uint8_t)(area.x_end >> 8);
    temp[3] = (uint8_t)(area.x_end & 0xff);
    temp[4] = (uint8_t)(area.y_start >> 8);
    temp[5] = (uint8_t)(area.y_start & 0xff);
    temp[6] = (uint8_t)(area.y_end >> 8);
    temp[7] = (uint8_t)(area.y_end & 0xff);

    ctrl->write(ctrl, &ctrl->lcd_args.setxcmd, 1, LCD_REG, 1);
    ctrl->write(ctrl, &temp[0], 4, LCD_DATA, 1);
    ctrl->write(ctrl, &ctrl->lcd_args.setycmd, 1, LCD_REG, 1);
    ctrl->write(ctrl, &temp[4], 4, LCD_DATA, 1);

    ctrl->write(ctrl, &ctrl->lcd_args.wramcmd, 1, LCD_REG, 1); // 开始写入GRAM
}

static void lcd_set_dir(LCD_CTRL_t *ctrl, uint8_t dir)
{
    if (dir > 4)
        return;

    uint8_t data = 0;
    uint8_t reg = 0x36;

    ctrl->write(ctrl, &reg, 1, LCD_REG, 1);

    switch (dir) // 以排针在上为正参考。MY：行地址顺序，MX：列地址顺序，MV：行/列交换
    {
    case 0: // 坐标起始点为右下，水平为x轴
        ctrl->lcd_args.width = LCD_W;
        ctrl->lcd_args.height = LCD_H;
        data = (1 << 3); // BGR==1,MY==0,MX==0,MV==0
        break;
    case 1: // 坐标起始点为左下，垂直为x轴
        ctrl->lcd_args.width = LCD_H;
        ctrl->lcd_args.height = LCD_W;
        data = (1 << 3) | (1 << 6) | (1 << 5); // BGR==1,MY==0,MX==1,MV==1
        break;
    case 2: // 坐标起始点为左上，水平为x轴
        ctrl->lcd_args.width = LCD_W;
        ctrl->lcd_args.height = LCD_H;
        data = (1 << 3) | (1 << 7) | (1 << 6); // BGR==1,MY==1,MX==1,MV==0
        break;
    case 3: // 坐标起始点为右上，垂直为x轴
        ctrl->lcd_args.width = LCD_H;
        ctrl->lcd_args.height = LCD_W;
        data = (1 << 3) | (1 << 7) | (1 << 5); // BGR==1,MY==1,MX==0,MV==1
        break;
    default:
        break;
    }

    ctrl->write(ctrl, &data, 1, LCD_DATA, 1);
    ctrl->lcd_args.dir = dir;
}

static void lcd_clear(LCD_CTRL_t *ctrl, LCD_AREA_t area, uint16_t color)
{
    for (uint16_t i = 0; i < ctrl->p_buf_size; i += 2)
    {
        ctrl->p_buf[i] = (uint8_t)(color >> 8);
        ctrl->p_buf[i + 1] = (uint8_t)(color & 0xff);
    }

    uint16_t width = area.x_end - area.x_start + 1;
    uint16_t height = area.y_end - area.y_start + 1;
    uint32_t size = width * height;

    if (ctrl->datawidth == SPI_DATAWIDTH_8BIT)
        size *= 2;

    for (uint32_t i = 0; i < size / ctrl->p_buf_size; i++)
    {
        ctrl->write(ctrl, ctrl->p_buf, ctrl->p_buf_size, LCD_DATA, 1);
    }
}

static void lcd_reset(LCD_CTRL_t *ctrl)
{
    ctrl->set_rst(ctrl, GPIO_PIN_RESET);
    vTaskDelay(100);
    ctrl->set_rst(ctrl, GPIO_PIN_SET);
    vTaskDelay(100);
}

static void lcd_write(LCD_CTRL_t *ctrl, const uint8_t *data, uint16_t size, LCD_DATA_REG_SELECT_t sel, uint8_t wait)
{
    if (wait) // 等待上一次发送完成，防止覆盖数据
    {
        while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
            ;
    }

    if (sel == LCD_DATA)
        ctrl->set_dc(ctrl, GPIO_PIN_SET);
    else if (sel == LCD_REG)
        ctrl->set_dc(ctrl, GPIO_PIN_RESET);
    else
        return;

    ctrl->set_cs(ctrl, GPIO_PIN_RESET);
    ctrl->spix_api->write(data, size);
    // while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
    //     ;
    // ctrl->set_cs(ctrl, GPIO_PIN_SET); //中断回调中调用
}

static void lcd_draw_point(LCD_CTRL_t *ctrl, uint16_t x, uint16_t y, uint16_t color)
{
    LCD_AREA_t area = {x, x, y, y};
    ctrl->set_windows(ctrl, area);
    color = (color >> 8) | (color << 8);
    ctrl->write(ctrl, (uint8_t *)&color, 2, LCD_DATA, 1);
}

static uint8_t lcd_get_dir(LCD_CTRL_t *ctrl)
{
    return ctrl->lcd_args.dir;
}

static uint16_t lcd_get_width(LCD_CTRL_t *ctrl)
{
    return ctrl->lcd_args.width;
}

static uint16_t lcd_get_height(LCD_CTRL_t *ctrl)
{
    return ctrl->lcd_args.height;
}

static uint8_t lcd_get_switch(LCD_CTRL_t *ctrl)
{
    return ctrl->sw;
}

static void lcd_set_cs(LCD_CTRL_t *ctrl, GPIO_PinState PinState)
{
    // HAL_GPIO_WritePin(ctrl->io_cs.port, ctrl->io_cs.pin, PinState);

    if (PinState != GPIO_PIN_RESET)
        ctrl->io_cs.port->BSRR = ctrl->io_cs.pin;
    else
        ctrl->io_cs.port->BSRR = (uint32_t)ctrl->io_cs.pin << 16U;
}

static void lcd_set_dc(LCD_CTRL_t *ctrl, GPIO_PinState PinState)
{
    // HAL_GPIO_WritePin(ctrl->io_dc.port, ctrl->io_dc.pin, PinState);

    if (PinState != GPIO_PIN_RESET)
        ctrl->io_dc.port->BSRR = ctrl->io_dc.pin;
    else
        ctrl->io_dc.port->BSRR = (uint32_t)ctrl->io_dc.pin << 16U;
}

static void lcd_set_rst(LCD_CTRL_t *ctrl, GPIO_PinState PinState)
{
    // HAL_GPIO_WritePin(ctrl->io_rst.port, ctrl->io_rst.pin, PinState);

    if (PinState != GPIO_PIN_RESET)
        ctrl->io_rst.port->BSRR = ctrl->io_rst.pin;
    else
        ctrl->io_rst.port->BSRR = (uint32_t)ctrl->io_rst.pin << 16U;
}

// 设置亮度
static void lcd_set_light(LCD_CTRL_t *ctrl, uint8_t light)
{
    /*0-100*/
    ctrl->light = light;
    ctrl->timx_api->set_duty(1, light);
}

static void lcd_set_datawidth(LCD_CTRL_t *ctrl, SPI_DATAWIDTH_t datawidth)
{
    // ctrl->notify_handle = xTaskGetCurrentTaskHandle();
    // ulTaskNotifyTake(pdTRUE, 1000);

    while (ctrl->spix_api->get_state() != HAL_SPI_STATE_READY)
        ;

    ctrl->spix_api->set_datawidth(datawidth);
    ctrl->datawidth = datawidth;
}

static void lcd_notify(LCD_CTRL_t *ctrl)
{
    if (ctrl->notify_handle != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // 中断中调用，发送通知，但是在高频中断中会导致系统卡顿
        // 如果唤醒了更高优先级的任务，xHigherPriorityTaskWoken会被设为pdTRUE
        vTaskNotifyGiveFromISR(ctrl->notify_handle, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken)
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

///////////////////////////////////////////////////////
static uint8_t lcd_buf[240 * 2];

static LCD_CTRL_t lcd_ctrl = {
    .notify_handle = NULL,

    .p_buf = lcd_buf,
    .p_buf_size = sizeof(lcd_buf),
    .spix_api = &spi1_api,
    .timx_api = &tim3_api,
    .io_cs = {
        GPIOA,
        GPIO_PIN_12,
    },
    .io_dc = {
        GPIOB,
        GPIO_PIN_1,
    },
    .io_rst = {
        GPIOB,
        GPIO_PIN_0,
    },
    .reg_data = ili9341_reg_data,
    .reg_size = 20,
    .lcd_args = {0},
    .sw = 0, // 开关

    .set_windows = lcd_set_windows,
    .set_dir = lcd_set_dir,
    .clear = lcd_clear,
    .reset = lcd_reset,
    .write = lcd_write,
    .set_dc = lcd_set_dc,
    .set_rst = lcd_set_rst,
    .set_cs = lcd_set_cs,
    .set_datawidth = lcd_set_datawidth,
};
static LCD_API_t lcd_api = {
    .init = lcd_init,
    .set_windows = lcd_set_windows,
    .set_dir = lcd_set_dir,
    .clear = lcd_clear,
    .write = lcd_write,
    .draw_point = lcd_draw_point,
    .get_dir = lcd_get_dir,
    .get_width = lcd_get_width,
    .get_height = lcd_get_height,
    .get_switch = lcd_get_switch,
    .set_cs = lcd_set_cs,
    .set_light = lcd_set_light,
    .set_datawidth = lcd_set_datawidth,
    .notify = lcd_notify,
};

const LCD_t lcd = {
    .ctrl = &lcd_ctrl,
    .api = &lcd_api,
};
