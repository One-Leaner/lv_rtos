#ifndef __UI_H__
#define __UI_H__

#include "lvgl.h"

enum UI_ID
{
    UI_ID_MENU = 0,
    UI_ID_SETTING,
    UI_ID_RETURN,
    UI_ID_FILE,
    UI_ID_VOLUME,
    UI_ID_SOUND,
    UI_ID_BRIGHTNESS,
    UI_ID_USB,
    UI_ID_IAP,
    UI_ID_OTA,
    UI_ID_NES_KEY,
    UI_ID_NES_RUN,
};

typedef struct
{
    uint8_t id;
    lv_obj_t *next_scr;
    void *other_data;
} user_data_t;

typedef struct UI_CTRL UI_CTRL_t;

typedef struct
{
    void (*init)(UI_CTRL_t *);
    uint8_t (*get_flush)(UI_CTRL_t *);
    void (*set_flush)(UI_CTRL_t *, uint8_t);
    void (*set_usb_flag)(UI_CTRL_t *, uint8_t);
    uint8_t (*get_usb_flag)(UI_CTRL_t *);
    void (*set_iap_flag)(UI_CTRL_t *, uint8_t);
    uint8_t (*get_iap_flag)(UI_CTRL_t *);
    void (*set_ota_flag)(UI_CTRL_t *, uint8_t);
    uint8_t (*get_ota_flag)(UI_CTRL_t *);
    void (*back_menu)(UI_CTRL_t *);
    void (*update)(UI_CTRL_t *);
} UI_API_t;

typedef struct
{
    UI_CTRL_t *ctrl;
    UI_API_t *api;
} UI_t;

extern const UI_t ui;

#endif
