#ifndef __ILI9341_REG_H__
#define __ILI9341_REG_H__

#include <stdint.h>

typedef struct
{
    uint8_t reg;
    uint8_t data[16];
    uint8_t data_size;
} ILI9341_REG_DATA_t;

extern const ILI9341_REG_DATA_t ili9341_reg_data[];

#endif
