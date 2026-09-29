# TLV612901-Function-STM32F1

Using the STM32 I2C to control the TLV612901.

STM32F103C8 + TLV612901 同步升壓晶片（5.5V / 11A，內建 bypass 與 I²C）驅動程式，
附 USB VCP 命令列介面。

📖 **完整文件請看 [userguide.md](userguide.md)** — 接線、編譯、燒錄、
19 條 CLI 指令、register 對照表、設計注意事項、疑難排解都在裡面。

## 快速開始

```bash
# 需要 arm-none-eabi-gcc、CMake >= 3.22、Ninja
cmake --preset Release
cmake --build --preset Release
# 產出 build/Release/tlv612901.hex
```

如果 CMake 找不到 STM32CubeF1 韌體套件，它會印出明確的修正方法。
也可以直接指定：

```bash
cmake --preset Release -DCUBE_FW=/path/to/STM32Cube_FW_F1_V1.8.7
```

或設定環境變數 `STM32CUBEDIR`。

## 快速指令

```
help                    指令列表
id                      DeviceID + I2C 位址
status                  狀態解碼
vout <mV>               設定輸出電壓 (2350..5500, 50mV 步進)
ilim <mA>               限流 (3500..11000, 500mA 步進)
mode auto|ultra|pwm     PFM / ultrasonic / forced PWM
en auto|bypass|off      電源狀態
```

## 接腳

| TLV612901 | STM32 | 說明 |
|---|---|---|
| EN (A1) | PA3 | 內建 800k 下拉，**必須主動驅動**；EN 拉低會讓 I²C 斷線，所以不當開關用 |
| GPIO (D1) | PA4 | ADDR strap：LOW=0x75 / HIGH=0x76 / floating=0x77 |
| SCL (B1) | PB6 | I²C1 |
| SDA (C1) | PB7 | I²C1 |

⚠️ 詳細注意事項（hiccup、共模、電池能力、電感選用）請務必讀 userguide.md 第 6 章。

## ⚠️ 一個容易踩的坑

**TLV612901 的 GPIO 預設功能是 `ADDR`，不是 `VSEL`**（`VSEL` 是 TLV612902 才有）。
因此輸出電壓只能透過 **`VOUTFLOORSET` (0x02)** 設定，
`VOUTROOFSET` (0x04) 對這顆晶片**完全無效**。

舊版 TLV61290 datasheet 的 Device Comparison Table 曾把 901 標為 `GPIO = VSEL`
（並註記 "Product preview"），該資訊已被 2026-08 的 TLV612901 專屬 datasheet 取代。

## 授權

本專案程式碼採用 [MIT 授權](LICENSE)。STM32Cube HAL 驅動程式版權屬
STMicroelectronics，請遵循其各自的授權條款。TLV612901 datasheet 版權屬
Texas Instruments。
