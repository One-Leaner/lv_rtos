#include "mytask.h"
#include "ui.h"
#include "debug.h"
#include "esp8266.h"
#include "lcd.h"
#include "touch.h"
#include "buzzer.h"
#include "fs.h"
#include "w25qxx.h"
#include "delay.h"
#include "heap.h"
#include "my_nes_port.h"

#include "usbd_core.h"
#include "usbd_msc.h"
extern USBD_HandleTypeDef hUsbDeviceHS;

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>

int fps = 0, fps_time = 0;
static SemaphoreHandle_t g_update_sem = NULL;
static SemaphoreHandle_t g_ui_sem = NULL;
volatile uint8_t ota_flag = 0;

/////////////////////任务区BEGIN//////////////////////////
static void mytask_ui(void *arg)
{
    UNUSED(arg);

    extern lv_obj_t *g_nes_canvas;
    uint32_t time = 0;
    uint32_t usb_delay = 0;
    uint8_t usb_flag = 0;
    uint32_t update_delay = 0;

    uint32_t nowtime = 0;
    uint32_t last_time = 0;

    uint8_t skip = 0;

    while (1)
    {
        if (ui.api->get_iap_flag(ui.ctrl) || ui.api->get_ota_flag(ui.ctrl))
            update_delay++;
        if (update_delay == 400) // 延时2s进入更新流程
        {
            update_delay = 0;
            spi1_api.set_datawidth(SPI_DATAWIDTH_8BIT);
            xSemaphoreGive(g_update_sem);
            // 等待更新完成
            xSemaphoreTake(g_ui_sem, portMAX_DELAY);
            ui.api->back_menu(ui.ctrl);
        }

        last_time = HAL_GetTick();

        switch (usb_flag)
        {
        case 0:
            ui.api->update(ui.ctrl);
            if (ui.api->get_usb_flag(ui.ctrl))
            {
                usb_delay = 0;
                usb_flag = 1;
            }
            break;

        case 1:
            ui.api->update(ui.ctrl);
            usb_delay++;
            if (usb_delay == 400) // 延时2s打开usb，等大ui刷新完成
            {
                usb_flag = 2;
                HAL_PCD_DevConnect((PCD_HandleTypeDef *)hUsbDeviceHS.pData);
            }
            break;

        case 2:
            if (hUsbDeviceHS.dev_state == USBD_STATE_DEFAULT || // 默认状态(第一次连接时是这个状态)
                hUsbDeviceHS.dev_state == USBD_STATE_SUSPENDED) // 断开连接
            {
                // 没有立刻连接上，去等待2s
                usb_delay = 0;
                usb_flag = 3;
            }
            else
            {
                usb_flag = 4;
            }
            break;

        case 3:
            usb_delay++;
            if (usb_delay == 400) // 等待2s未连接，关闭usb并返回
            {
                usb_flag = 0;
                ui.api->back_menu(ui.ctrl);
                ui.api->set_usb_flag(ui.ctrl, 0);
                HAL_PCD_DevDisconnect((PCD_HandleTypeDef *)hUsbDeviceHS.pData);
            }
            else if (hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED)
            {
                usb_flag = 4; // 成功连接
            }
            break;

        case 4:
            if (hUsbDeviceHS.dev_state == USBD_STATE_DEFAULT || // 默认状态
                hUsbDeviceHS.dev_state == USBD_STATE_SUSPENDED) // 断开连接
            {
                usb_flag = 0; // 断开连接
                ui.api->back_menu(ui.ctrl);
                ui.api->set_usb_flag(ui.ctrl, 0);
                HAL_PCD_DevDisconnect((PCD_HandleTypeDef *)hUsbDeviceHS.pData);
            }
            break;

        default:
            break;
        }

        nowtime = HAL_GetTick();
        // printf("lvgl time: %lu\n", nowtime - last_time);
        last_time = nowtime;

        if (nes.api->get_state(nes.ctrl) == NES_STATE_RUNNING)
        {
            nes.api->refresh(nes.ctrl);
            skip++;
            if (skip == 3)
            {
                skip = 0;
                lv_obj_invalidate(g_nes_canvas);
            }
            nowtime = HAL_GetTick();
            // printf("nes time: %lu\n", nowtime - last_time);
        }

        time++;
        if (time == 400)
        {
            // printf("ui task\n");
            time = 0;
        }

        if (fps_time < 200)
            fps++;

        vTaskDelay(5);
    }
}

