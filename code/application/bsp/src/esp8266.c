#include "esp8266.h"
#include "debug.h"
#include "usart.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "FreeRTOS.h"
#include "task.h"

struct ESP8266_CTRL
{
    TaskHandle_t notify_handle; // 用于任务通知

    const UART_t *uartx;
    uint8_t *const tx_buf;
    const uint16_t tx_buf_size;
    uint8_t *const rx_buf;
    const uint16_t rx_buf_size;

    //////////// tcp配置BEGIN //////////
    char service_ssid[33];     // 最大32个字符
    char service_password[65]; // 最大64个字符
    char service_ip[16];       // 服务器ip 最大15个字符
    uint16_t service_port;     // 服务器端口

    char ap_ssid[33];     // wifi名称 最大32个字符
    char ap_password[65]; // 密码 最大64个字符
    char ap_ip[16];       // 路由ip 最大15个字符
    char ap_gateway[16];  // 路由网关ip 最大15个字符
    char ap_netmask[16];  // 路由子网掩码 最大15个字符
    uint16_t ap_port;     // 路由端口

    char sta_ip[16];      // 客户端ip 最大15个字符
    char sta_gateway[16]; // 网关ip 最大15个字符
    char sta_netmask[16]; // 子网掩码 最大15个字符
    //////////// tcp配置END //////////

    //////////// mqtt配置BEGIN //////////
    uint8_t mqtt_link_id;     // mqtt链接id
    uint8_t mqtt_scheme;      // mqtt协议
    char mqtt_client_id[257]; // mqtt客户端id 最大256个字符
    char mqtt_username[65];   // mqtt用户名 最大64个字符
    char mqtt_password[65];   // mqtt密码 最大64个字符
    uint8_t mqtt_cert_key_id; // 证书密钥id
    uint8_t mqtt_ca_id;       // ca证书id
    char mqtt_path[33];       // 资源路径 最大32个字符

    char mqtt_host[129];    // MQTT broker 域名，可以直接填ip,最大长度：128 字节
    uint16_t mqtt_port;     // MQTT broker 端口
    uint8_t mqtt_reconnect; // 是否自动重连 0：不自动重连 1：自动重连
    //////////// mqtt配置END //////////

    ESP8266_CWMODE_t cwmode;
    ESP8266_CIPMODE_t cipmode;

    void (*printf)(ESP8266_CTRL_t *, const char *, ...);
    uint8_t (*scanf)(ESP8266_CTRL_t *, const char *, ...);
};

static void esp8266_printf(ESP8266_CTRL_t *ctrl, const char *fmt, ...)
{
    va_list args;                                                            // 创建参数列表
    va_start(args, fmt);                                                     // 初始化arg,使其指向第一个可变参数
    int len = vsnprintf((char *)ctrl->tx_buf, ctrl->tx_buf_size, fmt, args); // 将可变参数打印到字符串对应的格式化字符位置，并保存在esp8266_tx_buf中(这里sprintf不可用)
    va_end(args);                                                            // 释放参数列表

    if (len > 0)
    {
        while (ctrl->uartx->api->get_state(ctrl->uartx->ctrl) == HAL_UART_STATE_BUSY_TX)
            ;
        ctrl->uartx->api->write(ctrl->uartx->ctrl, ctrl->tx_buf, len);
    }
}

// 非阻塞scanf
static uint8_t esp8266_scanf(ESP8266_CTRL_t *ctrl, const char *fmt, ...)
{
    if (!ctrl->uartx->api->get_rxflag(ctrl->uartx->ctrl))
        return 0;
    ctrl->uartx->api->set_rxflag(ctrl->uartx->ctrl, 0);

    uint32_t rx_size = ctrl->uartx->api->get_rx_size(ctrl->uartx->ctrl);
    if (rx_size < ctrl->rx_buf_size)
        ctrl->rx_buf[rx_size] = '\0'; // 末尾添加\0，方便后续解析
    else
        ctrl->rx_buf[ctrl->rx_buf_size - 1] = '\0'; // 末尾添加\0

    // uart3.api->write(uart3.ctrl, (uint8_t *)("\r\n"), sizeof("\r\n")); // 回显换行，也可以不用

    va_list args;
    va_start(args, fmt);
    vsscanf((char *)ctrl->rx_buf, fmt, args);

    va_end(args);

    return 1;
}

