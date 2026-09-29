# TLV612901 專案使用指南

STM32F103C8 + TLV612901（5.5V / 11A 同步升壓晶片，含 bypass 模式與 I²C 介面）開源韌體。
透過 USB VCP 提供命令列介面，可調整輸出電壓、限流值、工作模式，並在故障時自動斷開輸出。

---

## 目錄

1. [系統架構](#1-系統架構)
2. [硬體接線](#2-硬體接線)
3. [編譯與燒錄](#3-編譯與燒錄)
4. [VCP 命令列](#4-vcp-命令列)
5. [Register 對照表](#5-register-對照表)
6. [設計注意事項](#6-設計注意事項)
7. [疑難排解](#7-疑難排解)
8. [原始碼地圖](#8-原始碼地圖)

---

## 1. 系統架構

```
 18650 (2.7~4.2V)          STM32F103C8
      │                         │
      ├── VIN/A2-4          PA3 ────► EN   (A1)   永久拉高
      │                         ├── PA4 ────► GPIO (D1)  ADDR strap → 0x75
      │                         ├── PB6 ────► SCL  (B1) ──┐
      │                         └── PB7 ────► SDA  (C1) ──┤
      │                                                ├──► TLV612901
   L 470nH            SW/C2-4                          │    I²C
      │                                                │
      ├──── COUT 4×22µF ────► VOUT/B2-4 ──── 5.0V ────► 負載
      │                                                │
      └──────────────────── GND/D2-4 ◄─────────────────┘

  USB ────► VCP (CDC) 命令列
```

供電路徑：電池 → TLV612901 → 5V。Vin 高於 Vout 時晶片自動進入 **bypass**（MOS 直接導通，
Vout 跟隨 Vin，效率最高）；Vin 掉到門檻以下才切換成 **boost**。這樣能完整用掉電池容量。

---

## 2. 硬體接線

### 2.1 必要接線

| TLV612901 球位 | 名稱 | 接到 STM32 | 說明 |
|---|---|---|---|
| A1 | **EN** | **PA3** | 內建 800kΩ 下拉，必須主動驅動。**見 2.2 警告** |
| D1 | GPIO | **PA4** | ADDR strap，開機前就必須固定電位 |
| B1 | SCL | PB6 | I²C1，開漏 |
| C1 | SDA | PB7 | I²C1，開漏 |
| A2-4 | VIN | 電池正極 | 建議加 22µF X5R 10V 輸入電容 |
| B2-4 | VOUT | 負載 | **4×22µF X5R 10V**（見 6.3） |
| C2-4 | SW | 電感節點 | 開關節點，走線要短 |
| D2-4 | GND | GND | 輸出電容接地要靠近本腳位 |

### 2.2 ⚠️ EN 腳位：不要用來做開關

**TLV612901 沒有 reset 腳位，只有 EN。** 更重要的是：

> **EN 拉低會讓 I²C 整條斷掉。** 關機電流只有 0.55µA，連 I²C transceiver 都無法供電。
> DS §7.5.2：如果想「功率級關掉但 I²C 還能通」，必須 **EN 保持高**，然後用
> `CONFIG[6:5] = 11`（I2C shutdown mode，待機電流 16µA）。

所以本專案的設計是：

* `PA3` 開機就拉高，**永遠不當開關用**
* 開／關用 `CONFIG[6:5] ENABLE` 控制

| ENABLE | 模式 | VOUT | 備註 |
|---|---|---|---|
| `00` / `01` | 自動 boost/bypass | 5.0V | 開機預設 |
| `10` | 強制 bypass | 跟隨 Vin | PGOOD 腳位無定義 |
| `11` | I2C shutdown | 0V | 功率級關閉，**I²C 保持作用** ← 故障斷電用這個 |

### 2.3 ⚠️ ADDR 腳位（PA4）

I²C 位址在**開機程序完成時就鎖死**，之後無法用軟體更改。

| GPIO 狀態 | I²C 位址 |
|---|---|
| Low（GND） | `0x75` |
| High | `0x76` |
| **Floating** | `0x77` |

* 韌體開機會自動掃描 `0x75 / 0x76 / 0x77` 並印出實際找到的位址，所以接錯也能知道。
* **強烈建議在硬體把 D1 直接接地**（或接 10k 下拉）。因為 STM32 在 reset 期間
  GPIO 會變成 hi-Z，此時若沒有外部下拉，位址會變成 `0x77`。用 PA4 驅動只是多一层保險。

### 2.4 邏輯電位相容性

晶片是 1.2V I/O 邏輯介面，但參數對 3.3V 完全安全：

* 絕對最大額定：EN / GPIO / SCL / SDA 對地 **−0.3 ~ 7V**（3.3V 安全）
* `V_IH` 最小 **0.82V**、`V_IL` 最大 **0.33V** → 3.3V 有約 2.5V 雜訊margin
* SDA 上拉電阻的電壓軌不可超過 7V，用 3.3V 或 1.8V 即可

---

## 3. 編譯與燒錄

需要 `arm-none-eabi-gcc`、`cmake` ≥ 3.22、`ninja`。

```bash
# Debug（開發用，-O0 -g3）
cmake --preset Debug
cmake --build --preset Debug

# Release（量產燒錄用，-Os）
cmake --preset Release
cmake --build --preset Release
```

### Flash 用量

| Preset | RAM | Flash | 佔比 |
|---|---|---|---|
| Debug | 7272 B / 20 KB | 53724 B / 64 KB | **82.0%** |
| Release | 7272 B / 20 KB | 27536 B / 64 KB | **42.0%** |

> **Debug 82% 的原因是 `-O0`，不是 printf。** 量測數據：
>
> | 項目 | Debug `-O0` | Release `-Os` |
> |---|---|---|
> | `vcp_cli.c` | 5822 B | 2786 B |
> | `tlv612901.c` | 2045 B | 1196 B |
> | newlib 格式化函式 | 1154 B | 1154 B |
> | 整個 firmware | 53724 B | 27536 B |
>
> `printf` 家族（`_printf_i` / `_vsnprintf_r` / `_strtol` 等）**兩個 preset 都是 1154 B**，
> 因為 newlib 是預先以 `-Os` 編譯好的函式庫，不受你的編譯旗標影響。
> 真正被 `-O0` 灌大的是本專案自己的程式碼（7867 B → 3982 B）。
>
> 換句話說：**printf 只佔 2% 的 flash，別為了它改寫成手寫 formatter。**
> 如果 Debug 空間不夠，正確做法是改用 `arm-none-eabi-objcopy --strip-debug` 或直接用 Release。
>
> **燒錄產品請用 Release**（42%，還有 26KB 空間）。Debug 會把 .text/.rodata 全部留下，
> 難以兼顧除錯資訊與體積。

### 產出檔案

`build/<Preset>/` 底下會產生：

| 檔案 | 用途 |
|---|---|
| `tlv612901.elf` | 燒錄 + 符號除錯 |
| `tlv612901.bin` | ST-Link Utility / OpenOCD 燒錄 |
| `tlv612901.hex` | 燒錄 |
| `tlv612901.map` | 記憶體使用分析 |

---

## 4. VCP 命令列

插上 USB 後開終端機（macOS 用 `screen /dev/tty.usbmodem*`、Windows 用 Tera Term / PuTTY，
**115200 8N1**，雖然 VCP 實際上會忽略 baud 設定），輸入 `help` 看指令列表。

### 4.1 資訊與除錯

| 指令 | 說明 |
|---|---|
| `help` | 指令列表 |
| `id` | 讀 DeviceID（應為 `0x70`）與實際 I²C 位址 |
| `dump` | 依序讀出 register `0x00`~`0x07` |
| `status` | 讀 STATUS 並解碼各旗標 |
| `rd <0..7>` | 原始讀取單一 register |
| `wr <0..7> <0..255>` | 原始寫入單一 register |
| `reset` | 軟體 reset，register 回到出廠值 |

### 4.2 輸出電壓

| 指令 | 說明 |
|---|---|
| `vout` | 顯示目前 VOUTFLOORSET 設定值 |
| `vout <mV>` | 設定輸出電壓，範圍 2350~5500mV，**50mV 步進** |

```
> vout
VOUT setpoint = 5.00 V (code 0x2B)

> vout 4800
VOUT setpoint -> 4.80 V (code 0x2F)
```

> 範圍以外的會被拒絕，例如 `vout 4370` 會回 ERR（不是 50mV 倍數）。
> 改變輸出電壓會直接改變供應給下游的電軌，**變更前請確認負載可承受**。

### 4.3 限流

| 指令 | 說明 |
|---|---|
| `ilim` | 顯示 boost 平均限流 |
| `ilim <mA>` | 設定 boost 平均限流，3500~11000mA，500mA 步進 |
| `ilimpt <0..3>` | 設定 bypass 限流：0=4A、1=6A、2=8A、3=10A |

```
> ilim 5500
ILIM_BOOST -> 5500 mA (code 0x1)
```

> 限流 **code 順序不連續**：`0xC` = 11A（最大），`0xD` 卻跳回 3.5A（最小）。
> 韌體用查表換算，不要用 `code × 0.5 + 3.5` 推算。

### 4.4 工作模式

| 指令 | 說明 |
|---|---|
| `mode auto` | Auto PFM（**出機預設**，輕載效率最高，但會有可聞噪音） |
| `mode ultra` | Ultrasonic，fsw 下限 23kHz，**無可聞噪音** |
| `mode pwm` | Forced PWM，fsw 下限 375kHz，漣波最低 |
| `en auto` | 自動 boost/bypass（正常運作） |
| `en bypass` | 強制 bypass，VOUT 跟隨 Vin |
| `en off` | 功率級關閉，I²C 保持作用（VOUT = 0V） |
| `hiccup <0\|1>` | 輸出短路時的 hiccup 重試（預設 1，建議保持開啟） |
| `dischg <0\|1>` | 關機時 VOUT 主動放電（預設 1） |
| `ssfm <0\|1>` | Forced PWM 下的展頻（預設 0） |
| `mon <0\|1>` | 開／關週期性狀態監控（預設 1） |

> `dischg=1` 時晶片內建約 **90Ω** 的放電路徑，5V 會有約 55mA 持續放電電流。
> 這在 power multiplexing 應用會造成問題（關機中的軌道會被拉低）。

### 4.5 故障處理行為

`mon` 開啟時，韌體每 250ms 讀一次 STATUS。偵測到下列情況會 **VCP 印出原因** 並
**自動切斷輸出**（`CONFIG[6:5] = 11`，I²C 仍可通）：

| 觸發旗標 | 印出訊息 |
|---|---|
| `TSD` (bit 7) | `THERMAL SHUTDOWN` |
| `ILIMBST` (bit 2) | `BOOST INPUT CURRENT LIMIT, check load/battery` |
| `FL_LD` (bit 1) | `INSTANTANEOUS OVERCURRENT, check for output short` |
| `ILIMPT` (bit 3) | `BYPASS FET CURRENT LIMIT, check for short/overload` |
| `PGOOD` 連續 4 次為 0 | `POWER GOOD LOST, output out of regulation` |

```
[TLV612901] OUTPUT DISABLED (INSTANTANEOUS OVERCURRENT, check for output short). I2C stays alive.
[TLV612901] STATUS=0x03 PGOOD=1 OPMODE=BOOST VOUT_UP=1 ILIMBST=0 FL_LD=1 ILIMPT=0 TSD=0
```

清除故障後用 `en auto` 重新啟動，或 `reset` 恢復出廠值。

> **注意**：`STATUS` 中 `TSD` / `VOUT_START` / `ILIMPT` / `ILIMBST` / `FL_LD`
> 皆是 **read-to-clear**。韌體會 latch 住事件避免輪詢漏掉，但若你自己另外寫 code
> 大量輪詢，這些旗標會被清掉。

---

## 5. Register 對照表

位址 `0x75`（ADDR 拉低）。全部為 8-bit，寫入時 7:6 為 reserved（寫入忽略、讀回 0）。

| 位址 | 名稱 | 內容 |
|---|---|---|
| `0x00` | DeviceID | `0x70`（ManufactureID=0111, DeviceID=0000），唯讀 |
| `0x01` | CONFIG | `RESET`(7) / `ENABLE`(6:5) / `HICCUP_MODE`(4) / `DISCHG`(3) / `SSFM`(2) / `MODE_CTRL`(1:0) |
| `0x02` | VOUTFLOORSET | `VOUTFLOOR_TH`(5:0)，**這顆 901 唯一有效的輸出電壓 register** |
| `0x03` | ILIMBSTSET | `ILIM_BOOST`(3:0)，boost 平均限流 |
| `0x04` | VOUTROOFSET | `VOUTROOF_TH`(5:0)，**在 TLV612901 上無效**（見 6.1） |
| `0x05` | STATUS | 唯讀旗標，見 4.5 |
| `0x06` | ILIMPTSET | `ILIM_FPT`(5:3) / `ILIM_APT`(2:0)，bypass 限流，僅 4/6/8/10A 有效 |
| `0x07` | BSTLOOP | `TI_internal`(7:3) 勿動 / `RC`(2:0) 內部補償電阻，**請勿修改** |

### 5.1 VOUT 代碼對照

出廠值 `0x2B` = 5.00V。

| Code | Volt | Code | Volt |
|---|---|---|---|
| `0x00` | 2.85 | `0x20` | 4.45 |
| `0x08` | 3.25 | `0x28` | 4.85 |
| `0x10` | 3.65 | **`0x2B`** | **5.00**（出廠值） |
| `0x18` | 4.05 | `0x30` | 5.25 |
| | | `0x35` | 5.50（上限） |

> ⚠️ **code 從 `0x36` 開始會往回跳**：`0x35`=5.50V，但 `0x36`=2.80V，
> 之後一路降到 `0x3F`=2.35V。所以 `2.35~2.80V` 這段要用 `0x36~0x3F`，
> 用 `0x36` 以上的 code 當「更高的電壓」會得到完全相反的結果。
> 韌體 `Core/Src/tlv612901.c` 用 64 項 lookup table 處理這個不連續。

---

## 6. 設計注意事項

### 6.1 ⚠️ VOUTROOFSET 在 901 上無效

DS p.13 §7.3.1 原文：

> *"For TLV612901, the GPIO is configured as ADDR function, **only set the output
> voltage by VOUTFLOORSET** (default 5.0V)."*

DS p.24 Table 8-1 對 `0x04` 的描述也註明 *"(only valid when GPIO is configured as
VSEL function)"*，而 **VSEL 功能是 TLV612902 才有的**。

> 常見混淆來源：舊版 TLV61290 datasheet 的 Device Comparison Table 曾把 901 標為
> `GPIO = VSEL`（且標註 "Product preview"）。該資訊已被 2026-08 的 TLV612901
> 專屬 datasheet 取代。**以 TLV612901 datasheet 為準。**

### 6.2 切換頻率不是固定的

這顆是 **hysteretic 電流模式控制**，沒有振盪器。DS §7.3.2：

> *"The TLV612901 uses a hysteretic control scheme, and TLV612901 maintains a
> **constant inductor ripple current** in the range of **1.0A**. Therefore,
> **the frequency is not fixed** and determined by the operation condition."*

推導式：`fsw = Vin × D / (L × ΔI_L)`，`D = 1 − Vin/Vout`

以 470nH、ΔI_L ≈ 1.0A 估算：

| Vin | Duty D | fsw |
|---|---|---|
| 2.5V | 0.50 | ≈ 2.66 MHz（峰值） |
| 2.7V | 0.46 | ≈ 2.64 MHz |
| 3.0V | 0.40 | ≈ 2.55 MHz |
| 3.3V | 0.34 | ≈ 2.39 MHz |
| 3.7V | 0.26 | ≈ 2.05 MHz |
| 4.2V | 0.16 | ≈ 1.43 MHz |

> ⚠️ 注意 fsw 是 **Vin 的拋物線函數，峰值在 Vin = Vout/2 = 2.5V**。
> 電壓**越低** fsw 反而**越高**（Vin 接近 Vout 時 duty→0，fsw→0）。
> 這代表在最需要大電流的低電壓區，電感工作頻率最高、磁芯損耗最大 ——
> 這才是電感選型要驗證的 operating condition，而不是 datasheet 的 100kHz 量測頻率。

各模式對 fsw 的**下限**（DS §6.5）：

| 模式 | fsw 下限 | 用途 |
|---|---|---|
| Auto PFM | 約 **20Hz**（無下限） | 效率最高，**但會發出可聞噪音** |
| Ultrasonic | **23 kHz**（min） | 消除可聞噪音 |
| Forced PWM | **375 kHz**（min，typ 440kHz） | 最低漣波、最低 EMI |

> 沒有任何模式有標示 fsw **上限**。設計輸入／輸出濾波時要把高頻段考慮進去。

### 6.3 電感與電容

**推薦值：470nH**（DS §6.3 有效範圍 330~560nH，內部補償以 470nH 最佳化）。
DS 特別註明內部補償是針對 `L = 470nH` 且 `CO_eff = 15µF` 調的。

#### 現有電感 QI630M-R47M-T 可以繼續用

| 項目 | 規格 | 評估 |
|---|---|---|
| 電感值 | 0.47µH ±20% | ✅ 在推薦範圍正中 |
| Idc | 16 A | ✅ 遠超需求 |
| Isat | 20 A（電感值降 30% 點） | ✅ 遠超需求 |
| DCR | 4.1 mΩ max | ✅ 極低 |
| 磁芯 | 金屬 composite，屏蔽 | ✅ 2~4MHz 適配良好 |

> **「100kHz」是量測頻率，不是最高工作頻率。** Datasheet 寫的是
> *"Testing frequency: 100KHz / 1V"*，即 LCR 表的量測條件。
> 判斷高頻適配要看**磁芯材質**（金屬 composite / NiZn 鐵氧體在高頻優於 MnZn），
> 不是看這個數字。

#### 電流估算（Vout=5.0V, ΔI_L≈1.0A）

`IL_avg = Iout × Vout / Vin`，`IL_peak = IL_avg + 0.5A`

| Vin | Iout 3A（連續） | Iout 4.5A（峰值） |
|---|---|---|
| 3.7V | 4.05A avg / **4.55A peak** | 6.08A avg / **6.58A peak** |
| 3.0V | 5.00A avg / **5.50A peak** | 7.50A avg / **8.00A peak** |
| 2.7V | 5.56A avg / **6.06A peak** | 8.33A avg / **8.83A peak** |

#### 輸出電容

DS §9.2.1.2.2 對 5V/3A 這類高脈衝負載建議 **4×22µF X5R 10V 0603**。

> ⚠️ **不要用 6.3V 額定的 22µF 0603。** DS 明確警告直流偏壓下有效電容量可能
> 掉到 10µF 以下。X5R + 10V 才有足夠餘裕。

DCR 損耗（QI630M，4.05A）：`I²R = 4.05² × 4.1mΩ ≈ 67mW`，可忽略。

### 6.4 hiccup mode 是什麼

**不是**給大輸出電容用的。它的用途是 **output short-to-ground 保護**，
並且和開機 pre-charge 階段**共用同一組 timer**（DS §7.4.11：*"The scheme of output
OSGP is same as the scheme in start-up phase"*）。

時序（DS §6.5）：

| 參數 | 值 |
|---|---|
| Hiccup on time | 1 ms |
| Hiccup off time | 19 ms |

也就是 **嘗試 1ms / 等待 19ms，約 5% duty 的重試迴圈**。短路時若不這樣做，
裝置會卡在永久限流重試而燒毀 MOSFET。短路解除後自動軟啟動恢復正常。

* 預設開啟（`CONFIG[4]` reset = 1），**建議保持開啟**。
* 關掉的話 datasheet 沒有說明替代行為，也沒有 SOA 數據，剩下的保護只剩
  限流 + 熱關斷（165°C），等於把有界的 5% duty 換成無界的發熱迴圈。
* ⚠️ 反過來說，**大 COUT 反而是 hiccup 的風險來源**：pre-charge 電流只有 0.5A，
  若 1ms 內 VOUT 充不到 Vin 就會被判 fault 而開始 hiccup，導致開機失敗。
  要解掉只能減少 COUT、提高 Vin 或減少開機負載。

### 6.5 限流值與電池能力

3A@5V 連續 = 15W 輸出，換算輸入約 4A（90% 效率下約 4.05A）。

> ⚠️ **15W 連續功率對單顆 18650 是很大的負擔。** 3400mAh × 3.7V = 12.6Wh，
> 在 90% 效率下需要 16.7W 輸入 → 理論上約 **45 分鐘**。
> 實際可用容量通常只有額定的 80~90%（負載下內阻放電），故 **約 35~40 分鐘**。
> 連續放電電流建議：
>
> * 一般 18650（如 LG HG2、三星 50E）連續上限約 3A，不夠
> * **需用高倍率電**：三星 30Q / 50S、LG M50LT（連續 5A 以上）
> * 4.5A 峰值對單顆 18650 也很吃緊，建議確認電芯規格或考慮 2P 並聯

選 `ilim 5500` 時請注意：`ILIM_BOOST` 是**平均**限流，電池快沒電時
（Vin < 3V）你的輸出電流會被壓到 3A 以下。若需要更大裕量，用 `ilim 8000`（出廠值）。

### 6.6 FPGA / RF 負載注意

Forced PWM 模式下電感電流會被強制連續，可能**反向流動到 −1A typ**
（DS §6.5 `ILIM_REVERSE`）。若下游對反向電流敏感（例如驅動 RF PA），
選電感與 layout 時要把這個反向擺幅算進去。

---

## 7. 疑難排解

### 開機印 `[TLV612901] NOT FOUND on I2C1`

依序檢查：

1. **ADDR 腳位電位** — 用 `id` 看不到東西代表根本沒 ACK。確認 D1 接地。
   若 MCU 開機前還沒驅動 PA4，位址會是 `0x77`。韌體有掃描三個位址，
   所以「沒找到」代表硬體真的沒回應。
2. **I²C 上拉電阻** — PB6/PB7 需要外部上拉電阻（典型 4.7k 上到 3.3V）。
   STM32 的內部上拉太弱。確認有裝。
3. **接線** — SCL/SDA 是否接反？TLV612901 的 SCL=B1、SDA=C1，**兩者不可對調**。
4. **供電** — 電池有電嗎？Vin 需 ≥ 1.8V。
5. **量測** — 用示波器看 SCL 有沒有時脈。若完全沒有波形，
   代表 STM32 沒發出交易（檢查 I²C1 是否被其他驅動佔用）。

### 燒錄後完全沒反應

多半是 `PA3` 沒有拉高。EN 內建 800kΩ 下拉，如果 PA3 沒有正確初始化，
晶片會停在關機狀態。此時 I²C 也不會回應。

### 可聞的嗡聲 / 嘣嘣聲

晶片切換頻率遠高於人耳可聞範圍，**噪音來自工作模式而不是電感**：

```
> mode ultra
```

Auto PFM 模式（出機預設）在輕載時 fsw 會掉到約 20Hz，這是設計如此以提高效率。
`mode ultra` 把 fsw 下限拉高到 23kHz 即可消除。
注意 ultrasonic 模式的 VOUT 會比 PWM 高約 **1%**。

若 `mode ultra` 後仍有聲音，可能來自：

* 輸出電容 ESR 造成的振盪
* layout 上 SW 節點的辐射
* 電感的機械共振（薄型屏蔽電感較常見）

### 輸出電壓設不上去

```
> vout
VOUT setpoint = 5.00 V (code 0x2B)
```

若讀回來對、但實測輸出不對，檢查 `status`：

* `OPMODE=0`（bypass）→ Vin 已經高於 VOUT 門檻，輸出跟隨 Vin，這是正常行為
* `PGOOD=0` → 輸出超出調節範圍，可能是負載過重或 Vin 太低
* 確認寫的是 `0x02` 不是 `0x04`（見 6.1）

### 輸出電壓每次開機都不對

Register 沒有掉電保存（DS 沒有提到 EEPROM）。每次上電都是出廠值。
若開機後 VOUT 不是 5.0V，代表 `TLV612901_Init()` 沒成功執行，
看開機訊息有沒有印出實際位址。

---

## 8. 原始碼地圖

| 檔案 | 職責 |
|---|---|
| `Core/Inc/tlv612901.h` | Register 位址、bit field 定義、code 對照常數、API 宣告、引腳配置 |
| `Core/Src/tlv612901.c` | I²C 讀寫、位址自動掃描、`TLV612901_Init()`、各參數 setter、status 解碼、code↔數值查表 |
| `Core/Inc/vcp_cli.h` / `Core/Src/vcp_cli.c` | VCP 命令列解析、`__io_putchar`（printf 出口）、週期性故障監控 |
| `Core/Src/gpio.c` | `USER CODE BEGIN 2` 內初始化 EN 與 ADDR |
| `Core/Src/main.c` | `USER CODE BEGIN 2` 內呼叫 `TLV612901_Init()`；主迴圈呼叫 `VCP_Monitor()` |
| `USB_DEVICE/App/usbd_cdc_if.c` | `CDC_Receive_FS()` 內把收到的位元組餵給 `VCP_Process()` |
| `cmake/stm32cubemx/CMakeLists.txt` | 新增 `tlv612901.c`、`vcp_cli.c` |

### 8.1 為什麼 code 放在 USER CODE 區塊

`main.c`、`gpio.c`、`usbd_cdc_if.c` 的修改都放在 CubeMX 的 `USER CODE BEGIN/END`
標記之間。**在 CubeMX 重新產生程式碼時這些區塊會被保留**，標記外的部分則會被覆蓋。

`tlv612901.h` / `vcp_cli.h` 裡的 pin 定義同樣放在 `USER CODE BEGIN Private defines`
區塊內。引腳對應若要更動，建議改 `.ioc` 檔的 GPIO label 讓 CubeMX 自動同步，
而不是手改 header。

### 8.2 已驗證 / 未驗證

| 項目 | 狀態 |
|---|---|
| Debug / Release 編譯 | ✅ 已驗證，零警告 |
| Flash / RAM 用量 | ✅ 已量測 |
| Register 位址與 bit field | ✅ 依 TLV612901 datasheet (2026-08) 逐項核對 |
| VOUT / ILIM 對照表 | ✅ 依 datasheet 表格 |
| I²C 通訊 | ⚠️ **未在實機驗證** |
| GPIO 時序與開機行為 | ⚠️ **未在實機驗證** |
| 熱／電流故障偵測 | ⚠️ **未在實機驗證**（需刻意製造故障） |

首次燒錄後建議先跑這幾條確認連線：

```
> id          # 應回 DeviceID=0x70 + 實際位址
> dump        # 0x02 應為 0x2B
> status      # PGOOD=1
> vout 4800   # 實測輸出應變 4.8V
> vout 5000   # 還原
```

---

## 參考資料

* TLV612901 datasheet, SLVSLZ3, 2026-08 —
  <https://www.ti.com/lit/ds/symlink/tlv612901.pdf>
* TLV61290 datasheet, SLVSI74A —
  <https://www.ti.com/lit/ds/symlink/tlv61290.pdf>
* QST QI630M series datasheet —
  <https://qstproducts.com/Inductor/QI630M.pdf>
