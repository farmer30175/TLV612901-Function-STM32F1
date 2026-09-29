/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tlv612901.h
  * @brief   BSP/driver for the TI TLV612901 5.5V/11A synchronous boost
  *          converter with bypass mode and I2C control interface.
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
#ifndef __TLV612901_H__
#define __TLV612901_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* Board pin assignment.
 * PA3 -> TLV612901 EN (ball A1). Has an internal 800k pulldown (R_PD), so it
 *       must be actively driven. NOTE: EN low kills the I2C block entirely, so
 *       EN is held HIGH and on/off is done with CONFIG[6:5] instead.
 * PA4 -> TLV612901 GPIO (ball D1), which is strapped as ADDR on the TLV612901.
 *       The I2C address is locked when the start-up sequence completes, so this
 *       pin MUST already be at its final level before the converter powers up.
 *       Tie it to GND in hardware if you need 0x75 without depending on the MCU. */
#define TLV_EN_PORT          GPIOA
#define TLV_EN_PIN           GPIO_PIN_3
#define TLV_ADDR_PORT        GPIOA
#define TLV_ADDR_PIN         GPIO_PIN_4

/* I2C target address, selected by the ADDR strap at power-up:
 *   ADDR low      -> 75h
 *   ADDR high     -> 76h
 *   ADDR floating -> 77h  (do not leave floating) */
#define TLV_I2C_ADDR_LOW     0x75U
#define TLV_I2C_ADDR_HIGH    0x76U
#define TLV_I2C_ADDR_FLOAT   0x77U

/* Register map */
#define TLV_REG_DEVICEID     0x00U
#define TLV_REG_CONFIG       0x01U
#define TLV_REG_VOUTFLOORSET 0x02U
#define TLV_REG_ILIMBSTSET   0x03U
#define TLV_REG_VOUTROOFSET  0x04U
#define TLV_REG_STATUS       0x05U
#define TLV_REG_ILIMPTSET    0x06U
#define TLV_REG_BSTLOOP      0x07U

/* CONFIG (01h) bit fields */
#define TLV_CFG_RESET_BIT    0x80U
#define TLV_CFG_ENABLE_MASK  0x60U
#define TLV_CFG_ENABLE_SHIFT 5U
#define TLV_CFG_HICCUP_BIT   0x10U
#define TLV_CFG_DISCHG_BIT   0x08U
#define TLV_CFG_SSFM_BIT     0x04U
#define TLV_CFG_MODE_MASK    0x03U
#define TLV_CFG_MODE_SHIFT   0U

/* CONFIG[6:5] ENABLE */
#define TLV_EN_AUTO_BYPASS_0 0x00U  /* auto boost/bypass */
#define TLV_EN_AUTO_BYPASS_1 0x01U  /* auto boost/bypass (power-on default) */
#define TLV_EN_FORCED_BYPASS 0x02U  /* VOUT follows VIN */
#define TLV_EN_I2C_SHUTDOWN  0x03U  /* power stage off, I2C still active */

/* CONFIG[1:0] MODE_CTRL - light load behaviour */
#define TLV_MODE_AUTO_PFM     0x00U /* power-on default, fsw falls to ~20Hz */
#define TLV_MODE_ULTRASONIC   0x01U /* fsw floored at 23kHz, no audible noise */
#define TLV_MODE_FORCED_PWM   0x02U /* continuous conduction, fsw >= 375kHz */
#define TLV_MODE_FORCED_PWM_2 0x03U

/* STATUS (05h) - read only. TSD/VOUT_START/ILIMPT/ILIMBST/FL_LD are
 * clear-on-read, so poll slowly and latch in firmware. */
#define TLV_ST_TSD       0x80U  /* thermal shutdown */
#define TLV_ST_CRC_PASS  0x40U  /* TI OTP test bit, not a functional status */
#define TLV_ST_VOUT_START 0x20U /* 0: VOUT < 0.8V */
#define TLV_ST_OPMODE    0x10U  /* 0: bypass, 1: boost */
#define TLV_ST_ILIMPT    0x08U  /* bypass mode current limit triggered */
#define TLV_ST_ILIMBST   0x04U  /* boost avg input current limit, >4ms */
#define TLV_ST_FL_LD     0x02U  /* boost instantaneous current limit */
#define TLV_ST_PGOOD     0x01U  /* 0: output out of regulation */

