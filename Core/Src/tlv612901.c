/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    tlv612901.c
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
/* Includes ------------------------------------------------------------------*/
#include "tlv612901.h"
#include <stdio.h>

/* USER CODE BEGIN 0 */

#define TLV_I2C_TIMEOUT_MS  100U

static uint16_t tlv_addr = TLV_I2C_ADDR_LOW;

/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

void TLV612901_EN_Set(uint8_t on)
{
  HAL_GPIO_WritePin(TLV_EN_PORT, TLV_EN_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void TLV612901_ADDR_Set(uint8_t high)
{
  HAL_GPIO_WritePin(TLV_ADDR_PORT, TLV_ADDR_PIN, high ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void TLV612901_SetAddr(uint16_t addr)
{
  tlv_addr = addr;
}

uint16_t TLV612901_GetAddr(void)
{
  return tlv_addr;
}

HAL_StatusTypeDef TLV612901_ReadReg(uint8_t reg, uint8_t *value)
{
  if (value == NULL) {
    return HAL_ERROR;
  }
  return HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(tlv_addr << 1), reg,
                          I2C_MEMADD_SIZE_8BIT, value, 1, TLV_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef TLV612901_WriteReg(uint8_t reg, uint8_t value)
{
  return HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(tlv_addr << 1), reg,
                           I2C_MEMADD_SIZE_8BIT, (uint8_t *)&value, 1,
                           TLV_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef TLV612901_ModifyReg(uint8_t reg, uint8_t mask, uint8_t value)
{
  uint8_t tmp = 0U;
  HAL_StatusTypeDef st = TLV612901_ReadReg(reg, &tmp);
  if (st != HAL_OK) {
    return st;
  }
  tmp = (uint8_t)((tmp & (uint8_t)~mask) | (uint8_t)(value & mask));
  return TLV612901_WriteReg(reg, tmp);
}

static HAL_StatusTypeDef TLV612901_Probe(uint16_t addr)
{
  uint8_t id = 0U;

  if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 3, TLV_I2C_TIMEOUT_MS) != HAL_OK) {
    return HAL_ERROR;
  }
  if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr << 1), TLV_REG_DEVICEID,
                       I2C_MEMADD_SIZE_8BIT, &id, 1, TLV_I2C_TIMEOUT_MS) != HAL_OK) {
    return HAL_ERROR;
  }
  if (id != TLV_DEVICEID_RESET) {
    return HAL_ERROR;
  }
  return HAL_OK;
}

HAL_StatusTypeDef TLV612901_Init(void)
{
  static const uint16_t candidates[3] = {
    TLV_I2C_ADDR_LOW, TLV_I2C_ADDR_HIGH, TLV_I2C_ADDR_FLOAT
  };
  uint8_t i;

  TLV612901_EN_Set(1U);

  for (i = 0U; i < 3U; i++) {
    if (TLV612901_Probe(candidates[i]) == HAL_OK) {
      tlv_addr = candidates[i];
      break;
    }
  }
  if (i == 3U) {
    return HAL_ERROR;
  }

  TLV612901_SetVout(TLV_VOUT_DEFAULT_MV);
  TLV612901_SetEnable(TLV_EN_AUTO_BYPASS_1);
  TLV612901_SetMode(TLV_MODE_AUTO_PFM);
  TLV612901_SetHiccup(1U);
  TLV612901_SetDischarge(1U);

  return HAL_OK;
}

HAL_StatusTypeDef TLV612901_SoftReset(void)
{
  HAL_StatusTypeDef st = TLV612901_WriteReg(TLV_REG_CONFIG, TLV_CFG_RESET_BIT);
  HAL_Delay(5U);
  return st;
}

HAL_StatusTypeDef TLV612901_SetEnable(uint8_t enable)
{
  return TLV612901_ModifyReg(TLV_REG_CONFIG, TLV_CFG_ENABLE_MASK,
                             (uint8_t)(enable << TLV_CFG_ENABLE_SHIFT));
}

HAL_StatusTypeDef TLV612901_SetMode(uint8_t mode)
{
  return TLV612901_ModifyReg(TLV_REG_CONFIG, TLV_CFG_MODE_MASK,
                             (uint8_t)(mode << TLV_CFG_MODE_SHIFT));
}

HAL_StatusTypeDef TLV612901_SetHiccup(uint8_t on)
{
  return TLV612901_ModifyReg(TLV_REG_CONFIG, TLV_CFG_HICCUP_BIT,
                             on ? TLV_CFG_HICCUP_BIT : 0U);
}

HAL_StatusTypeDef TLV612901_SetDischarge(uint8_t on)
{
  return TLV612901_ModifyReg(TLV_REG_CONFIG, TLV_CFG_DISCHG_BIT,
                             on ? TLV_CFG_DISCHG_BIT : 0U);
}

HAL_StatusTypeDef TLV612901_SetSpreadSpectrum(uint8_t on)
{
  return TLV612901_ModifyReg(TLV_REG_CONFIG, TLV_CFG_SSFM_BIT,
                             on ? TLV_CFG_SSFM_BIT : 0U);
}

HAL_StatusTypeDef TLV612901_SetVout(uint16_t mV)
{
  uint8_t code = TLV612901_VoutMvToCode(mV);
  if (code == 0xFFU) {
    return HAL_ERROR;
  }
  return TLV612901_WriteReg(TLV_REG_VOUTFLOORSET, code);
}

HAL_StatusTypeDef TLV612901_GetVout(uint16_t *mV)
{
  uint8_t raw = 0U;
  HAL_StatusTypeDef st = TLV612901_ReadReg(TLV_REG_VOUTFLOORSET, &raw);
  if (st != HAL_OK) {
    return st;
  }
  *mV = TLV612901_CodeToVoutMv((uint8_t)(raw & 0x3FU));
  return HAL_OK;
}

HAL_StatusTypeDef TLV612901_SetIlimBoost(uint8_t code)
{
  return TLV612901_ModifyReg(TLV_REG_ILIMBSTSET, 0x0FU, (uint8_t)(code & 0x0FU));
}

HAL_StatusTypeDef TLV612901_GetIlimBoost(uint8_t *code, uint16_t *mA)
{
  uint8_t raw = 0U;
  HAL_StatusTypeDef st = TLV612901_ReadReg(TLV_REG_ILIMBSTSET, &raw);
  if (st != HAL_OK) {
    return st;
  }
  *code = (uint8_t)(raw & 0x0FU);
  *mA = TLV612901_CodeToIlimMa(*code);
  return HAL_OK;
}

HAL_StatusTypeDef TLV612901_SetIlimBypass(uint8_t code)
{
  return TLV612901_ModifyReg(TLV_REG_ILIMPTSET, TLV_ILIM_PT_MASK,
                             (uint8_t)(code & TLV_ILIM_PT_MASK));
}

HAL_StatusTypeDef TLV612901_GetIlimBypass(uint8_t *code, uint16_t *mA)
{
  uint8_t raw = 0U;
  HAL_StatusTypeDef st = TLV612901_ReadReg(TLV_REG_ILIMPTSET, &raw);
  if (st != HAL_OK) {
    return st;
  }
  *code = (uint8_t)(raw & 0x07U);
  *mA = (uint16_t)(4000U + (*code) * 2000U);
  return HAL_OK;
}

HAL_StatusTypeDef TLV612901_GetStatus(uint8_t *status)
{
  return TLV612901_ReadReg(TLV_REG_STATUS, status);
}

uint8_t TLV612901_StatusHasFault(uint8_t status)
{
  if ((status & TLV_ST_TSD) != 0U) {
    return 1U;
  }
  if ((status & TLV_ST_ILIMBST) != 0U) {
    return 1U;
  }
  if ((status & TLV_ST_FL_LD) != 0U) {
    return 1U;
  }
  if ((status & TLV_ST_ILIMPT) != 0U) {
    return 1U;
  }
  if ((status & TLV_ST_PGOOD) == 0U) {
    return 1U;
  }
  return 0U;
}

void TLV612901_StatusToString(uint8_t status, char *buf, uint16_t size)
{
  int n;

  if ((size == 0U) || (buf == NULL)) {
    return;
  }
  buf[0] = '\0';

  n = snprintf(buf, size, "STATUS=0x%02X PGOOD=%u OPMODE=%s ",
               status,
               (unsigned)((status & TLV_ST_PGOOD) ? 1U : 0U),
               ((status & TLV_ST_OPMODE) ? "BOOST" : "BYPASS"));
  if ((n < 0) || ((uint16_t)n >= size)) {
    return;
  }
  size = (uint16_t)(size - (uint16_t)n);
  buf += n;

  n = snprintf(buf, size, "VOUT_UP=%u ", (unsigned)((status & TLV_ST_VOUT_START) ? 1U : 0U));
  if ((n < 0) || ((uint16_t)n >= size)) {
    return;
  }
  size = (uint16_t)(size - (uint16_t)n);
  buf += n;

  n = snprintf(buf, size, "ILIMBST=%u FL_LD=%u ILIMPT=%u TSD=%u",
               (unsigned)((status & TLV_ST_ILIMBST) ? 1U : 0U),
               (unsigned)((status & TLV_ST_FL_LD) ? 1U : 0U),
               (unsigned)((status & TLV_ST_ILIMPT) ? 1U : 0U),
               (unsigned)((status & TLV_ST_TSD) ? 1U : 0U));
  (void)n;
}

/* The VOUT code -> voltage map is NOT a single linear formula over the whole
 * 6-bit range: it ramps 2.85V..5.50V for codes 0x00..0x35, then wraps back
 * down to 2.80V..2.35V for codes 0x36..0x3F. A lookup table is used so the
 * discontinuity cannot be silently mis-compared. */
static const uint16_t tlv_vout_mv[64] = {
  2850, 2900, 2950, 3000, 3050, 3100, 3150, 3200,
  3250, 3300, 3350, 3400, 3450, 3500, 3550, 3600,
  3650, 3700, 3750, 3800, 3850, 3900, 3950, 4000,
  4050, 4100, 4150, 4200, 4250, 4300, 4350, 4400,
  4450, 4500, 4550, 4600, 4650, 4700, 4750, 4800,
  4850, 4900, 4950, 5000, 5050, 5100, 5150, 5200,
  5250, 5300, 5350, 5400, 5450, 5500, 2800, 2750,
  2700, 2650, 2600, 2550, 2500, 2450, 2400, 2350
};

uint16_t TLV612901_CodeToVoutMv(uint8_t code)
{
  return tlv_vout_mv[code & 0x3FU];
}

uint8_t TLV612901_VoutMvToCode(uint16_t mV)
{
  uint8_t i;
  for (i = 0U; i < 64U; i++) {
    if (tlv_vout_mv[i] == mV) {
      return i;
    }
  }
  return 0xFFU;
}

static const uint16_t tlv_ilim_ma[16] = {
  5000, 5500, 6000, 6500, 7000, 7500, 8000, 8500,
  9000, 9500, 10000, 10500, 11000, 3500, 4000, 4500
};

uint16_t TLV612901_CodeToIlimMa(uint8_t code)
{
  return tlv_ilim_ma[code & 0x0FU];
}

uint8_t TLV612901_IlimMaToCode(uint16_t mA)
{
  uint8_t i;
  for (i = 0U; i < 16U; i++) {
    if (tlv_ilim_ma[i] == mA) {
      return i;
    }
  }
  return 0xFFU;
}

/* USER CODE END 1 */
