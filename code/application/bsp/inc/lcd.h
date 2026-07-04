#ifndef __LCD_H__
#define __LCD_H__

#include "gpio.h"
#include "spi.h"

#define RED 0xf800 // 红色

#define USE_HORIZONTAL 2 // 1-排针在上竖屏  定义液晶屏顺时针旋转方向 	0-0度旋转，1-90度旋转，2-180度旋转，3-270度旋转
// 定义LCD的尺寸
#define LCD_W 240
#define LCD_H 320

typedef enum
{
    LCD_DATA = 0,
    LCD_REG,
} LCD_DATA_REG_SELECT_t;

typedef struct
{
    uint16_t x_start;
    uint16_t x_end;
    uint16_t y_start;
    uint16_t y_end;
} LCD_AREA_t;

typedef struct LCD_CTRL LCD_CTRL_t;

typedef struct
{
    void (*init)(LCD_CTRL_t *);
    void (*set_windows)(LCD_CTRL_t *, LCD_AREA_t);
    void (*set_dir)(LCD_CTRL_t *, uint8_t);
    void (*clear)(LCD_CTRL_t *, LCD_AREA_t, uint16_t);
    void (*write)(LCD_CTRL_t *, const uint8_t *, uint16_t, LCD_DATA_REG_SELECT_t, uint8_t);
    void (*draw_point)(LCD_CTRL_t *, uint16_t, uint16_t, uint16_t);
    uint8_t (*get_dir)(LCD_CTRL_t *);
    uint16_t (*get_width)(LCD_CTRL_t *);
    uint16_t (*get_height)(LCD_CTRL_t *);
    uint8_t (*get_switch)(LCD_CTRL_t *);
    void (*set_cs)(LCD_CTRL_t *, GPIO_PinState);
    void (*set_light)(LCD_CTRL_t *, uint8_t);
    void (*set_datawidth)(LCD_CTRL_t *, SPI_DATAWIDTH_t);
    void (*notify)(LCD_CTRL_t *);
} LCD_API_t;

typedef struct
{
    LCD_CTRL_t *ctrl;
    LCD_API_t *api;
} LCD_t;

extern const LCD_t lcd;

#endif

//=========================================电源接线================================================//
//     LCD模块                STM32单片机
//      VCC          接          3.3V      //电源
//      GND          接          GND          //电源地
//=======================================液晶屏数据线接线==========================================//
// 本模块默认数据总线类型为SPI总线
//     LCD模块                STM32单片机
//    SDI(MOSI)      接          PB15         //液晶屏SPI总线数据写信号
//    SDO(MISO)      接          PB14         //液晶屏SPI总线数据读信号，如果不需要读，可以不接线
//=======================================液晶屏控制线接线==========================================//
//     LCD模块 					      STM32单片机
//       LED         接          PB5          //液晶屏背光控制信号，如果不需要控制，接5V或3.3V
//       SCK         接          PB13         //液晶屏SPI总线时钟信号
//      DC/RS        接          PB8         //液晶屏数据/命令控制信号
//       RST         接          PB7         //液晶屏复位控制信号
//       CS          接          PB6         //液晶屏片选控制信号
//=========================================触摸屏触接线=========================================//