#if 0
static void mytask_nes(void *arg)
{
    UNUSED(arg);
    extern nes_t *g_nes;
    extern SemaphoreHandle_t g_nes_sem;
    extern volatile uint8_t nes_frame_ok;

    // 等待信号量
    xSemaphoreTake(g_nes_sem, portMAX_DELAY);

    while (1)
    {
        if (nes_frame_ok == 0)
            nes_run(g_nes);
        else
            vTaskDelay(1);
    }
}
#endif

static void mytask_esp8266(void *arg)
{
    UNUSED(arg);

    printf("esp8266 task\n");
    esp8266.api->init(esp8266.ctrl);

    // 客户端测试
    // esp8266.api->tcp_config(esp8266.ctrl, ESP8266_CWMODE_STA);
    // esp8266.api->tcp_connect_server(esp8266.ctrl);
    // 服务器测试
    // esp8266.api->tcp_config(esp8266.ctrl, ESP8266_CWMODE_AP);
    // esp8266.api->tcp_create_server(esp8266.ctrl);
    // mqtt测试
    esp8266.api->mqtt_config(esp8266.ctrl, ESP8266_CWMODE_STA);
    esp8266.api->mqtt_connect(esp8266.ctrl);

    // 订阅主题
    esp8266.api->mqtt_subscribe(esp8266.ctrl, "device/ota/start", 0);
    esp8266.api->mqtt_subscribe(esp8266.ctrl, "device/ota/data", 0);
    esp8266.api->mqtt_subscribe(esp8266.ctrl, "device/ota/stop", 0);
    esp8266.api->mqtt_subscribe(esp8266.ctrl, "device/ota/size", 0);

    while (1)
    {
        // tcp 发送消息
        // esp8266.api->tcp_send(esp8266.ctrl, 0, "hello world\r\n");

        vTaskDelay(1000);
    }
}

static void mytask_esp8266_scanf(void *arg)
{
    UNUSED(arg);

    static char log_str[128];
    printf("scanf task\n");

    while (1)
    {
        if (ota_flag)
        {
            vTaskDelay(100);
            continue;
        }

        if (esp8266.api->scanf(esp8266.ctrl, "%[^\0]", log_str))
        {
            printf("\nscanf:\n%s\n", log_str);
            if (strstr(log_str, "OK") != NULL)
                esp8266.api->notify(esp8266.ctrl);
        }

        vTaskDelay(10);
    }
}

static void mytask_update(void *arg)
{
    UNUSED(arg);
    extern uint8_t iap_receive(void);
    extern void iap_write_version(void);
    extern uint8_t ota_tcp_receive(void);
    extern uint8_t ota_mqtt_receive(const char *msg);

    static char str[3 * 1024];

    iap_write_version();
    // 等待信号量
    printf("wait update...\n");
    xSemaphoreTake(g_update_sem, portMAX_DELAY);
    printf("update start...\n");

    while (1)
    {
        if (ui.api->get_iap_flag(ui.ctrl))
        {
            if (iap_receive())
            {
                printf("iap update ok\n");
                ui.api->set_iap_flag(ui.ctrl, 0);
                xSemaphoreGive(g_ui_sem);

                xSemaphoreTake(g_update_sem, portMAX_DELAY);
            }
        }
        else if (ui.api->get_ota_flag(ui.ctrl))
        {
            ota_flag = 1;
            // if (ota_tcp_receive())
            // {
            //     printf("ota update ok\n");
            //     ui.api->set_ota_flag(ui.ctrl, 0);
            //     xSemaphoreGive(g_ui_sem);
            //     ota_flag = 0;

            //     xSemaphoreTake(g_update_sem, portMAX_DELAY);
            // }

            // 发布START_REQ
            static uint8_t start_req = 0;
            if (start_req == 0)
            {
                esp8266.api->mqtt_publish(esp8266.ctrl, "device/ota/ack", "START_REQ", 0, 0);
                start_req = 1;
            }

            if (esp8266.api->scanf(esp8266.ctrl, "%[^\0]", str))
            {
                if (ota_mqtt_receive(str))
                {
                    printf("ota update ok\n");
                    ui.api->set_ota_flag(ui.ctrl, 0);
                    xSemaphoreGive(g_ui_sem);
                    ota_flag = 0;
                    start_req = 0;

                    xSemaphoreTake(g_update_sem, portMAX_DELAY);
                }
            }
        }
    }
}

