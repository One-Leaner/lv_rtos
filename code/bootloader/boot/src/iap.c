#include "w25qxx.h"
#include <stdio.h>
#include <string.h>

#define APP_START_ADDR 0x08010000  /* 应用程序起始地址 */
#define APP_MAX_SIZE (960 * 1024U) /* 最大大小 960KB */

#define IAP_A_ADDR 0x00000000                     // A区起始地址
#define IAP_A_SIZE (1024 * 1024U)                 // A区大小(1MB)
#define IAP_B_ADDR (IAP_A_ADDR + IAP_A_SIZE)      // B区起始地址
#define IAP_B_SIZE (1024 * 1024U)                 // B区大小(1MB)
#define IAP_CONFIG_ADDR (IAP_B_ADDR + IAP_B_SIZE) // 配置区起始地址
#define IAP_CONFIG_SIZE (4 * 1024U)               // 配置区大小(4kB)

#define IAP_MAGIC 0x49505634

typedef struct
{
    uint32_t magic;         // 魔数
    uint32_t firmware_size; // 固件大小
    char version[16];       // 版本号
    uint8_t update_flag;    // 升级标志位
} IAP_CONFIG_t;

void boot_jump_to_app(void)
{
    // 关闭中断
    __disable_irq();
    // 设置MSP(APP栈顶指针)
    __set_MSP(*((uint32_t *)APP_START_ADDR));
    // 复位函数地址
    uint32_t app_reset_handler_addr = *((uint32_t *)(APP_START_ADDR + 4));
    // 获取复位函数指针指向的函数地址
    void (*app_reset_handler)(void) = (void (*)(void))(app_reset_handler_addr);
    // 重定向到APP的中断向量表
    SCB->VTOR = APP_START_ADDR; // 关键!!!
    // 跳转到应用
    app_reset_handler();
}

// 备份旧固件
static void iap_backup(void)
{
    uint8_t buffer[256];
    printf("\n========== IAP Backup ==========\n");
    printf("Source:0x%x (Internal Flash)\n", APP_START_ADDR);
    printf("Target:0x%x (W25QXX)\n", IAP_B_ADDR);

    printf("Backup firmware...\n");
    for (uint32_t offset = 0; offset < APP_MAX_SIZE; offset += sizeof(buffer))
    {
        if ((offset & (W25QXX_SECTOR_SIZE - 1)) == 0)
        {
            w25qxx.api->erase_sector(w25qxx.ctrl, IAP_B_ADDR + offset);
        }
        // 从内部 Flash 读取 256 字节
        memcpy(buffer, (const void *)(APP_START_ADDR + offset), sizeof(buffer));
        // 写入 W25QXX
        w25qxx.api->write(w25qxx.ctrl, IAP_B_ADDR + offset, buffer, sizeof(buffer));

        // 每 256KB 打印一次进度
        if ((offset % (256 * 1024)) == 0)
        {
            printf("Progress: %lu%%\n", offset * 100 / APP_MAX_SIZE);
        }
    }
    printf("Backup complete! (%u bytes)\n", APP_MAX_SIZE);
    printf("================================\n\n");
}

/*
 * 获取 Flash 地址所在的 Sector 编号（STM32F407VGT6）
 * Sector 0~3:  16KB each    (0x08000000 - 0x0800FFFF)
 * Sector 4:    64KB         (0x08010000 - 0x0801FFFF)
 * Sector 5~11: 128KB each   (0x08020000 - 0x080FFFFF)
 */
static uint32_t get_flash_sector(uint32_t addr)
{
    if (addr < 0x08004000)
        return FLASH_SECTOR_0;
    if (addr < 0x08008000)
        return FLASH_SECTOR_1;
    if (addr < 0x0800C000)
        return FLASH_SECTOR_2;
    if (addr < 0x08010000)
        return FLASH_SECTOR_3;
    if (addr < 0x08020000)
        return FLASH_SECTOR_4;
    if (addr < 0x08040000)
        return FLASH_SECTOR_5;
    if (addr < 0x08060000)
        return FLASH_SECTOR_6;
    if (addr < 0x08080000)
        return FLASH_SECTOR_7;
    if (addr < 0x080A0000)
        return FLASH_SECTOR_8;
    if (addr < 0x080C0000)
        return FLASH_SECTOR_9;
    if (addr < 0x080E0000)
        return FLASH_SECTOR_10;
    return FLASH_SECTOR_11;
}

