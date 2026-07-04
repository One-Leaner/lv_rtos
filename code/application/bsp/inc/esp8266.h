#ifndef __ESP8266_H__
#define __ESP8266_H__

#include <stdint.h>

// 固件版本序号 1471

typedef enum
{
    ESP8266_CWMODE_INVALID = 0,
    ESP8266_CWMODE_STA,
    ESP8266_CWMODE_AP,
    ESP8266_CWMODE_STA_AP,
} ESP8266_CWMODE_t;

typedef enum
{
    ESP8266_CIPMODE_SINGLE = 0,  // 普通模式（返回接收数据有包头 +IPD,n，n为数据长度）
    ESP8266_CIPMODE_TRANSPARENT, // 透传模式（esp8266返回接收的数据无包头 +IPD,n，n为数据长度）
} ESP8266_CIPMODE_t;

typedef struct ESP8266_CTRL ESP8266_CTRL_t; // 对外隐藏成员

typedef struct
{
    uint8_t (*scanf)(ESP8266_CTRL_t *, const char *, ...);
    void (*set_ap_ip)(ESP8266_CTRL_t *, char *, char *, char *);

    void (*init)(ESP8266_CTRL_t *);
    void (*view_tcp_info)(ESP8266_CTRL_t *);
    void (*tcp_config)(ESP8266_CTRL_t *, ESP8266_CWMODE_t);
    void (*tcp_connect_server)(ESP8266_CTRL_t *);
    void (*tcp_create_server)(ESP8266_CTRL_t *);
    void (*tcp_close_server)(ESP8266_CTRL_t *);
    void (*tcp_send)(ESP8266_CTRL_t *, uint8_t, const char *, ...);

    void (*view_mqtt_info)(ESP8266_CTRL_t *);
    void (*mqtt_config)(ESP8266_CTRL_t *, ESP8266_CWMODE_t);
    void (*mqtt_connect)(ESP8266_CTRL_t *);
    void (*mqtt_disconnect)(ESP8266_CTRL_t *);
    void (*mqtt_subscribe)(ESP8266_CTRL_t *, char *, uint8_t);
    void (*mqtt_unsubscribe)(ESP8266_CTRL_t *, char *);
    void (*mqtt_publish)(ESP8266_CTRL_t *, char *, char *, uint8_t, uint8_t);

#if 0
    // 这个固件版本暂不支持
    void (*mqtt_set_client_id)(ESP8266_CTRL_t *, char *);
    void (*mqtt_set_username)(ESP8266_CTRL_t *, char *);
    void (*mqtt_set_password)(ESP8266_CTRL_t *, char *);
#endif

    void (*notify)(ESP8266_CTRL_t *);
} ESP8266_API_t;

typedef struct
{
    ESP8266_CTRL_t *ctrl;
    ESP8266_API_t *api;
} ESP8266_t;

extern const ESP8266_t esp8266;

#endif