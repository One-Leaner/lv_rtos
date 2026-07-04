#ifndef __TOUCH_H__
#define __TOUCH_H__

// 注意：这个lcd（ili9341）的VCC一定要接5V，否则触摸数据会出大问题，3.3V供电会导致远离原点的位置误差很大！！！！
// 这个逼模块加了个LDO，输入的3.3V经过LDO输出，电压应该是低于3.3V的
// spi频率不要太高
// 发送命令后要给个延时，等待adc转换。不过也就几微秒，程序运行时可以达到，看情况加

#include "gpio.h"

typedef struct TOUCH_CTRL TOUCH_CTRL_t;

typedef struct
{
    void (*init)(TOUCH_CTRL_t *, uint16_t);
    uint8_t (*adjust)(TOUCH_CTRL_t *);
    uint8_t (*get_xy)(TOUCH_CTRL_t *, uint16_t *, uint16_t *);
    void (*scan)(TOUCH_CTRL_t *);
    uint8_t (*get_state)(TOUCH_CTRL_t *);
    uint8_t (*get_switch)(TOUCH_CTRL_t *);
    void (*set_switch)(TOUCH_CTRL_t *, uint8_t);
    void (*set_cs)(TOUCH_CTRL_t *, GPIO_PinState);

} TOUCH_API_t;

typedef struct
{
    TOUCH_CTRL_t *ctrl;
    TOUCH_API_t *api;
} TOUCH_t;

extern const TOUCH_t touch;

#endif
