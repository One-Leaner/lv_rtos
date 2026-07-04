/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    usart.h
 * @brief   This file contains all the function prototypes for
 *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

  /* USER CODE BEGIN Includes */

  /* USER CODE END Includes */

  extern UART_HandleTypeDef huart2;

  extern UART_HandleTypeDef huart3;

  /* USER CODE BEGIN Private defines */

  /* USER CODE END Private defines */

  void MX_USART2_UART_Init(void);
  void MX_USART3_UART_Init(void);

  /* USER CODE BEGIN Prototypes */
  typedef struct UART_CTRL UART_CTRL_t;

  typedef struct
  {
    void (*write)(UART_CTRL_t *, const uint8_t *, uint16_t);
    void (*read)(UART_CTRL_t *, uint8_t *, uint16_t);
    void (*rxIT)(UART_CTRL_t *, uint8_t *, uint16_t);
    HAL_UART_StateTypeDef (*get_state)(UART_CTRL_t *);
    uint8_t (*get_rxflag)(UART_CTRL_t *);
    void (*set_rxflag)(UART_CTRL_t *, uint8_t);
    uint16_t (*get_rx_size)(UART_CTRL_t *);
    void (*set_rx_size)(UART_CTRL_t *, uint16_t);
    uint8_t *(*get_rx_buf)(UART_CTRL_t *);
  } UART_API_t;

  typedef struct
  {
    UART_CTRL_t *ctrl;
    UART_API_t *api;
  } UART_t;

  extern const UART_t uart2;
  extern const UART_t uart3;
  /* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */
