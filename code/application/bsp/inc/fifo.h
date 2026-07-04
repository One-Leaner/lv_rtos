#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdint.h>

typedef struct FIFO_CTRL FIFO_CTRL_t;

typedef struct
{
    void (*init)(FIFO_CTRL_t *, uint8_t);
    void (*free)(FIFO_CTRL_t *);
    uint8_t (*put)(FIFO_CTRL_t *, void *, uint16_t);
    uint8_t (*get)(FIFO_CTRL_t *, void *const, uint16_t);
    int (*get_crr_num)(FIFO_CTRL_t *ctrl);
} FIFO_API_t;

typedef struct
{
    FIFO_CTRL_t *ctrl;
    FIFO_API_t *api;
} FIFO_t;

extern const FIFO_t fifo;

#endif
