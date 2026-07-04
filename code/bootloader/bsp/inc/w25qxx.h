#ifndef __W25QXX_H__
#define __W25QXX_H__

#include "spi.h"
#include "gpio.h"

// W25Q64大小8MB
#define W25QXX_BLOCK_SIZE 4096 // 4KB
#define W25QXX_BLOCK_COUNT 2048

// 扇区大小(4KB)
#define W25QXX_SECTOR_SIZE (4 * 1024U)

// 划分逻辑扇区，用于文件系统存储
#define LOGIC_BLOCK_SIZE 8
#define LOGIC_SECTOR_SIZE (W25QXX_BLOCK_SIZE / LOGIC_BLOCK_SIZE)
#define LOGIC_SECTOR_COUNT (W25QXX_BLOCK_COUNT * LOGIC_BLOCK_SIZE)

// Commands

/* Reset Operations */
#define RESET_ENABLE_CMD 0x66
#define RESET_MEMORY_CMD 0x99

#define ENTER_QPI_MODE_CMD 0x38
#define EXIT_QPI_MODE_CMD 0xFF

/* Identification Operations */
#define READ_ID_CMD 0x90
#define DUAL_READ_ID_CMD 0x92
#define QUAD_READ_ID_CMD 0x94
#define READ_JEDEC_ID_CMD 0x9F

/* Read Operations */
#define READ_CMD 0x03
#define FAST_READ_CMD 0x0B
#define DUAL_OUT_FAST_READ_CMD 0x3B
#define DUAL_INOUT_FAST_READ_CMD 0xBB
#define QUAD_OUT_FAST_READ_CMD 0x6B
#define QUAD_INOUT_FAST_READ_CMD 0xEB

/* Write Operations */
#define WRITE_ENABLE_CMD 0x06
#define WRITE_DISABLE_CMD 0x04

/* Register Operations */
#define READ_STATUS_REG1_CMD 0x05
#define READ_STATUS_REG2_CMD 0x35
#define READ_STATUS_REG3_CMD 0x15

#define WRITE_STATUS_REG1_CMD 0x01
#define WRITE_STATUS_REG2_CMD 0x31
#define WRITE_STATUS_REG3_CMD 0x11

/* Program Operations */
#define PAGE_PROG_CMD 0x02
#define QUAD_INPUT_PAGE_PROG_CMD 0x32

/* Erase Operations */
#define SECTOR_ERASE_CMD 0x20
#define CHIP_ERASE_CMD 0xC7

#define PROG_ERASE_RESUME_CMD 0x7A
#define PROG_ERASE_SUSPEND_CMD 0x75

/* Flag Status Register */
#define W25QXX_FSR_BUSY 0x01 /*!< busy */
#define W25QXX_FSR_WREN 0x02 /*!< write enable */
#define W25QXX_FSR_QE 0x02   /*!< quad enable */

typedef struct W25QXX_CTRL W25QXX_CTRL_t;

typedef struct
{
    void (*init)(W25QXX_CTRL_t *);
    void (*reset)(W25QXX_CTRL_t *);
    uint16_t (*read_id)(W25QXX_CTRL_t *);
    void (*write)(W25QXX_CTRL_t *, uint32_t, const uint8_t *, uint32_t);
    void (*read)(W25QXX_CTRL_t *, uint32_t, uint8_t *, uint32_t);
    void (*erase_sector)(W25QXX_CTRL_t *, uint32_t);
    void (*erase_chip)(W25QXX_CTRL_t *);
} W25QXX_API_t;

typedef struct
{
    W25QXX_CTRL_t *ctrl;
    W25QXX_API_t *api;
} W25QXX_t;

extern const W25QXX_t w25qxx;

#endif