/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    vcp_cli.c
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
/* Includes ------------------------------------------------------------------*/
#include "vcp_cli.h"
#include "tlv612901.h"
#include "usbd_cdc_if.h"
#include "usbd_cdc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/* USER CODE BEGIN 0 */

#define VCP_LINE_MAX      64U
#define VCP_MON_PERIOD_MS 250U
#define VCP_PG_FAULT_RUNS 4U

static char vcp_line[VCP_LINE_MAX];
static uint8_t vcp_len = 0U;
static uint8_t vcp_latched = 0U;
static uint8_t vcp_pg_bad = 0U;
static uint8_t vcp_cdc_up = 0U;
static uint8_t vcp_mon = 1U;

/* USER CODE END 0 */

/* USER CODE BEGIN 1 */

int __io_putchar(int ch)
{
  uint8_t c = (uint8_t)ch;
  uint32_t guard;

  if (!vcp_cdc_up) {
    return ch;
  }

  /* CDC_Transmit_FS reports USBD_BUSY while the previous packet is still in
     flight. Retrying is required, otherwise the character is lost. Bounded so a
     disconnected or wedged host cannot hang the caller. */
  for (guard = 0U; guard < VCP_TX_WAIT_LOOPS; guard++) {
    if (CDC_Transmit_FS(&c, 1U) == USBD_OK) {
      return ch;
    }
  }
  return ch;
}

/* Send a whole string in as few USB packets as possible. One byte per packet
   would drop most of the line, since only one packet retires per 1ms frame. */
static void VCP_Write(const char *s, uint32_t len)
{
  uint32_t off = 0U;
  uint32_t guard;

  if (!vcp_cdc_up) {
    return;
  }

  while (off < len) {
    uint32_t n = len - off;
    if (n > VCP_TX_CHUNK) {
      n = VCP_TX_CHUNK;
    }
    for (guard = 0U; guard < VCP_TX_WAIT_LOOPS; guard++) {
      if (CDC_Transmit_FS((uint8_t *)&s[off], (uint16_t)n) == USBD_OK) {
        break;
      }
    }
    off += n;
  }
}

void VCP_Print(const char *s)
{
  VCP_Write(s, (uint32_t)strlen(s));
}