/* ILIMBSTSET (03h)[3:0] ILIM_BOOST - average inductor current limit in boost.
 * The codes are NOT monotonic: 0xC is the 11A maximum and 0xD wraps to 3.5A. */
#define TLV_ILIM_3P5A     0x0DU
#define TLV_ILIM_4P0A     0x0EU
#define TLV_ILIM_4P5A     0x0FU
#define TLV_ILIM_5P0A     0x00U
#define TLV_ILIM_5P5A     0x01U
#define TLV_ILIM_6P0A     0x02U
#define TLV_ILIM_6P5A     0x03U
#define TLV_ILIM_7P0A     0x04U
#define TLV_ILIM_7P5A     0x05U
#define TLV_ILIM_8P0A     0x06U
#define TLV_ILIM_8P5A     0x07U
#define TLV_ILIM_9P0A     0x08U
#define TLV_ILIM_9P5A     0x09U
#define TLV_ILIM_10P0A    0x0AU
#define TLV_ILIM_10P5A    0x0BU
#define TLV_ILIM_11P0A    0x0CU

/* ILIMPTSET (06h) 3-bit current limits, 4/6/8/10A. 100b-111b are not defined. */
#define TLV_ILIM_PT_4A     0x0U
#define TLV_ILIM_PT_6A     0x1U
#define TLV_ILIM_PT_8A     0x2U
#define TLV_ILIM_PT_10A    0x3U
#define TLV_ILIM_PT_MASK   0x07U

/* Datasheet power-on values, used as defaults and for read-back checks. */
#define TLV_DEVICEID_RESET 0x70U
#define TLV_VOUT_DEFAULT_MV 5000U

/* USER CODE END Private defines */

/* USER CODE BEGIN Prototypes */

void TLV612901_EN_Set(uint8_t on);
void TLV612901_ADDR_Set(uint8_t high);
void TLV612901_SetAddr(uint16_t addr);
uint16_t TLV612901_GetAddr(void);

HAL_StatusTypeDef TLV612901_ReadReg(uint8_t reg, uint8_t *value);
HAL_StatusTypeDef TLV612901_WriteReg(uint8_t reg, uint8_t value);
HAL_StatusTypeDef TLV612901_ModifyReg(uint8_t reg, uint8_t mask, uint8_t value);

HAL_StatusTypeDef TLV612901_Init(void);
HAL_StatusTypeDef TLV612901_SoftReset(void);

HAL_StatusTypeDef TLV612901_SetEnable(uint8_t enable);
HAL_StatusTypeDef TLV612901_SetMode(uint8_t mode);
HAL_StatusTypeDef TLV612901_SetHiccup(uint8_t on);
HAL_StatusTypeDef TLV612901_SetDischarge(uint8_t on);
HAL_StatusTypeDef TLV612901_SetSpreadSpectrum(uint8_t on);

HAL_StatusTypeDef TLV612901_SetVout(uint16_t mV);
HAL_StatusTypeDef TLV612901_GetVout(uint16_t *mV);
HAL_StatusTypeDef TLV612901_SetIlimBoost(uint8_t code);
HAL_StatusTypeDef TLV612901_GetIlimBoost(uint8_t *code, uint16_t *mA);
HAL_StatusTypeDef TLV612901_SetIlimBypass(uint8_t code);
HAL_StatusTypeDef TLV612901_GetIlimBypass(uint8_t *code, uint16_t *mA);

HAL_StatusTypeDef TLV612901_GetStatus(uint8_t *status);
uint8_t TLV612901_StatusHasFault(uint8_t status);
void TLV612901_StatusToString(uint8_t status, char *buf, uint16_t size);

uint16_t TLV612901_CodeToVoutMv(uint8_t code);
uint8_t TLV612901_VoutMvToCode(uint16_t mV);
uint16_t TLV612901_CodeToIlimMa(uint8_t code);
uint8_t TLV612901_IlimMaToCode(uint16_t mA);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /* __TLV612901_H__ */