static void esp8266_init(ESP8266_CTRL_t *ctrl)
{
    ctrl->uartx->api->rxIT(ctrl->uartx->ctrl, ctrl->rx_buf, ctrl->rx_buf_size);

    // ctrl->printf(ctrl, "AT+RESTORE\r\n");// 重置模块
    // vTaskDelay(2000);
    printf("cmd-1: +++\n");
    ctrl->printf(ctrl, "+++"); // 退出透传模式
    vTaskDelay(1500);          // 手册说明至少延时1s

    printf("cmd0: RST\n");
    ctrl->printf(ctrl, "AT+RST\r\n"); // 重启
    vTaskDelay(1000);

    // 注意：ATE1表示开启回显，ATE0表示关闭回显
    // 需要\r回车才能生效
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    printf("cmd1: ATE0\n");
    ulTaskNotifyTake(pdTRUE, 0);
    do
    {
        ctrl->printf(ctrl, "ATE0\r\n");
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("ATE0 timeout\n");
        else
            printf("ATE0 success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_set_ap_ip(ESP8266_CTRL_t *ctrl, char *ip, char *gateway, char *netmask)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    int ip_len_arry = sizeof(ctrl->ap_ip);
    int gateway_len_arry = sizeof(ctrl->ap_gateway);
    int netmask_len_arry = sizeof(ctrl->ap_netmask);
    int ip_len = strnlen(ip, ip_len_arry);
    int gateway_len = strnlen(gateway, gateway_len_arry);
    int netmask_len = strnlen(netmask, netmask_len_arry);

    if (ip_len == ip_len_arry)
    {
        printf("ip is too long or no \\0\n");
        return;
    }
    if (gateway_len == gateway_len_arry)
    {
        printf("gateway is too long or no \\0\n");
        return;
    }
    if (netmask_len == netmask_len_arry)
    {
        printf("netmask is too long or no \\0\n");
        return;
    }

    // 设置IP地址
    strncpy(ctrl->ap_ip, ip, ip_len + 1);
    strncpy(ctrl->ap_gateway, gateway, gateway_len + 1);
    strncpy(ctrl->ap_netmask, netmask, netmask_len + 1);

    ulTaskNotifyTake(pdTRUE, 0);
    // 设置静态IP地址(也可用动态IP)
    do
    {
        ctrl->printf(ctrl, "AT+CIPAP=\"%s\",\"%s\",\"%s\"\r\n",
                     ctrl->ap_ip,
                     ctrl->ap_gateway,
                     ctrl->ap_netmask);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("CIPAP timeout\n");
        else
            printf("CIPAP success\n");
    } while (ulNotificationValue == 0);
}

// 查看信息
static void esp8266_view_tcp_info(ESP8266_CTRL_t *ctrl)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);
    // 查询IP地址
    do
    {
        ctrl->printf(ctrl, "AT+CIPSTA?\r\n");
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("info ip timeout\n");
        else
            printf("info ip success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 查询TCP状态
    do
    {
        ctrl->printf(ctrl, "AT+CIPSTATUS\r\n");
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("info tcp timeout\n");
        else
            printf("info tcp success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 查询连接状态
    do
    {
        ctrl->printf(ctrl, "AT+CWJAP?\r\n");
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("info cwjap timeout\n");
        else
            printf("info cwjap success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_tcp_config(ESP8266_CTRL_t *ctrl, ESP8266_CWMODE_t cwmode)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ctrl->cwmode = cwmode;

    ulTaskNotifyTake(pdTRUE, 0);
    // 断开wifi  循环断开wifi可能会有问题
    printf("cmd2: CWQAP\n");
    ctrl->printf(ctrl, "AT+CWQAP\r\n");
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
    if (ulNotificationValue == 0)
        printf("CWQAP timeout\n");
    else
        printf("CWQAP success\n");

    ulTaskNotifyTake(pdTRUE, 0);
    // 设置模式（sta客户端1 / ap服务器2）
    printf("cmd3: CWMODE=%d\n", ctrl->cwmode);
    do
    {
        ctrl->printf(ctrl, "AT+CWMODE=%d\r\n", ctrl->cwmode); // 设置模式（sta客户端1 / ap服务器2）
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("CWMODE timeout\n");
        else
            printf("CWMODE success\n");
    } while (ulNotificationValue == 0);

    // 普通/透传（断开重连）模式
    printf("cmd4: CIPMODE=%d\n", ctrl->cipmode);
    do
    {
        ctrl->printf(ctrl, "AT+CIPMODE=%d\r\n", ctrl->cipmode);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("CIPMODE timeout\n");
        else
            printf("CIPMODE success\n");
    } while (ulNotificationValue == 0);

    if (ctrl->cwmode == ESP8266_CWMODE_STA)
    {
        ulTaskNotifyTake(pdTRUE, 0);
        // 单连接模式0/多连接模式1  客户端似乎只能写0
        printf("cmd5: CIPMUX=0\n");
        do
        {
            ctrl->printf(ctrl, "AT+CIPMUX=0\r\n");
            ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
            if (ulNotificationValue == 0)
                printf("CIPMUX timeout\n");
            else
                printf("CIPMUX success\n");
        } while (ulNotificationValue == 0);

        ulTaskNotifyTake(pdTRUE, 0);
        // 设置静态IP地址(也可用动态IP)
        printf("cmd6: CIPSTA=\"%s\",\"%s\",\"%s\"\r\n", ctrl->sta_ip, ctrl->sta_gateway, ctrl->sta_netmask);
        do
        {
            ctrl->printf(ctrl, "AT+CIPSTA=\"%s\",\"%s\",\"%s\"\r\n",
                         ctrl->sta_ip,
                         ctrl->sta_gateway,
                         ctrl->sta_netmask);
            ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
            if (ulNotificationValue == 0)
                printf("CIPSTA timeout\n");
            else
                printf("CIPSTA success\n");
        } while (ulNotificationValue == 0);
    }
    else
    {
        ulTaskNotifyTake(pdTRUE, 0);
        // 单连接模式0/多连接模式1  服务端只能写1
        printf("cmd5: CIPMUX=1\n");
        do
        {
            ctrl->printf(ctrl, "AT+CIPMUX=1\r\n");
            ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
            if (ulNotificationValue == 0)
                printf("CIPMUX timeout\n");
            else
                printf("CIPMUX success\n");
        } while (ulNotificationValue == 0);

        ulTaskNotifyTake(pdTRUE, 0);
        // 设置静态IP地址(也可用动态IP)
        printf("cmd6: CIPAP=\"%s\",\"%s\",\"%s\"\r\n", ctrl->ap_ip, ctrl->ap_gateway, ctrl->ap_netmask);
        do
        {
            ctrl->printf(ctrl, "AT+CIPAP=\"%s\",\"%s\",\"%s\"\r\n",
                         ctrl->ap_ip,
                         ctrl->ap_gateway,
                         ctrl->ap_netmask);
            ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
            if (ulNotificationValue == 0)
                printf("CIPAP timeout\n");
            else
                printf("CIPAP success\n");
        } while (ulNotificationValue == 0);
    }
}

static void esp8266_tcp_connect_server(ESP8266_CTRL_t *ctrl)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);
    // 连接wifi
    do
    {
        ctrl->printf(ctrl, "AT+CWJAP=\"%s\",\"%s\"\r\n",
                     ctrl->service_ssid,
                     ctrl->service_password);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("CWJAP timeout\n");
        else
            printf("CWJAP success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 连接服务器(只测试了单连接，还有一个link id参数)
    do
    {
        ctrl->printf(ctrl, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",
                     ctrl->service_ip,
                     ctrl->service_port);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("CIPSTART timeout\n");
        else
            printf("CIPSTART success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_tcp_create_server(ESP8266_CTRL_t *ctrl)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);
    // 配置AP参数,打开热点
    do
    {
        ctrl->printf(ctrl, "AT+CWSAP=\"%s\",\"%s\",5,3\r\n",
                     ctrl->ap_ssid,
                     ctrl->ap_password);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("CWSAP timeout\n");
        else
            printf("CWSAP success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 建立TCP服务器  多连接情况下 (AT+CIPMUX=1)，才能开启 TCP 服务器
    do
    {
        ctrl->printf(ctrl, "AT+CIPSERVER=1,%d\r\n", ctrl->ap_port);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
        if (ulNotificationValue == 0)
            printf("CIPSERVER timeout\n");
        else
            printf("CIPSERVER success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_tcp_close_server(ESP8266_CTRL_t *ctrl)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);

    ctrl->printf(ctrl, "AT+CIPSERVER=0,%d\r\n", ctrl->ap_port);
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
    if (ulNotificationValue == 0)
        printf("CIPSERVER close timeout\n");
    else
        printf("CIPSERVER close success\n");
}

static void esp8266_tcp_send(ESP8266_CTRL_t *ctrl, uint8_t link_id, const char *fmt, ...)
{
    static uint8_t scanf_str[128];
    static uint8_t transfer_flag = 0;

    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf((char *)scanf_str, sizeof(scanf_str), fmt, args);
    va_end(args);

    if (len > 0)
    {
        if (ctrl->cipmode == ESP8266_CIPMODE_SINGLE)
        {
            if (ctrl->cwmode == ESP8266_CWMODE_STA)
            {
                ulTaskNotifyTake(pdTRUE, 0);

                ctrl->printf(ctrl, "AT+CIPSEND=%d\r\n", len);
                ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
                if (ulNotificationValue == 0)
                    printf("CIPSEND timeout 0\n");
                else
                    printf("CIPSEND success 0\n");
            }
            else
            {
                ulTaskNotifyTake(pdTRUE, 0);

                ctrl->printf(ctrl, "AT+CIPSEND=%d,%d\r\n", link_id, len);
                ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
                if (ulNotificationValue == 0)
                    printf("CIPSEND timeout 0\n");
                else
                    printf("CIPSEND success 0\n");

                ulTaskNotifyTake(pdTRUE, 0);
            }

            ctrl->printf(ctrl, "%s", scanf_str);
        }
        else
        {
            if (!transfer_flag)
            {
                transfer_flag = 1;

                ulTaskNotifyTake(pdTRUE, 0);
                do
                {
                    ctrl->printf(ctrl, "AT+CIPSEND\r\n");
                    ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
                    if (ulNotificationValue == 0)
                        printf("CIPSEND timeout 0\n");
                    else
                        printf("CIPSEND success 0\n");
                } while (ulNotificationValue == 0);
            }
        }
        // 透传模式下，每包最大 2048 字节，透传模式以20ms为间隔进行数据打包，并且不再回发"OK"
        // 如果要退出透传模式，发送"+++"，末尾不要添加\r\n
        ctrl->printf(ctrl, "%s", scanf_str);
        vTaskDelay(30);
    }
}

static void esp8266_view_mqtt_info(ESP8266_CTRL_t *ctrl)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);
    // 查询mqtt连接状态
    do
    {
        ctrl->printf(ctrl, "AT+MQTTSUB?\r\n");
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("MQTTSUB timeout\n");
        else
            printf("MQTTSUB success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_mqtt_config(ESP8266_CTRL_t *ctrl, ESP8266_CWMODE_t cwmode)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);
    // 断开mqtt连接，释放资源
    ctrl->printf(ctrl, "AT+MQTTCLEAN=%d\r\n", ctrl->mqtt_link_id);
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000);
    if (ulNotificationValue == 0)
        printf("MQTTCLEAN timeout\n");
    else
        printf("MQTTCLEAN success\n");

    ulTaskNotifyTake(pdTRUE, 0);
    // 断开wifi  循环断开wifi可能会有问题
    ctrl->printf(ctrl, "AT+CWQAP\r\n");
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
    if (ulNotificationValue == 0)
        printf("CWQAP timeout\n");
    else
        printf("CWQAP success\n");

    ulTaskNotifyTake(pdTRUE, 0);
    // 设置wifi模式
    do
    {
        ctrl->printf(ctrl, "AT+CWMODE=%d\r\n", cwmode);       // 设置模式（sta客户端1 / ap服务器2）
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("CWMODE timeout\n");
        else
            printf("CWMODE success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 配置mqtt用户参数
    do
    {
        ctrl->printf(ctrl, "AT+MQTTUSERCFG=%d,%d,\"%s\",\"%s\",\"%s\",%d,%d,\"%s\"\r\n",
                     ctrl->mqtt_link_id,
                     ctrl->mqtt_scheme,
                     ctrl->mqtt_client_id,
                     ctrl->mqtt_username,
                     ctrl->mqtt_password,
                     ctrl->mqtt_cert_key_id,
                     ctrl->mqtt_ca_id,
                     ctrl->mqtt_path);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("MQTTUSERCFG timeout\n");
        else
            printf("MQTTUSERCFG success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_mqtt_connect(ESP8266_CTRL_t *ctrl)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);
    // 连接wifi
    do
    {
        ctrl->printf(ctrl, "AT+CWJAP=\"%s\",\"%s\"\r\n",
                     ctrl->service_ssid,
                     ctrl->service_password);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("CWJAP timeout\n");
        else
            printf("CWJAP success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 连接mqtt
    do
    {
        ctrl->printf(ctrl, "AT+MQTTCONN=%d,\"%s\",%d,%d\r\n",
                     ctrl->mqtt_link_id,
                     ctrl->mqtt_host,
                     ctrl->mqtt_port,
                     ctrl->mqtt_reconnect);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("MQTTCONN timeout\n");
        else
            printf("MQTTCONN success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_mqtt_disconnect(ESP8266_CTRL_t *ctrl)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;
    ulTaskNotifyTake(pdTRUE, 0);

    ctrl->printf(ctrl, "AT+MQTTCLEAN=%d\r\n", ctrl->mqtt_link_id);

    ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
    if (ulNotificationValue == 0)
        printf("MQTTCLEAN timeout\n");
    else
        printf("MQTTCLEAN success\n");
}

static void esp8266_mqtt_subscribe(ESP8266_CTRL_t *ctrl, char *topic, uint8_t qos)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    ulTaskNotifyTake(pdTRUE, 0);
    do
    {
        ctrl->printf(ctrl, "AT+MQTTSUB=%d,\"%s\",%d\r\n", ctrl->mqtt_link_id, topic, qos);

        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("%s timeout\n", topic);
        else
            printf("%s success\n", topic);

    } while (ulNotificationValue == 0);
}

static void esp8266_mqtt_unsubscribe(ESP8266_CTRL_t *ctrl, char *topic)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;
    ulTaskNotifyTake(pdTRUE, 0);

    ctrl->printf(ctrl, "AT+MQTTUNSUB=%d,\"%s\"\r\n", ctrl->mqtt_link_id, topic);
    ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
    if (ulNotificationValue == 0)
        printf("MQTTUNSUB timeout\n");
    else
        printf("MQTTUNSUB success\n");
}

static void esp8266_mqtt_publish(ESP8266_CTRL_t *ctrl, char *topic, char *data, uint8_t qos, uint8_t retain)
{
    // ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    // uint32_t ulNotificationValue = 0;
    // ulTaskNotifyTake(pdTRUE, 0);

    ctrl->printf(ctrl, "AT+MQTTPUB=%d,\"%s\",\"%s\",%d,%d\r\n",
                 ctrl->mqtt_link_id,
                 topic,
                 data,
                 qos,
                 retain);

    // ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
    // if (ulNotificationValue == 0)
    //     printf("MQTTPUB timeout\n");
    // else
    //     printf("MQTTPUB success\n");

    vTaskDelay(20);
}

#if 0
static void esp8266_mqtt_set_client_id(ESP8266_CTRL_t *ctrl, char *client_id)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    int arry_len = sizeof(ctrl->mqtt_client_id);
    int len = strnlen(client_id, arry_len);

    if (len == arry_len) // 过长或无\0结尾
    {
        printf("client_id is too long or no \\0\n");
        return;
    }

    strncpy(ctrl->mqtt_client_id, client_id, len + 1);

    ulTaskNotifyTake(pdTRUE, 0);
    // 设置client_id长度
    do
    {
        ctrl->printf(ctrl, "AT+MQTTLONGCLIENTID=%d,\"%d\"\r\n",
                     ctrl->mqtt_link_id,
                     len);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("MQTTLONGCLIENTID timeout\n");
        else
            printf("MQTTLONGCLIENTID success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 输入client_id
    do
    {
        ctrl->printf(ctrl, "%s\r\n", ctrl->mqtt_client_id);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("set client_id timeout\n");
        else
            printf("set client_id success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_mqtt_set_username(ESP8266_CTRL_t *ctrl, char *username)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    int arry_len = sizeof(ctrl->mqtt_username);
    int len = strnlen(username, arry_len);

    if (len == arry_len) // 过长或无\0结尾
    {
        printf("username is too long or no \\0\n");
        return;
    }

    strncpy(ctrl->mqtt_username, username, len + 1);

    ulTaskNotifyTake(pdTRUE, 0);
    // 设置username长度
    do
    {
        ctrl->printf(ctrl, "AT+MQTTLONGUSERNAME=%d,\"%d\"\r\n", ctrl->mqtt_link_id, len);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("MQTTLONGUSERNAME timeout\n");
        else
            printf("MQTTLONGUSERNAME success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 输入username
    do
    {
        ctrl->printf(ctrl, "%s\r\n", ctrl->mqtt_username);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("set username timeout\n");
        else
            printf("set username success\n");
    } while (ulNotificationValue == 0);
}

static void esp8266_mqtt_set_password(ESP8266_CTRL_t *ctrl, char *password)
{
    ctrl->notify_handle = xTaskGetCurrentTaskHandle(); // 记录当前任务句柄
    uint32_t ulNotificationValue = 0;

    int arry_len = sizeof(ctrl->mqtt_password);
    int len = strnlen(password, arry_len);

    if (len == arry_len) // 过长或无\0结尾
    {
        printf("password is too long or no \\0\n");
        return;
    }

    strncpy(ctrl->mqtt_password, password, len + 1);

    ulTaskNotifyTake(pdTRUE, 0);
    // 设置password长度
    do
    {
        ctrl->printf(ctrl, "AT+MQTTLONGPASSWORD=%d,\"%d\"\r\n", ctrl->mqtt_link_id, len);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("MQTTLONGPASSWORD timeout\n");
        else
            printf("MQTTLONGPASSWORD success\n");
    } while (ulNotificationValue == 0);

    ulTaskNotifyTake(pdTRUE, 0);
    // 输入password
    do
    {
        ctrl->printf(ctrl, "%s\r\n", ctrl->mqtt_password);
        ulNotificationValue = ulTaskNotifyTake(pdTRUE, 5000); // 等待通知
        if (ulNotificationValue == 0)
            printf("set password timeout\n");
        else
            printf("set password success\n");
    } while (ulNotificationValue == 0);
}
#endif

static void esp8266_notify(ESP8266_CTRL_t *ctrl)
{
    if (ctrl->notify_handle != NULL)
    {
        xTaskNotifyGive(ctrl->notify_handle);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
static uint8_t esp8266_tx_buf[128];
static uint8_t esp8266_rx_buf[3 * 1024];

static ESP8266_CTRL_t esp8266_ctrl = {
    .notify_handle = NULL,

    .uartx = &uart3,
    .tx_buf = esp8266_tx_buf,
    .tx_buf_size = sizeof(esp8266_tx_buf),
    .rx_buf = esp8266_rx_buf,
    .rx_buf_size = sizeof(esp8266_rx_buf),

    .service_ssid = "LAPTOP",
    .service_password = "12345678",
    .service_ip = "192.168.9.161",
    .service_port = 6000,

    .ap_ssid = "ESP8266",
    .ap_password = "12345678",
    .ap_ip = "192.168.20.100",
    .ap_gateway = "192.168.20.1",
    .ap_netmask = "255.255.255.0",
    .ap_port = 8080,

    .sta_ip = "192.168.137.211",
    .sta_gateway = "192.168.137.1",
    .sta_netmask = "255.255.255.0",

    .mqtt_link_id = 0, // 只支持0
    .mqtt_scheme = 1,  // 只支持1或6
    .mqtt_client_id = "esp_client_id",
    .mqtt_username = "esp_name",
    .mqtt_password = "12345678",
    .mqtt_cert_key_id = 0, // 仅支持一套 cert 证书，参数为 0
    .mqtt_ca_id = 0,       // 仅支持一套 CA 证书，参数为 0
    .mqtt_path = "",

    .mqtt_host = "broker.emqx.io",
    .mqtt_port = 1883,
    .mqtt_reconnect = 1,

    .cwmode = ESP8266_CWMODE_INVALID,
    .cipmode = ESP8266_CIPMODE_TRANSPARENT,
    // ESP8266_CIPMODE_SINGLE  仅支持 TCP 单连接和 UDP 固定通信端对端的情况

    .printf = esp8266_printf,
    .scanf = esp8266_scanf,
};

static ESP8266_API_t esp8266_api = {
    .scanf = esp8266_scanf,
    .set_ap_ip = esp8266_set_ap_ip,

    .init = esp8266_init,
    .view_tcp_info = esp8266_view_tcp_info,
    .tcp_config = esp8266_tcp_config,
    .tcp_connect_server = esp8266_tcp_connect_server,
    .tcp_create_server = esp8266_tcp_create_server,
    .tcp_close_server = esp8266_tcp_close_server,
    .tcp_send = esp8266_tcp_send,

    .view_mqtt_info = esp8266_view_mqtt_info,
    .mqtt_config = esp8266_mqtt_config,
    .mqtt_connect = esp8266_mqtt_connect,
    .mqtt_disconnect = esp8266_mqtt_disconnect,
    .mqtt_subscribe = esp8266_mqtt_subscribe,
    .mqtt_unsubscribe = esp8266_mqtt_unsubscribe,
    .mqtt_publish = esp8266_mqtt_publish,

#if 0
    .mqtt_set_client_id = esp8266_mqtt_set_client_id,
    .mqtt_set_username = esp8266_mqtt_set_username,
    .mqtt_set_password = esp8266_mqtt_set_password,
#endif

    .notify = esp8266_notify,
};

const ESP8266_t esp8266 = {
    .ctrl = &esp8266_ctrl,
    .api = &esp8266_api,
};
