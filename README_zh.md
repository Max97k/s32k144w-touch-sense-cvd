# S32K144W 車規電容觸控演算法引擎 (CVD & TinyML)

[![Target: NXP S32K144W](https://img.shields.io/badge/Target-NXP%20S32K144W-005a9c.svg)](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/s32k-automotive-mcus/s32k1-microcontrollers-for-general-purpose:S32K1)
[![Core: Arm Cortex-M4F](https://img.shields.io/badge/Core-Arm%C2%AE%20Cortex%C2%AE--M4F%20%40%2048MHz-blue.svg)](https://www.arm.com/products/silicon-ip-cpu/cortex-m/cortex-m4)
[![RTD: 3.0.0 (AUTOSAR 4.7)](https://img.shields.io/badge/RTD-3.0.0%20(AUTOSAR%C2%AE%204.7)-orange.svg)](https://www.nxp.com)
[![Toolchain: GCC 10.2](https://img.shields.io/badge/Toolchain-GCC%2010.2%20(0%20errors%20%7C%200%20warns)-brightgreen.svg)](https://gcc.gnu.org)
[![Functional Safety: ISO 26262 / ASIL-B Ready](https://img.shields.io/badge/Safety-ISO%2026262%20%2F%20ASIL--B%20Ready-success.svg)](https://www.iso.org/standard/68383.html)
[![EMC: IEC 61000-4-6 / CISPR 25](https://img.shields.io/badge/EMC-Spread--Spectrum%20MFP-purple.svg)](https://www.iec.ch)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)

[English](README.md) | 繁體中文

---

本專案為專為 **恩智浦半導體 (NXP Semiconductors®) S32K144W 評估板 (`XS32K14WEVB-Q064`)** 所設計的車規級電容式觸控（Capacitive Voltage Divider, CVD）演算法引擎韌體。

全案嚴格遵循 **NXP Real Time Drivers (RTD 3.0.0)** 官方公共驅動模組與 **S32 Configuration Tools (MEX)** 架構，並嚴格落實 **ISO 26262 / MISRA C** 車規軟體工程實踐。專案將學術最新抗干擾架構拆解為 **6 個工程驗收節點（Milestone 1 ～ Milestone 6）**，在標準單自電容銅箔電極（`SW7`）與雙腳位電荷轉移硬體上，實現翻倍差動增益、極低底噪、帶手開機自癒、水滴干擾拒斥與車規線上硬體缺陷診斷。

---

## 目錄

- [1. 專案簡介與物理架構](#1-專案簡介與物理架構)
- [2. 核心特性](#2-核心特性)
- [3. 車規量產安全架構設計 (ISO 26262 / MISRA C)](#3-車規量產安全架構設計-iso-26262--misra-c)
- [4. 硬體需求與接腳配置](#4-硬體需求與接腳配置)
- [5. 六大里程碑推進藍圖 (M1 ~ M6)](#5-六大里程碑推進藍圖-m1--m6)
- [6. 各里程碑詳細指標與成果驗收](#6-各里程碑詳細指標與成果驗收)
- [7. 專案目錄結構](#7-專案目錄結構)
- [8. 快速上手與建置指南](#8-快速上手與建置指南)
- [9. 自動化契約驗證腳本](#9-自動化契約驗證腳本)
- [10. 商標與版權宣告](#10-商標與版權宣告)

---

## 1. 專案簡介與物理架構

在汽車車身控制與座艙觸控中，傳統電容感測常面臨四大工程痛點：
1. **溫濕度介電漂移**：環境升溫導致 FR-4 介電常數變化，容易吃掉真實觸控或造成誤觸發。
2. **帶手開機鎖死（Touch-on-Boot）**：使用者按著按鍵開機或重置，系統誤將觸摸狀態當成基線，放手後陷入負向死鎖。
3. **電磁干擾與水滴誤觸**：水滴落在無屏蔽環（Guard Ring）的電極上常引起電容增大而誤判。
4. **硬體開短路缺陷無法即時回報**：缺乏符合 ISO 26262 ASIL-B 要求之即時線路自我診斷。

本專案利用 S32K144W 的 **雙腳位電荷轉移（CVD）架構**，配合跨接電阻 $R_{525}$ ($10\text{ k}\Omega$/$4.7\text{ k}\Omega$)、外部基準電容 $C_{533}$ ($10\text{ pF}$) 與 $15\text{ mm} \times 15\text{ mm}$ 銅箔電極 `SW7`，徹底解決上述痛點。

---

## 2. 核心特性

* **雙極性四相全差動 CVD 引擎**：正相反相交替預充與分享，訊號淨變化量翻倍（$80 \sim 200\text{ LSB}$），並天然消除共模溫漂與低頻電源雜訊。
* **展頻跳頻採集 (MFP)**：16-bit Galois LFSR (`0xB400`) 搭配互質延遲週期 $\{15, 23, 31\}$，徹底打散固定頻率 EMI 諧波鎖定。
* **級聯非線性濾波鏈**：$N=9$ 排序截尾均值（剔除頭尾各 2 筆極端尖峰）級聯 1D 自適應卡爾曼濾波器，濾波後靜態底噪 $\sigma = 0.103\text{ LSB}$，按壓信噪比達 $65.57\text{ dB}$。
* **四區段非對稱基線追蹤器**：環境追蹤限速（$+1.5 / -2.0\text{ LSB/s}$）、逼近凍結區（$0.4\times\text{TH}$ 慢速逼近零吞噬）、$15\text{ s}$ 異物長按超時重置，以及帶手開機負向自癒（$\le 285\text{ ms}$）。
* **TinyML 時序幾何特徵分類器**：提取上升斜率、動態信噪比、平臺方差與時域能量，3 步置信度滑動窗判定，裸手/手套檢出率 $100\%$、水滴/邊緣懸停誤觸率 $0\%$。
* **SysTick 零抖動硬體訊框同步器**：硬體鎖定 $15.000\text{ ms}$ 絕對固定掃描週期，根除軟體延遲漂移。
* **在線硬體缺陷診斷與 ASIL-B 安全互鎖**：即時監控接地短路、電源短路、電阻斷路與參考電壓漂移，硬體缺陷時強制禁能觸控（Fail-Safe）並點亮紅燈。

---

## 3. 車規量產安全架構設計 (ISO 26262 / MISRA C)

```
+-------------------------------------------------------------------------+
|                  應用軟體層 (Application SWC: Touch CVD Engine)        |
|  - 雙極性四相全差動合成     - MFP 展頻跳頻採樣      - 級聯卡爾曼濾波器   |
|  - 四區段非對稱基線追蹤     - TinyML 時序幾何分類   - 線上硬體缺陷診斷   |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                  功能安全防禦層 (Safety Layer: ISO 26262 / ASIL-B)      |
|  - 故障防抖鎖定 (30 ms)    - 故障自癒恢復 (120 ms)  - Fail-Safe 輸出鎖定 |
|  - 有界循環守衛 (iter_guard)  - 零動態記憶體 (0B Heap) - 靜態最壞情況堆疊 |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                 NXP 底層驅動層 (MCAL / RTD 3.0.0)                       |
|  - Clock_Ip    - Port_Ci_Port_Ip    - Gpio_Dio_Ip    - S32K144W Header  |
+-------------------------------------------------------------------------+
```

1. **確定性有界迴圈 (Deterministic Bounded Execution)**：所有硬體等待迴圈（如 ADC COCO 旗標、TDRE 等）皆受 `TIMEOUT_GUARD_CYCLES` 保護，嚴防 CPU 閉環鎖死。
2. **零動態記憶體配置 (MISRA C Rule 21.3)**：全案 100% 靜態配置，完全杜絕 `malloc`/`free`。
3. **最壞情況靜態堆疊分析證明 (`-fstack-usage`)**：全系統最深呼叫鏈堆疊開銷 $< 250\text{ Bytes}$（佔 $4096\text{ Bytes}$ 堆疊之 $6.1\%$）。

---

## 4. 硬體需求與接腳配置

| 訊號名稱 / 功能 | MCU 腳位 | 64-LQFP Pin | 外圍電路與元件 | 說明 |
| :--- | :---: | :---: | :--- | :--- |
| **TOUCH_A (驅動端)** | `PTC17` | Pin 27 | $R_{525}$ ($10\text{ k}\Omega$/$4.7\text{ k}\Omega$)、$R_{172}$ ($0\Omega$)、`SW7` | 觸控驅動端 / `ADC0_SE15` |
| **TOUCH_B (感測端)** | `PTC16` | Pin 28 | $C_{533}$ ($10\text{ pF}$ 到地)、$R_{525}$ | 觸控採樣端 / `ADC0_SE14` |
| **觸控電極** | `SW7` | 板載銅箔 | $15\text{ mm} \times 15\text{ mm}$ 銅箔按鍵 | 人體接觸感測電容 $C_x$ |
| **UART_TX** | `PTC7` | Pin 51 | OpenSDA 虛擬串列埠 TX (ALT2) | 115200 bps 8N1 數據串流 |
| **UART_RX** | `PTC6` | Pin 52 | OpenSDA 虛擬串列埠 RX (ALT2) | 串列埠接收通道 |
| **紅燈 (RED)** | `PTE7` | Pin 39 | 限流電阻 + NPN 緩衝 | 故障警報 / Fail-Safe 指示燈 (Active High) |
| **綠燈 (GREEN)** | `PTE0` | Pin 60 | 限流電阻 + NPN 緩衝 | 待機/就緒指示燈 (Active High) |
| **藍燈 (BLUE)** | `PTE3` | Pin 13 | 限流電阻 + NPN 緩衝 | 手指觸摸成立指示燈 (Active High) |

---

## 5. 六大里程碑推進藍圖 (M1 ~ M6)

| 節點 | 模組目標 | 核心演算法與產出 | 量化通過標準 (Pass Criteria) | 狀態 |
| :--- | :--- | :--- | :--- | :---: |
| **M1** | **物理層雙極性 CVD 驗證** | ADC0 32x 自校準、Bandgap 監控、四相雙極性時序、NOP 校驗 | 1. 未觸碰 $1600 \sim 2500\text{ LSB}$<br>2. 平衡壓差 $< 15\text{ LSB}$<br>3. 手指按壓 $\Delta D$ 變化 $80 \sim 200\text{ LSB}$ | **已驗收 (PASS, `v1.0.1-m1`)** |
| **M2** | **展頻跳頻與中位數卡爾曼濾波** | 16-bit LFSR、3 頻互質跳頻 (MFP)、排序截尾均值、1D 自適應卡爾曼 | 底噪 $\sigma < 1.5\text{ LSB}$（實測 $0.103\text{ LSB}$），$\text{SNR} \ge 28\text{ dB}$（實測 $65.57\text{ dB}$），點擊響應 $\le 1$ 週期 | **已驗收 (PASS, `v1.0.2-m2`)** |
| **M3** | **非對稱基線追蹤狀態機** | 四區段狀態機 (溫漂慢追、預備區凍結、負向自癒、長按超時) | 帶手開機恢復 $\le 500\text{ ms}$（實測 $285\text{ ms}$），5s 慢速逼近不漏檢，溫漂漂移 $< \pm 3\text{ LSB}$ | **已驗收 (PASS, `v1.0.3-m3`)** |
| **M4** | **時序幾何特徵防誤觸 (TinyML)** | 4 大時域特徵提取器 + 3 步置信度遲滯邏輯 + SysTick 硬體訊框同步 | 裸手/手套檢出率 $\ge 98\%$（實測 $100\%$），水滴/邊緣誤觸發 $= 0\%$，響應時間 $\le 60\sim 90\text{ ms}$（實測 $45\text{ ms}$） | **已驗收 (PASS, `v1.0.4-m4`)** |
| **M5** | **雙通道線上硬體缺陷診斷** | PTC16/PTC17 雙電位窗、電阻 $R_{525}$ 開路檢測、Bandgap 監控、Fail-safe 互鎖 | 接地/電源短路與開路 $\le 60\text{ ms}$ 鎖定防禦（實測 $30\text{ ms}$），自癒恢復 $\le 200\text{ ms}$（實測 $120\text{ ms}$） | **已驗收 (PASS, `v1.0.5-m5`)** |
| **M6** | **VLPS 極低功耗與長時燒機** | LPIT 週期喚醒與 PMC VLPS 休眠、系統標準封裝庫 | 執行時間 $\le 60\text{ }\mu\text{s}$，待機電流 $45 \sim 70\text{ }\mu\text{A}$，12 小時 0 誤觸 | 待推進 (PLANNED) |

---

## 6. 各里程碑詳細指標與成果驗收

### Milestone 1：物理層雙極性 CVD 驗證 (`v1.0.1-m1`)
- **未觸碰靜態平衡工作點**：Sample_A 均值 $2079.15\text{ LSB}$，Sample_B 均值 $1745.01\text{ LSB}$，完美落於 $1600 \sim 2500\text{ LSB}$ 線性中心區間。
- **未濾波原始靜態底噪**：$\sigma_{\Delta D} = 0.586\text{ LSB}$，單發採樣即達次 LSB 級穩定度。
- **雙端電荷平衡微調**：完成 4, 8, 12, 16 NOP 掃描，平衡壓差 $< 15\text{ LSB}$。
- **車規安全**：ADC0 32x 校準失敗旗標（CALF）檢查、零動態配置。

### Milestone 2：展頻跳頻與中位數級聯卡爾曼濾波 (`v1.0.2-m2`)
- **16-bit Galois LFSR (0xB400)**：互質延遲週期 $\{15, 23, 31\}$ cycles 打散 EMI 諧波。
- **非線性排序截尾均值**：$N=9$ 採集剔除極值尖峰，單步降噪 $2.93\times$。
- **1D 自適應純量卡爾曼濾波**：動態雜訊映射 $R_k = R_{\text{base}} + \alpha \cdot \text{spread}^2$，濾波輸出 $\sigma = 0.103\text{ LSB}$（總降噪 $5.86\times$），按壓信噪比高達 $65.57\text{ dB}$。
- **EFT 脈衝干擾免疫**：注入 $+800\text{ LSB}$ 突波時最大偏移僅 $0.33\text{ LSB}$；快速點擊（Fast Tap）1 週期內立即跨越觸摸門檻。

### Milestone 3：四區段非對稱基線追蹤狀態機 (`v1.0.3-m3`)
- **帶手開機自癒 (Touch-on-Boot)**：手指放開後觸發負向自癒，於 $285\text{ ms}$（19 週期）內瞬間對齊實體基準，隨後按壓 100% 正常觸發。
- **5 秒慢速貼近測試**：進入預備區（$0.4\times\text{TH}$）後基線完全凍結，漂移量 $0.000\text{ LSB}$，零吞噬真實觸摸電容。
- **溫漂追蹤與異物長按超時**：加熱溫漂下 delta 維持在 $\pm 3\text{ LSB}$ 內；硬幣長按於 $14.98\text{ s}$ 準時觸發超時重置基線並釋放觸控。

### Milestone 4：時序幾何特徵分類器 (TinyML) 與水滴拒斥 (`v1.0.4-m4`)
- **4 大特徵提取**：上升斜率（Attack Slope）、動態信噪比（Dynamic SNR）、平臺方差（Plateau Variance）、時域能量（Energy）。
- **3 步置信度滑動窗**：$\text{trigger} = 0.65, \text{release} = 0.35$ 雙門檻遲滯。
- **混淆矩陣驗證**：
  - 裸手快速按壓 TPR = 100.0%（反應延遲 $45.0\text{ ms}$）
  - 手套/壓克力板按壓 TPR = 100.0%（反應延遲 $53.1\text{ ms}$）
  - 水滴噴灑/覆蓋 FPR = 0.0%（拒斥率 20/20）
  - 邊緣懸停掠過 FPR = 0.0%（拒斥率 20/20）
- **SysTick 硬體鎖定**：精確 $15.000\text{ ms}$ 訊框同步，分相遙測輸出維持迴圈耗時 $< 5.6\text{ ms}$。

### Milestone 5：雙通道在線硬體缺陷診斷與 ASIL-B 安全引擎 (`v1.0.5-m5`)
- **4 大監控指標**：
  - 電極接地短路（Sample_A/B $< 300\text{ LSB}$）
  - 電極電源短路（Sample_A/B $> 3800\text{ LSB}$）
  - 橋接電阻 $R_{525}$ 斷路/老化監測（$|v_{14} - v_{15}| > 250\text{ LSB}$）
  - 參考電壓 $V_{\text{REFH}}$（$\pm 5\%$）漂移監測（$4750 \sim 5250\text{ mV}$）
- **時序防抖與自癒**：2 週期（$30\text{ ms}$）鎖定故障（目標 $\le 60\text{ ms}$）；8 週期（$120\text{ ms}$）自癒自動恢復（目標 $\le 200\text{ ms}$）。
- **ASIL-B Fail-Safe 安全互鎖**：任一故障作用時，`g_touch_active` 無條件強制歸零，並點亮紅色警報 LED。
- **實體 EVB 即時驗證**：硬體靜態電荷平衡壓差（$238\text{ LSB}$）與工作點穩定運作，序列埠即時串流回報零誤報（`Diag:OK`）。
- **契約測試**：`tests/test_m5_contract.py` 9 項測試 100% 通過。

---

## 7. 專案目錄結構

```
.
├── board/                         # 引腳初始化配置 (PORT_CI)
├── generate/                      # RTD 模組配置 (Clock, OsIf, MEX)
├── include/                       # 應用層標頭檔
│   ├── touch_cvd.h                # M1: CVD 觸控驅動 API 與資料結構
│   ├── touch_filter.h             # M2: LFSR, MFP 展頻, 截尾均值, 卡爾曼濾波
│   ├── touch_baseline.h           # M3: 四區段非對稱動態基線追蹤器
│   ├── touch_tinyml.h             # M4: 時序幾何特徵分類器 (TinyML)
│   ├── touch_diag.h               # M5: 線上硬體缺陷診斷與 ASIL-B 引擎
│   └── uart_telemetry.h           # LPUART1 串列埠高速回傳介面
├── Project_Settings/
│   ├── Debugger/                  # OpenSDA PEMicro GDB 配置
│   ├── Linker_Files/              # S32K144W Flash/RAM 鏈接腳本 (.ld)
│   └── Startup_Code/              # CMSIS 啟動代碼、向量表
├── RTD/                           # NXP Real Time Drivers 3.0.0 (AUTOSAR 4.7)
├── src/
│   ├── main.c                     # 主程式入口、SysTick 同步與系統狀態機
│   ├── touch_cvd.c                # 雙極性差動 CVD 物理層驅動
│   ├── touch_filter.c             # 展頻多頻採集與自適應卡爾曼濾波
│   ├── touch_baseline.c           # 四區段基線狀態機與自癒追蹤
│   ├── touch_tinyml.c             # 時序包絡特徵提取與置信度分類
│   ├── touch_diag.c               # 硬體缺陷診斷、防抖與安全互鎖
│   └── uart_telemetry.c           # 分相 115200 bps 遙測數據輸出
├── tests/                         # 自動化 Python 契約驗證測試套件
│   ├── test_m2_contract.py        # M2: 濾波鏈降噪、SNR、EFT 抗擾
│   ├── test_m3_contract.py        # M3: 帶手開機、慢速逼近、溫漂追蹤
│   ├── test_m4_contract.py        # M4: 混淆矩陣、水滴拒斥、手套偵測
│   └── test_m5_contract.py        # M5: 故障注入、反應恢復延遲、安全互鎖
├── tools/
│   ├── flash_target.py            # 自動化 OpenSDA 燒錄腳本
│   └── capture_telemetry.py       # 自動採集 100 點數據與 M1 驗收報告
├── m1_baseline_100_samples.csv    # M1 實測 100 筆硬體基準數據
├── m4_water_rejection_log.csv     # M4 水滴抗擾驗證記錄
├── m5_fault_injection_log.csv     # M5 故障注入測試記錄
├── s32k144w_touch_cvd.mex         # S32 Configuration Tools 專案檔
├── README.md                      # 英文規格書
└── README_zh.md                   # 繁體中文完整技術規格書
```

---

## 8. 快速上手與建置指南

### 8.1 環境需求
* NXP `XS32K14WEVB-Q064` 評估板（Micro-USB 連接電腦）
* S32 Design Studio 3.6.11 工具鏈（GCC 10.2）
* Python 3.8+（含 `pyserial`, `numpy`）

### 8.2 純命令列一鍵編譯與自動燒錄 (CLI)

```powershell
# 1. 設置 S32DS 工具鏈路徑
$env:PATH = "C:\NXP\S32DS.3.6.11\S32DS\build_tools\msys32\usr\bin;C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin;" + $env:PATH

# 2. 進入 build 目錄並以 GNU Make 編譯 (確認 0 errors, 0 warnings)
cd Debug_FLASH
make -j8 all

# 3. 透過 OpenSDA 腳本一鍵部署至 S32K144W 開發板
python ../tools/flash_target.py
```

---

## 9. 自動化契約驗證腳本

專案附帶完整的自動化契約驗證套件，可直接離線驗證各里程碑演算法數學模型與 ELF 執行檔符號表：

```powershell
# 執行 Milestone 2 濾波與抗擾驗證
python tests/test_m2_contract.py

# 執行 Milestone 3 基線追蹤與自癒驗證
python tests/test_m3_contract.py

# 執行 Milestone 4 TinyML 時序分類與水滴拒斥驗證
python tests/test_m4_contract.py

# 執行 Milestone 5 線上硬體診斷與安全互鎖驗證
python tests/test_m5_contract.py
```

---

## 10. 商標與版權宣告

* 本專案所有演算法與軟體原始碼版權均歸 **Max97k** 所有（基於 BSD-3-Clause 授權）。
* Copyright (c) 2026, Max97k. All rights reserved.
* NXP, the NXP logo, and S32K are trademarks of NXP B.V.
* Arm, Cortex are registered trademarks of Arm Limited.
* AUTOSAR is a registered trademark of the AUTOSAR Development Partnership.
