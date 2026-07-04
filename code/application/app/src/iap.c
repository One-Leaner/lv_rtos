#include "usart.h"
#include "w25qxx.h"
#include "esp8266.h"
#include <stdio.h>
#include <string.h>

#define IAP_A_ADDR 0x00000000                     // A区起始地址
#define IAP_A_SIZE (1024 * 1024U)                 // A区大小(1MB)
#define IAP_B_ADDR (IAP_A_ADDR + IAP_A_SIZE)      // B区起始地址
#define IAP_B_SIZE (1024 * 1024U)                 // B区大小(1MB)
#define IAP_CONFIG_ADDR (IAP_B_ADDR + IAP_B_SIZE) // 配置区起始地址
#define IAP_CONFIG_SIZE (4 * 1024U)               // 配置区大小(4kB)

#define IAP_MAGIC 0x49505634
#define IAP_VERSION "1.0.0"

#define CHUNK_RAW_SIZE 128 // MQTT OTA 每包原始数据大小 (与上位机一致)

typedef struct
{
    uint32_t magic;         // 魔数
    uint32_t firmware_size; // 固件大小
    char version[16];       // 版本号
    uint8_t update_flag;    // 升级标志位
} IAP_CONFIG_t;

// ============================================================
// MQTT OTA: 状态机
// ============================================================
typedef enum
{
    OTA_MQTT_IDLE = 0,
    OTA_MQTT_DATA,
    OTA_MQTT_DONE
} ota_mqtt_state_t;

void iap_write_version(void)
{
    // printf("iap_write_version\n");
    // 读取配置区版本号
    IAP_CONFIG_t cfg;
    w25qxx.api->read(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
    // printf("version = %s\n", cfg.version);

    if (strcmp(cfg.version, IAP_VERSION) == 0)
        return;

    printf("update version\n");
    // 写入新版本号
    strcpy(cfg.version, IAP_VERSION);
    w25qxx.api->erase_sector(w25qxx.ctrl, IAP_CONFIG_ADDR);
    w25qxx.api->write(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
    // 回读
    // w25qxx.api->read(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
    // printf("version = %s\n", cfg.version);
}

// 接收上位机固件并写入A区
uint8_t iap_receive(void)
{
    static uint8_t first_rx = 1; // 第一个包，解析包大小
    static uint32_t size = 0;    // 接收包大小
    static uint8_t rx_ok = 1;
    static uint32_t crr_addr = IAP_A_ADDR;
    static uint8_t cmd[] = {0xff, 0x57, 0x41, 0x56};

    if (rx_ok)
    {
        rx_ok = 0;
        uart2.api->write(uart2.ctrl, cmd, 4);
    }

    if (!uart2.api->get_rxflag(uart2.ctrl))
        return 0;
    uart2.api->set_rxflag(uart2.ctrl, 0);
    rx_ok = 1;

    uint8_t *rx_buf = uart2.api->get_rx_buf(uart2.ctrl);
    if (first_rx)
    {
        if (uart2.api->get_rx_size(uart2.ctrl) != 4)
        {
            printf("rx_size error, size = %u\n", uart2.api->get_rx_size(uart2.ctrl));
            return 0;
        }

        first_rx = 0;
        size = rx_buf[0] << 24 | rx_buf[1] << 16 | rx_buf[2] << 8 | rx_buf[3];
        printf("size = %lu, rxsize = %u\n", size, uart2.api->get_rx_size(uart2.ctrl));
        return 0;
    }

    if (uart2.api->get_rx_size(uart2.ctrl) != 256)
    {
        printf("rx_size error, size = %u\n", uart2.api->get_rx_size(uart2.ctrl));
        while (1)
            ;
    }

    // 写入A区
    // 先擦除当前扇区(4KB对齐)
    if ((crr_addr & (W25QXX_SECTOR_SIZE - 1)) == 0) // 每4KB擦除一次
    {
        w25qxx.api->erase_sector(w25qxx.ctrl, crr_addr);
    }
    w25qxx.api->write(w25qxx.ctrl, crr_addr, rx_buf, 256);
    crr_addr += 256;

    if (crr_addr - IAP_A_ADDR >= size)
    {
        IAP_CONFIG_t cfg = {
            .magic = IAP_MAGIC,
            .firmware_size = size,
            .version = IAP_VERSION,
            .update_flag = 1,
        };

        // 擦除配置区扇区(4KB对齐)
        w25qxx.api->erase_sector(w25qxx.ctrl, IAP_CONFIG_ADDR);
        // 写入配置区
        w25qxx.api->write(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
        // // 读回验证
        // IAP_CONFIG_t verify;
        // w25qxx.api->read(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&verify, sizeof(verify));
        // printf("Write verify: magic=0x%x, size=%lu, ver=%s, flag=%d\n",
        //        verify.magic, verify.firmware_size, verify.version, verify.update_flag);

        printf("iap load ok\n");

        crr_addr = IAP_A_ADDR;
        first_rx = 1;

        return 1;
    }

    return 0;
}

// tcp 接收固件并写入A区
uint8_t ota_tcp_receive(void)
{
    static uint8_t first_rx = 1; // 第一个包，解析包大小
    static uint32_t size = 0;    // 接收包大小
    static uint8_t rx_ok = 1;
    static uint32_t crr_addr = IAP_A_ADDR;
    static uint8_t cmd[] = {0xff, 0x57, 0x41, 0x56};

    if (rx_ok)
    {
        rx_ok = 0;
        esp8266.api->tcp_send(esp8266.ctrl, 0, "%c%c%c%c\r\n", cmd[0], cmd[1], cmd[2], cmd[3]);
    }

    if (!uart3.api->get_rxflag(uart3.ctrl))
        return 0;
    uart3.api->set_rxflag(uart3.ctrl, 0);
    rx_ok = 1;

    uint8_t *rx_buf = uart3.api->get_rx_buf(uart3.ctrl);
    if (first_rx)
    {
        first_rx = 0;
        size = rx_buf[0] << 24 | rx_buf[1] << 16 | rx_buf[2] << 8 | rx_buf[3];
        printf("size = %lu, rxsize = %u\n", size, uart3.api->get_rx_size(uart3.ctrl));
        return 0;
    }

    if (uart3.api->get_rx_size(uart3.ctrl) != 256)
    {
        printf("rx_size error, size = %u\n", uart3.api->get_rx_size(uart3.ctrl));
        while (1)
            ;
    }

    // 写入A区
    // 先擦除当前扇区(4KB对齐)
    if ((crr_addr & (W25QXX_SECTOR_SIZE - 1)) == 0) // 每4KB擦除一次
    {
        w25qxx.api->erase_sector(w25qxx.ctrl, crr_addr);
    }
    w25qxx.api->write(w25qxx.ctrl, crr_addr, rx_buf, 256);
    crr_addr += 256;

    if (crr_addr - IAP_A_ADDR >= size)
    {
        IAP_CONFIG_t cfg = {
            .magic = IAP_MAGIC,
            .firmware_size = size,
            .version = IAP_VERSION,
            .update_flag = 1,
        };

        // 擦除配置区扇区(4KB对齐)
        w25qxx.api->erase_sector(w25qxx.ctrl, IAP_CONFIG_ADDR);
        // 写入配置区
        w25qxx.api->write(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));

        printf("ota load ok\n");

        crr_addr = IAP_A_ADDR;
        first_rx = 1;

        return 1;
    }

    return 0;
}

// mqtt 接收固件并写入A区
uint8_t ota_mqtt_receive(const char *msg)
{
    // printf("\nMSG:\n%s\n", msg);

    static uint8_t buf[256];
    static uint32_t crr_addr;
    static uint32_t firmware_size;
    static uint32_t frame_size;
    static uint32_t frame_decoded = 0;
    static uint16_t dec_len = 0;
    const char *ptr;

    const char *sub = strstr(msg, "device/ota/");
    if (!sub)
        return 0;
    sub += 11;

    if (strncmp(sub, "start", 5) == 0)
    {
        // 解析固件大小
        ptr = strrchr(sub, ',');
        sscanf(ptr + 1, "%lu", &firmware_size);
        // printf("firmware_size = %lu\n", firmware_size);
        // 发送应答
        esp8266.api->mqtt_publish(esp8266.ctrl, "device/ota/ack", "OK", 0, 0);
        crr_addr = IAP_A_ADDR; // 重置写入地址到A区开始
        dec_len = 0;
    }
    else if (strncmp(sub, "size", 4) == 0)
    {
        // 每帧数据包长度
        ptr = strrchr(sub, ',');
        sscanf(ptr + 1, "%lu", &frame_size);
        frame_decoded = 0;
        // 发送应答
        esp8266.api->mqtt_publish(esp8266.ctrl, "device/ota/ack", "OK", 0, 0);
    }
    else if (strncmp(sub, "data", 4) == 0)
    {
        // 解析数据包
        ptr = strchr(sub + 6, ',');
        if (!ptr)
            return 0;

        while (1)
        {
            // 解码数据包
            const char *hex;
            for (hex = ptr + 1; *hex != '\r' && *hex != '\0'; hex += 2)
            {
                uint8_t high = 0, low = 0;
                if (*hex >= '0' && *hex <= '9')
                    high = *hex - '0';
                else if (*hex >= 'A' && *hex <= 'F')
                    high = *hex - 'A' + 10;
                else if (*hex >= 'a' && *hex <= 'f')
                    high = *hex - 'a' + 10;

                if (*(hex + 1) >= '0' && *(hex + 1) <= '9')
                    low = *(hex + 1) - '0';
                else if (*(hex + 1) >= 'A' && *(hex + 1) <= 'F')
                    low = *(hex + 1) - 'A' + 10;
                else if (*(hex + 1) >= 'a' && *(hex + 1) <= 'f')
                    low = *(hex + 1) - 'a' + 10;

                buf[dec_len++] = (high << 4) | low;
                frame_decoded++;

                if (dec_len >= 256)
                {
                    dec_len = 0;
                    // 检查是否需要擦除当前扇区(4KB对齐)
                    if ((crr_addr & (W25QXX_SECTOR_SIZE - 1)) == 0) // 每4KB擦除一次
                    {
                        w25qxx.api->erase_sector(w25qxx.ctrl, crr_addr);
                    }
                    // 写入A区
                    w25qxx.api->write(w25qxx.ctrl, crr_addr, buf, 256);
                    crr_addr += 256;
                }
            }
            ptr = strstr(hex, "data");
            if (!ptr)
                break;
            ptr = strchr(ptr + 6, ',');
            if (!ptr)
                break;
        }

        if (dec_len > 0) // 最后一次接收
        {
            // 检查是否需要擦除当前扇区(4KB对齐)
            if ((crr_addr & (W25QXX_SECTOR_SIZE - 1)) == 0) // 每4KB擦除一次
            {
                w25qxx.api->erase_sector(w25qxx.ctrl, crr_addr);
            }
            // 写入A区
            w25qxx.api->write(w25qxx.ctrl, crr_addr, buf, dec_len);
            crr_addr += dec_len;
            dec_len = 0;
        }
        if (frame_decoded == frame_size)
        {
            // 发送应答
            esp8266.api->mqtt_publish(esp8266.ctrl, "device/ota/ack", "OK", 0, 0);
            frame_decoded = 0;
            frame_size = 0;
        }
        else
        {
            // 网络波动导致一帧数据被esp8266分为了两包或更多
            // printf("frame_size = %lu, frame_decoded = %lu\n", frame_size, frame_decoded);
        }

        // uint8_t check[32];
        // w25qxx.api->read(w25qxx.ctrl, IAP_A_ADDR, check, 32);
        // printf("[OTA] A_32_byte: ");
        // for (int i = 0; i < 32; i++)
        //     printf("%02X", check[i]);
        // printf("\n");
        // while (1)
        //     ;
    }
    else if (strncmp(sub, "stop", 4) == 0)
    {
        // 停止接收，写入配置区
        IAP_CONFIG_t cfg = {
            .magic = IAP_MAGIC,
            .firmware_size = firmware_size,
            .version = IAP_VERSION,
            .update_flag = 1,
        };
        // 擦除配置区扇区(4KB对齐)
        w25qxx.api->erase_sector(w25qxx.ctrl, IAP_CONFIG_ADDR);
        // 写入配置区
        w25qxx.api->write(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
        // 发送应答
        esp8266.api->mqtt_publish(esp8266.ctrl, "device/ota/ack", "OK", 0, 0);

        // === 校验：实际写入字节数 vs 声明的 firmware_size ===
        uint32_t actual_written = crr_addr - IAP_A_ADDR;
        // printf("[OTA] firmware_size=%lu  actual_written=%lu  diff=%ld\n",
        //        firmware_size, actual_written, (int32_t)(firmware_size - actual_written));

        return 1;
    }

    return 0;
}