// 将 W25QXX A 区的固件写入内部 Flash
void iap_update_app(void)
{
    uint32_t flash_addr, w25_addr;
    uint32_t last_sector = 0xFFFFFFFF, current_sector;
    uint8_t buffer[256];

    IAP_CONFIG_t cfg;
    w25qxx.api->read(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));

    printf("\n========== IAP Flash Write ==========\n");
    printf("Source: 0x%x (W25QXX--A)\n", IAP_A_ADDR);
    printf("Target: 0x%x (Internal Flash)\n", APP_START_ADDR);
    printf("Firmware size: %lu bytes\n", cfg.firmware_size);
    printf("Crrent APP Version: %s\n", cfg.version);
    printf("Update flag: %d\n", cfg.update_flag);

    if (cfg.update_flag == 0)
    {
        printf("No update flag, skip.\n");
        printf("====================================\n\n");
        return;
    }
    // 魔数校验
    if (cfg.magic != IAP_MAGIC)
    {
        printf("Invalid magic number, skip.\n");
        printf("====================================\n\n");
        return;
    }

    // 备份旧固件
    iap_backup();

    /* 解锁 Flash */
    HAL_FLASH_Unlock();

    for (flash_addr = APP_START_ADDR, w25_addr = IAP_A_ADDR;
         w25_addr < IAP_A_ADDR + cfg.firmware_size;
         flash_addr += sizeof(buffer), w25_addr += sizeof(buffer))
    {
        /* 判断是否进入新的 Sector，进入则擦除 */
        current_sector = get_flash_sector(flash_addr);
        if (current_sector != last_sector)
        {
            FLASH_EraseInitTypeDef erase = {
                .TypeErase = FLASH_TYPEERASE_SECTORS,
                .Sector = current_sector,
                .NbSectors = 1,
                .VoltageRange = FLASH_VOLTAGE_RANGE_3,
            };
            uint32_t sector_error = 0;

            printf("Erasing sector %lu... \n", current_sector);
            if (HAL_FLASHEx_Erase(&erase, &sector_error) != HAL_OK)
            {
                printf("FAILED! (sector=%lu, error=%lu)\n", current_sector, sector_error);
                HAL_FLASH_Lock();
                return;
            }
            printf("OK\n");
            last_sector = current_sector;
        }

        /* 从 W25QXX A 区读取 256 字节 */
        w25qxx.api->read(w25qxx.ctrl, w25_addr, buffer, sizeof(buffer));

        /* 按 32 位字写入内部 Flash */
        for (uint32_t i = 0; i < sizeof(buffer); i += 4)
        {
            uint32_t word = *(uint32_t *)(buffer + i);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flash_addr + i, word) != HAL_OK)
            {
                printf("Write FAILED at addr 0x%x\n", flash_addr + i);
                HAL_FLASH_Lock();
                return;
            }
        }

        /* 每 256KB 打印一次进度 */
        if ((w25_addr % (256 * 1024)) == 0 && w25_addr > IAP_A_ADDR)
        {
            uint32_t pct = (w25_addr - IAP_A_ADDR) * 100U / APP_MAX_SIZE;
            printf("Progress: %lu%%\n", pct);
        }
    }

    HAL_FLASH_Lock();
    printf("Flash write complete!\n");
    printf("====================================\n\n");

    // 清除更新标志
    cfg.update_flag = 0;
    w25qxx.api->erase_sector(w25qxx.ctrl, IAP_CONFIG_ADDR); // 擦除配置区扇区(4KB对齐)
    w25qxx.api->write(w25qxx.ctrl, IAP_CONFIG_ADDR, (uint8_t *)&cfg, sizeof(cfg));
}