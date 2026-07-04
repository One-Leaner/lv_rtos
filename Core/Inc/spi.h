/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    spi.h
 * @brief   This file contains all the function prototypes for
 *          the spi.c file
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPI_H__
#define __SPI_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

  /* USER CODE BEGIN Includes */

  /* USER CODE END Includes */

  extern SPI_HandleTypeDef hspi1;

  extern SPI_HandleTypeDef hspi2;

  /* USER CODE BEGIN Private defines */

  /* USER CODE END Private defines */

  void MX_SPI1_Init(void);
  void MX_SPI2_Init(void);

  /* USER CODE BEGIN Prototypes */
  typedef enum
  {
    SPI_DATAWIDTH_8BIT = 0,
    SPI_DATAWIDTH_16BIT,
  } SPI_DATAWIDTH_t;

  typedef struct
  {
    void (*write)(const uint8_t *, uint16_t);
    void (*read)(uint8_t *, uint16_t);
    void (*writeRead)(const uint8_t *, uint8_t *, uint16_t);
    HAL_SPI_StateTypeDef (*get_state)();
    void (*set_datawidth)(SPI_DATAWIDTH_t);
  } SPI_API_t;

  extern const SPI_API_t spi1_api;
  extern const SPI_API_t spi2_api;
  /* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __SPI_H__ */
