/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    vcp_cli.h
  * @brief   USB CDC / VCP command line interface for the TLV612901 BSP.
  *
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
#ifndef __VCP_CLI_H__
#define __VCP_CLI_H__

/* Maximum characters per USB CDC packet. */
#define VCP_TX_CHUNK        64U

/* Bounded retry count when the CDC endpoint reports USBD_BUSY. */
#define VCP_TX_WAIT_LOOPS   20000U

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Prototypes */

void VCP_Init(void);
void VCP_Process(const uint8_t *buf, uint32_t len);
void VCP_Monitor(void);
void VCP_Print(const char *s);
void VCP_Printf(const char *fmt, ...);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /* __VCP_CLI_H__ */