/////////////////////任务区END//////////////////////////

typedef struct
{
    TaskFunction_t pxTaskCode;
    const char *const pcName;
    const uint32_t ulStackDepth;
    void *const pvParameters;
    UBaseType_t uxPriority;
    StackType_t *const puxStackBuffer;
    StaticTask_t *const pxTaskBuffer;
} MYTASK_ARGS_t;

// 添加任务到列表
static StaticTask_t mytask_ui_tcb;
static StaticTask_t mytask_esp8266_tcb;
static StaticTask_t mytask_esp8266_scanf_tcb;
static StaticTask_t mytask_update_tcb;

static uint8_t mytask_ui_stack[8 * 1024];
static uint8_t mytask_esp8266_stack[1024];
static uint8_t mytask_esp8266_scanf_stack[1024];
static uint8_t mytask_update_stack[1024];

static MYTASK_ARGS_t mytask_lists[] = {
    {
        .pxTaskCode = mytask_ui,
        .pcName = "mytask_ui",
        .ulStackDepth = sizeof(mytask_ui_stack) / 4,
        .pvParameters = NULL,
        .uxPriority = 10,
        .puxStackBuffer = (StackType_t *)mytask_ui_stack,
        .pxTaskBuffer = &mytask_ui_tcb,
    },
    {
        .pxTaskCode = mytask_esp8266,
        .pcName = "mytask_esp8266",
        .ulStackDepth = sizeof(mytask_esp8266_stack) / 4,
        .pvParameters = NULL,
        .uxPriority = 10,
        .puxStackBuffer = (StackType_t *)mytask_esp8266_stack,
        .pxTaskBuffer = &mytask_esp8266_tcb,
    },
    {
        .pxTaskCode = mytask_esp8266_scanf,
        .pcName = "mytask_esp8266_scanf",
        .ulStackDepth = sizeof(mytask_esp8266_scanf_stack) / 4,
        .pvParameters = NULL,
        .uxPriority = 10,
        .puxStackBuffer = (StackType_t *)mytask_esp8266_scanf_stack,
        .pxTaskBuffer = &mytask_esp8266_scanf_tcb,
    },
    {
        .pxTaskCode = mytask_update,
        .pcName = "mytask_update",
        .ulStackDepth = sizeof(mytask_update_stack) / 4,
        .pvParameters = NULL,
        .uxPriority = 11,
        .puxStackBuffer = (StackType_t *)mytask_update_stack,
        .pxTaskBuffer = &mytask_update_tcb,
    },
};

void mytask_init()
{
    delay_init();
    heap.api->init(heap.ctrl);
    debug.api->init(debug.ctrl);
    w25qxx.api->init(w25qxx.ctrl);
    fs_mount(); // 挂载文件
    lcd.api->init(lcd.ctrl);
    touch.api->init(touch.ctrl, 10);
    touch.api->adjust(touch.ctrl);
    ui.api->init(ui.ctrl);
    printf("app init done\n");

    // 初始化信号量
    g_ui_sem = xSemaphoreCreateBinary();
    g_update_sem = xSemaphoreCreateBinary();

    for (uint16_t i = 0; i < sizeof(mytask_lists) / sizeof(mytask_lists[0]); i++)
    {
        xTaskCreateStatic(mytask_lists[i].pxTaskCode,
                          mytask_lists[i].pcName,
                          mytask_lists[i].ulStackDepth,
                          mytask_lists[i].pvParameters,
                          mytask_lists[i].uxPriority,
                          mytask_lists[i].puxStackBuffer,
                          mytask_lists[i].pxTaskBuffer);
    }
}