void VCP_Printf(const char *fmt, ...)
{
  char buf[160];
  va_list ap;
  int n;

  va_start(ap, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (n < 0) {
    return;
  }
  if ((uint32_t)n >= sizeof(buf)) {
    n = (int)sizeof(buf) - 1;
  }
  VCP_Write(buf, (uint32_t)n);
}

void VCP_Init(void)
{
  vcp_len = 0U;
  vcp_latched = 0U;
  vcp_pg_bad = 0U;
}

static void VCP_ShutdownOutput(const char *why)
{
  uint8_t st = 0U;

  if (TLV612901_SetEnable(TLV_EN_I2C_SHUTDOWN) == HAL_OK) {
    VCP_Printf("\r\n[TLV612901] OUTPUT DISABLED (%s). I2C stays alive.\r\n", why);
  } else {
    VCP_Printf("\r\n[TLV612901] OUTPUT DISABLE FAILED (%s).\r\n", why);
  }
  if (TLV612901_GetStatus(&st) == HAL_OK) {
    char sbuf[128];
    TLV612901_StatusToString(st, sbuf, sizeof(sbuf));
    VCP_Printf("[TLV612901] %s\r\n", sbuf);
  }
}

static void VCP_CmdHelp(void)
{
  VCP_Print(
    "\r\n--- TLV612901 CLI ---\r\n"
    "  help                 this list\r\n"
    "  id                   device id and I2C address\r\n"
    "  dump                 read registers 0x00..0x07\r\n"
    "  status               read+decode STATUS\r\n"
    "  rd <reg>             raw read one register\r\n"
    "  wr <reg> <val>       raw write one register\r\n"
    "  vout                 show VOUTFLOORSET setpoint\r\n"
    "  vout <mV>            set VOUTFLOORSET (2350..5500mV, 50mV step)\r\n"
    "  ilim                 show boost current limit\r\n"
    "  ilim <mA>            set boost avg current limit (3500..11000)\r\n"
    "  ilimpt <0..3>        set bypass limit 0=4A 1=6A 2=8A 3=10A\r\n"
    "  mode auto|ultra|pwm  light load: auto PFM / ultrasonic / forced PWM\r\n"
    "  en auto|bypass|off   power stage: auto boost+bypass / forced bypass / off\r\n"
    "  hiccup <0|1>         hiccup retry on output short\r\n"
    "  dischg <0|1>         output discharge on shutdown\r\n"
    "  ssfm <0|1>           spread spectrum in forced PWM\r\n"
    "  reset                soft reset, restores register defaults\r\n"
    "  mon <0|1>            periodic status monitor with auto-shutdown\r\n");
}

static void VCP_CmdDump(void)
{
  uint8_t reg;
  for (reg = 0U; reg <= 7U; reg++) {
    uint8_t v = 0U;
    if (TLV612901_ReadReg(reg, &v) != HAL_OK) {
      VCP_Printf("  reg 0x%02X : I2C ERROR\r\n", reg);
    } else {
      VCP_Printf("  reg 0x%02X : 0x%02X\r\n", reg, v);
    }
  }
}

static void VCP_CmdStatus(void)
{
  uint8_t st = 0U;
  char sbuf[128];

  if (TLV612901_GetStatus(&st) != HAL_OK) {
    VCP_Print("STATUS: I2C ERROR (no ACK from the converter)\r\n");
    return;
  }
  TLV612901_StatusToString(st, sbuf, sizeof(sbuf));
  VCP_Printf("%s\r\n", sbuf);
  vcp_latched |= (uint8_t)(st & (TLV_ST_TSD | TLV_ST_ILIMBST |
                                 TLV_ST_FL_LD | TLV_ST_ILIMPT));
}

static void VCP_Execute(char *line)
{
  char *arg = strpbrk(line, " \t");
  if (arg != NULL) {
    *arg++ = '\0';
    while ((*arg == ' ') || (*arg == '\t')) {
      arg++;
    }
  } else {
    arg = line + strlen(line);
  }

  if (strcmp(line, "help") == 0) {
    VCP_CmdHelp();
  } else if ((strcmp(line, "id") == 0) || (strcmp(line, "?") == 0)) {
    uint8_t id = 0U;
    if (TLV612901_ReadReg(TLV_REG_DEVICEID, &id) == HAL_OK) {
      VCP_Printf("DeviceID=0x%02X (expect 0x%02X)  I2C addr=0x%02X\r\n",
                 id, TLV_DEVICEID_RESET, TLV612901_GetAddr());
    } else {
      VCP_Print("DeviceID: I2C ERROR\r\n");
    }
  } else if (strcmp(line, "dump") == 0) {
    VCP_CmdDump();
  } else if (strcmp(line, "status") == 0) {
    VCP_CmdStatus();
  } else if (strcmp(line, "rd") == 0) {
    uint8_t v = 0U;
    long r = strtol(arg, NULL, 0);
    if ((arg[0] == '\0') || (r < 0) || (r > 7) ||
        (TLV612901_ReadReg((uint8_t)r, &v) != HAL_OK)) {
      VCP_Print("ERR usage: rd <0..7>\r\n");
    } else {
      VCP_Printf("reg 0x%02lX = 0x%02X\r\n", r, v);
    }
  } else if (strcmp(line, "wr") == 0) {
    char *end = NULL;
    long r = strtol(arg, &end, 0);
    long v = 0L;
    if (end != NULL) {
      v = strtol(end, NULL, 0);
    }
    if ((arg[0] == '\0') || (r < 0) || (r > 7) || (v < 0) || (v > 255)) {
      VCP_Print("ERR usage: wr <0..7> <0..255>\r\n");
    } else if (TLV612901_WriteReg((uint8_t)r, (uint8_t)v) != HAL_OK) {
      VCP_Print("ERR I2C write failed\r\n");
    } else {
      VCP_Printf("reg 0x%02lX <- 0x%02lX\r\n", r, v);
    }
  } else if (strcmp(line, "vout") == 0) {
    if (arg[0] == '\0') {
      uint16_t mv = 0U;
      if (TLV612901_GetVout(&mv) == HAL_OK) {
        VCP_Printf("VOUT setpoint = %u.%02u V (code 0x%02X)\r\n",
                   mv / 1000U, (mv % 1000U) / 10U,
                   TLV612901_VoutMvToCode(mv));
      } else {
        VCP_Print("ERR I2C read failed\r\n");
      }
    } else {
      uint16_t mv = (uint16_t)atoi(arg);
      if ((mv < 2350U) || (mv > 5500U) ||
          (TLV612901_VoutMvToCode(mv) == 0xFFU)) {
        VCP_Printf("ERR vout must be 2350..5500 mV in 50 mV steps, got '%s'\r\n", arg);
      } else if (TLV612901_SetVout(mv) != HAL_OK) {
        VCP_Print("ERR I2C write failed\r\n");
      } else {
        VCP_Printf("VOUT setpoint -> %u.%02u V (code 0x%02X)\r\n",
                   mv / 1000U, (mv % 1000U) / 10U, TLV612901_VoutMvToCode(mv));
      }
    }
  } else if (strcmp(line, "ilim") == 0) {
    if (arg[0] == '\0') {
      uint8_t code = 0U;
      uint16_t ma = 0U;
      if (TLV612901_GetIlimBoost(&code, &ma) == HAL_OK) {
        VCP_Printf("ILIM_BOOST = %u.%01u A (code 0x%X)\r\n",
                   ma / 1000U, (ma % 1000U) / 100U, code);
      } else {
        VCP_Print("ERR I2C read failed\r\n");
      }
    } else {
      long ma = atol(arg);
      uint8_t code = TLV612901_IlimMaToCode((uint16_t)ma);
      if ((code == 0xFFU) || (ma < 3500L) || (ma > 11000L)) {
        VCP_Printf("ERR ilim must be 3500..11000 mA in 500 mA steps, got '%s'\r\n", arg);
      } else if (TLV612901_SetIlimBoost(code) != HAL_OK) {
        VCP_Print("ERR I2C write failed\r\n");
      } else {
        VCP_Printf("ILIM_BOOST -> %ld mA (code 0x%X)\r\n", ma, code);
      }
    }
  } else if (strcmp(line, "ilimpt") == 0) {
    long c = (arg[0] == '\0') ? -1L : strtol(arg, NULL, 0);
    if ((c < 0L) || (c > 3L) || (TLV612901_SetIlimBypass((uint8_t)c) != HAL_OK)) {
      VCP_Print("ERR usage: ilimpt <0|1|2|3>  (4A/6A/8A/10A)\r\n");
    } else {
      VCP_Printf("bypass ILIM -> %ld A\r\n", (c == 0L) ? 4L : (c * 2L + 4L));
    }
  } else if (strcmp(line, "mode") == 0) {
    uint8_t m;
    if (strcmp(arg, "auto") == 0) {
      m = TLV_MODE_AUTO_PFM;
    } else if (strcmp(arg, "ultra") == 0) {
      m = TLV_MODE_ULTRASONIC;
    } else if (strcmp(arg, "pwm") == 0) {
      m = TLV_MODE_FORCED_PWM;
    } else {
      VCP_Print("ERR usage: mode auto|ultra|pwm\r\n");
      return;
    }
    if (TLV612901_SetMode(m) != HAL_OK) {
      VCP_Print("ERR I2C write failed\r\n");
      return;
    }
    VCP_Printf("light load mode -> %s\r\n", arg);
    if (m == TLV_MODE_AUTO_PFM) {
      VCP_Print("  note: auto PFM lets fsw fall to ~20Hz, expect audible whine\r\n");
    } else if (m == TLV_MODE_ULTRASONIC) {
      VCP_Print("  note: ultrasonic floors fsw at 23kHz, VOUT ~1% higher\r\n");
    } else {
      VCP_Print("  note: forced PWM, fsw floored at 375kHz, lowest ripple\r\n");
    }
  } else if (strcmp(line, "en") == 0) {
    uint8_t e;
    if (strcmp(arg, "auto") == 0) {
      e = TLV_EN_AUTO_BYPASS_1;
    } else if (strcmp(arg, "bypass") == 0) {
      e = TLV_EN_FORCED_BYPASS;
    } else if (strcmp(arg, "off") == 0) {
      e = TLV_EN_I2C_SHUTDOWN;
    } else {
      VCP_Print("ERR usage: en auto|bypass|off\r\n");
      return;
    }
    if (TLV612901_SetEnable(e) != HAL_OK) {
      VCP_Print("ERR I2C write failed\r\n");
    } else {
      VCP_Printf("power stage -> %s\r\n", arg);
    }
  } else if ((strcmp(line, "hiccup") == 0) || (strcmp(line, "dischg") == 0) ||
             (strcmp(line, "ssfm") == 0)) {
    long v = (arg[0] == '\0') ? -1L : strtol(arg, NULL, 0);
    HAL_StatusTypeDef st;
    if ((v < 0L) || (v > 1L)) {
      VCP_Printf("ERR usage: %s <0|1>\r\n", line);
      return;
    }
    if (strcmp(line, "hiccup") == 0) {
      st = TLV612901_SetHiccup((uint8_t)v);
    } else if (strcmp(line, "dischg") == 0) {
      st = TLV612901_SetDischarge((uint8_t)v);
    } else {
      st = TLV612901_SetSpreadSpectrum((uint8_t)v);
    }
    if (st != HAL_OK) {
      VCP_Print("ERR I2C write failed\r\n");
    } else {
      VCP_Printf("%s -> %ld\r\n", line, v);
    }
  } else if (strcmp(line, "reset") == 0) {
    if (TLV612901_SoftReset() != HAL_OK) {
      VCP_Print("ERR I2C write failed\r\n");
    } else {
      VCP_Print("soft reset done, registers back to defaults\r\n");
      vcp_latched = 0U;
      vcp_pg_bad = 0U;
    }
  } else if (strcmp(line, "mon") == 0) {
    long v = (arg[0] == '\0') ? -1L : strtol(arg, NULL, 0);
    if ((v < 0L) || (v > 1L)) {
      VCP_Print("ERR usage: mon <0|1>\r\n");
    } else {
      vcp_mon = (uint8_t)v;
      vcp_latched = 0U;
      vcp_pg_bad = 0U;
      VCP_Printf("status monitor %s\r\n", vcp_mon ? "ON" : "OFF");
    }
  } else if (line[0] == '\0') {
    return;
  } else {
    VCP_Printf("ERR unknown command '%s', try 'help'\r\n", line);
  }
}

void VCP_Process(const uint8_t *buf, uint32_t len)
{
  uint32_t i;

  for (i = 0U; i < len; i++) {
    char c = (char)buf[i];

    if (c == '\r' || c == '\n') {
      if (vcp_len > 0U) {
        vcp_line[vcp_len] = '\0';
        VCP_Execute(vcp_line);
        vcp_len = 0U;
      }
      VCP_Print("\r\n");
    } else if ((c == '\b') || (c == 0x7F)) {
      if (vcp_len > 0U) {
        vcp_len--;
        VCP_Print("\b \b");
      }
    } else if ((c >= 0x20) && (c < 0x7F)) {
      if (vcp_len < (VCP_LINE_MAX - 1U)) {
        vcp_line[vcp_len++] = c;
        putchar(c);
      }
    } else {
      /* ignore */
    }
  }
}

void VCP_Monitor(void)
{
  static uint32_t last_ms = 0U;
  uint32_t now = HAL_GetTick();
  uint8_t st = 0U;
  uint8_t newf;

  if ((now - last_ms) < VCP_MON_PERIOD_MS) {
    return;
  }
  last_ms = now;

  if (!vcp_mon) {
    return;
  }

  if (TLV612901_GetStatus(&st) != HAL_OK) {
    return;
  }

  newf = (uint8_t)(st & (TLV_ST_TSD | TLV_ST_ILIMBST |
                         TLV_ST_FL_LD | TLV_ST_ILIMPT) & (uint8_t)~vcp_latched);
  if (newf != 0U) {
    vcp_latched |= newf;
    if ((newf & TLV_ST_TSD) != 0U) {
      VCP_ShutdownOutput("THERMAL SHUTDOWN");
    } else if ((newf & TLV_ST_ILIMBST) != 0U) {
      VCP_ShutdownOutput("BOOST INPUT CURRENT LIMIT, check load/battery");
    } else if ((newf & TLV_ST_FL_LD) != 0U) {
      VCP_ShutdownOutput("INSTANTANEOUS OVERCURRENT, check for output short");
    } else {
      VCP_ShutdownOutput("BYPASS FET CURRENT LIMIT, check for short/overload");
    }
    return;
  }

  if ((st & TLV_ST_PGOOD) == 0U) {
    vcp_pg_bad++;
    if (vcp_pg_bad >= VCP_PG_FAULT_RUNS) {
      vcp_pg_bad = 0U;
      VCP_ShutdownOutput("POWER GOOD LOST, output out of regulation");
    }
  } else {
    vcp_pg_bad = 0U;
  }
}

/* USER CODE END 1 */
