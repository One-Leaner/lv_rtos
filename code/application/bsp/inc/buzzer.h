#ifndef __BUZZER_H__
#define __BUZZER_H__

#include <stdint.h>

typedef struct BUZZER_CTRL BUZZER_CTRL_t;

typedef struct
{
    void (*set_volume)(BUZZER_CTRL_t *ctrl, uint8_t volume);
    void (*on)(BUZZER_CTRL_t *ctrl);
    void (*off)(BUZZER_CTRL_t *ctrl);
} BUZZER_API_t;

typedef struct
{
    BUZZER_CTRL_t *ctrl;
    BUZZER_API_t *api;
} BUZZER_t;

extern const BUZZER_t buzzer;

#endif