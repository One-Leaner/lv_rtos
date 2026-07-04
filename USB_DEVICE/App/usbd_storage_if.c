/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : usbd_storage_if.c
 * @version        : v1.0_Cube
 * @brief          : Memory management layer.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_storage_if.h"

/* USER CODE BEGIN INCLUDE */
#include "sdio.h"
#include "sd_diskio.h"
#include "delay.h"
#include "w25qxx.h"
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
 * @brief Usb device.
 * @{
 */

/** @defgroup USBD_STORAGE
 * @brief Usb mass storage device module
 * @{
 */

/** @defgroup USBD_STORAGE_Private_TypesDefinitions
 * @brief Private types.
 * @{
 */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
 * @}
 */

/** @defgroup USBD_STORAGE_Private_Defines
 * @brief Private defines.
 * @{
 */

#define STORAGE_LUN_NBR 1
#define STORAGE_BLK_NBR 0x10000
#define STORAGE_BLK_SIZ 0x200

/* USER CODE BEGIN PRIVATE_DEFINES */
// w25qxx和lcd共用spi总线，同时操作导致卡顿，电脑无法正常访问，只用卷0(sd卡)
#define __STORAGE_LUN_NBR 1
/* USER CODE END PRIVATE_DEFINES */

/**
 * @}
 */

/** @defgroup USBD_STORAGE_Private_Macros
 * @brief Private macros.
 * @{
 */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
 * @}
 */

/** @defgroup USBD_STORAGE_Private_Variables
 * @brief Private variables.
 * @{
 */

/* USER CODE BEGIN INQUIRY_DATA_HS */
/** USB Mass storage Standard Inquiry Data. */
const int8_t STORAGE_Inquirydata_HS[] = {
    /* 36 */

    /* LUN 0 */
    0x00,
    0x80,
    0x02,
    0x02,
    (STANDARD_INQUIRY_DATA_LEN - 5),
    0x00,
    0x00,
    0x00,
    'S', 'T', 'M', ' ', ' ', ' ', ' ', ' ', /* Manufacturer : 8 bytes */
    'P', 'r', 'o', 'd', 'u', 'c', 't', ' ', /* Product      : 16 Bytes */
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '0', '.', '0', '1' /* Version      : 4 Bytes */
    ,
    /* LUN 1 */
    0x00,
    0x80,
    0x02,
    0x02,
    (STANDARD_INQUIRY_DATA_LEN - 5),
    0x00,
    0x00,
    0x00,
    'S', 'T', 'M', ' ', ' ', ' ', ' ', ' ', /* Manufacturer : 8 bytes */
    'P', 'r', 'o', 'd', 'u', 'c', 't', ' ', /* Product      : 16 Bytes */
    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '0', '.', '0', '1' /* Version      : 4 Bytes */
};
/* USER CODE END INQUIRY_DATA_HS */

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
 * @}
 */

/** @defgroup USBD_STORAGE_Exported_Variables
 * @brief Public variables.
 * @{
 */

extern USBD_HandleTypeDef hUsbDeviceHS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
 * @}
 */

/** @defgroup USBD_STORAGE_Private_FunctionPrototypes
 * @brief Private functions declaration.
 * @{
 */

static int8_t STORAGE_Init_HS(uint8_t lun);
static int8_t STORAGE_GetCapacity_HS(uint8_t lun, uint32_t *block_num, uint16_t *block_size);
static int8_t STORAGE_IsReady_HS(uint8_t lun);
static int8_t STORAGE_IsWriteProtected_HS(uint8_t lun);
static int8_t STORAGE_Read_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_Write_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len);
static int8_t STORAGE_GetMaxLun_HS(void);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
 * @}
 */

USBD_StorageTypeDef USBD_Storage_Interface_fops_HS =
    {
        STORAGE_Init_HS,
        STORAGE_GetCapacity_HS,
        STORAGE_IsReady_HS,
        STORAGE_IsWriteProtected_HS,
        STORAGE_Read_HS,
        STORAGE_Write_HS,
        STORAGE_GetMaxLun_HS,
        (int8_t *)STORAGE_Inquirydata_HS};

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Initializes the storage unit (medium).
 * @param  lun: Logical unit number.
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
int8_t STORAGE_Init_HS(uint8_t lun)
{
  /* USER CODE BEGIN 9 */
  UNUSED(lun);

  return (USBD_OK);
  /* USER CODE END 9 */
}

/**
 * @brief  Returns the medium capacity.
 * @param  lun: Logical unit number.
 * @param  block_num: Number of total block number.
 * @param  block_size: Block size.
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
int8_t STORAGE_GetCapacity_HS(uint8_t lun, uint32_t *block_num, uint16_t *block_size)
{
  /* USER CODE BEGIN 10 */
  switch (lun)
  {
  case 0:
    *block_num = hsd.SdCard.BlockNbr;
    *block_size = hsd.SdCard.BlockSize;
    break;
  case 1:
    *block_num = LOGIC_SECTOR_COUNT;
    *block_size = LOGIC_SECTOR_SIZE;
    break;
  default:
    break;
  }

  return (USBD_OK);
  /* USER CODE END 10 */
}

