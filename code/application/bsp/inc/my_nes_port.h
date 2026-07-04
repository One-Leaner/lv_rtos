#ifndef __MY_NES_PORT_H__
#define __MY_NES_PORT_H__

#include <stdint.h>

/* NES按键定义 */
#define NES_PAD_A 0x01
#define NES_PAD_B 0x02
#define NES_PAD_SELECT 0x04
#define NES_PAD_START 0x08
#define NES_PAD_UP 0x10
#define NES_PAD_DOWN 0x20
#define NES_PAD_LEFT 0x40
#define NES_PAD_RIGHT 0x80

typedef enum
{
    NES_STATE_STOPPED = 0,
    NES_STATE_RUNNING,
    NES_STATE_PAUSED,
} NES_STATE_t;

typedef struct NES_CTRL NES_CTRL_t;

typedef struct
{
    uint8_t (*start)(NES_CTRL_t *, const char *);
    void (*refresh)(NES_CTRL_t *);
    void (*pause)(NES_CTRL_t *);
    void (*resume)(NES_CTRL_t *);
    void (*stop)(NES_CTRL_t *);
    void (*set_pad)(NES_CTRL_t *, uint8_t, uint8_t);
    void *(*get_workframe)(NES_CTRL_t *);
    uint16_t (*get_height)(NES_CTRL_t *);
    uint16_t (*get_width)(NES_CTRL_t *);
    NES_STATE_t (*get_state)(NES_CTRL_t *);
} NES_API_t;

/* NES渲染器实例 */
typedef struct
{
    NES_CTRL_t *ctrl;
    NES_API_t *api;
} NES_t;

/* 外部声明 */
extern const NES_t nes;

#endif