/**
 * @brief   Checks whether the medium is ready.
 * @param  lun:  Logical unit number.
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
int8_t STORAGE_IsReady_HS(uint8_t lun)
{
  /* USER CODE BEGIN 11 */
  // UNUSED(lun);

  switch (lun)
  {
  case 0:
    if (SD_Driver.disk_status(lun) != 0)
    {
      // printf("sd status failed\n");
      return (USBD_FAIL);
    }
    else
    {
      // printf("sd status success\n");
      return (USBD_OK);
    }
    break;
  case 1:
    return (USBD_OK);
    break;
  default:
    return (USBD_FAIL);
    break;
  }

  return (USBD_FAIL);
  /* USER CODE END 11 */
}

/**
 * @brief  Checks whether the medium is write protected.
 * @param  lun: Logical unit number.
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
int8_t STORAGE_IsWriteProtected_HS(uint8_t lun)
{
  /* USER CODE BEGIN 12 */
  UNUSED(lun);
  return (USBD_OK);
  /* USER CODE END 12 */
}

/**
 * @brief  Reads data from the medium.
 * @param  lun: Logical unit number.
 * @param  buf: data buffer.
 * @param  blk_addr: Logical block address.
 * @param  blk_len: Blocks number.
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
int8_t STORAGE_Read_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  /* USER CODE BEGIN 13 */
  uint32_t timeout = 1000;
  HAL_SD_CardStateTypeDef state;
  HAL_StatusTypeDef dma_state;

  uint32_t addr = blk_addr * LOGIC_SECTOR_SIZE;
  uint32_t len = blk_len * LOGIC_SECTOR_SIZE;

  switch (lun)
  {
  case 0:
    dma_state = HAL_SD_ReadBlocks_DMA(&hsd, buf, blk_addr, blk_len);
    if (dma_state == HAL_BUSY)
      return (USBD_BUSY);
    else if (dma_state == HAL_ERROR)
      return (USBD_FAIL);

    while (timeout--)
    {
      state = HAL_SD_GetCardState(&hsd);

      if (state == HAL_SD_CARD_TRANSFER)
      {
        return (USBD_OK);
      }
      // 也可以使用HAL_Delay;
      delay_us(1500); // 这里的延时是必须，留给sdio清除标志位的时间
    }
    return (USBD_FAIL);
    break;
  case 1:
    w25qxx.api->read(w25qxx.ctrl, addr, buf, len);
    return (USBD_OK);
    break;
  default:
    break;
  }

  return (USBD_FAIL);
  /* USER CODE END 13 */
}

/**
 * @brief  Writes data into the medium.
 * @param  lun: Logical unit number.
 * @param  buf: data buffer.
 * @param  blk_addr: Logical block address.
 * @param  blk_len: Blocks number.
 * @retval USBD_OK if all operations are OK else USBD_FAIL
 */
int8_t STORAGE_Write_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len)
{
  /* USER CODE BEGIN 14 */
  uint32_t timeout = 1000;
  HAL_SD_CardStateTypeDef state;
  HAL_StatusTypeDef dma_state;

  uint32_t addr = blk_addr * LOGIC_SECTOR_SIZE;
  uint32_t len = blk_len * LOGIC_SECTOR_SIZE;

  switch (lun)
  {
  case 0:
    dma_state = HAL_SD_WriteBlocks_DMA(&hsd, buf, blk_addr, blk_len);
    if (dma_state == HAL_BUSY)
      return (USBD_BUSY);
    else if (dma_state == HAL_ERROR)
      return (USBD_FAIL);

    while (timeout--)
    {
      state = HAL_SD_GetCardState(&hsd);
      // printf("w state = %d\n", state);
      if (state == HAL_SD_CARD_TRANSFER)
      {
        return (USBD_OK);
      }
      // 也可以使用HAL_Delay
      delay_us(1000); // 加上延时后，写入sd卡文件就不会损坏了，写入速度大概在400kb/s
    }
    // printf("sd write success\n");
    return (USBD_FAIL);
    break;
  case 1:
    w25qxx.api->erase_sector(w25qxx.ctrl, addr);
    w25qxx.api->write(w25qxx.ctrl, addr, buf, len);
    return (USBD_OK);
    break;
  default:
    break;
  }

  return (USBD_FAIL);
  /* USER CODE END 14 */
}

/**
 * @brief  Returns the Max Supported LUNs.
 * @param  None
 * @retval Lun(s) number.
 */
int8_t STORAGE_GetMaxLun_HS(void)
{
  /* USER CODE BEGIN 15 */
  return (__STORAGE_LUN_NBR - 1);
  /* USER CODE END 15 */
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
 * @}
 */

/**
 * @}
 */
