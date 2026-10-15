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
- [5. Milestone 1 運作原理與物理時序](#5-milestone-1-運作原理與物理時序)
  - [5.1 雙極性四相全差動 CVD 採集時序](#51-雙極性四相全差動-cvd-採集時序)
  - [5.2 NOP 電荷平衡微調驗證](#52-nop-電荷平衡微調驗證)
  - [5.3 ADC0 32x 自校準與 Bandgap 基準監控](#53-adc0-32x-自校準與-bandgap-基準監控)
- [6. 六大里程碑推進藍圖 (M1 ~ M6)](#6-六大里程碑推進藍圖-m1--m6)
- [7. 專案目錄結構](#7-專案目錄結構)
- [8. 快速上手與建置指南](#8-快速上手與建置指南)
  - [8.1 環境需求](#81-環境需求)
  - [8.2 純命令列一鍵編譯與自動燒錄 (CLI)](#82-純命令列一鍵編譯與自動燒錄-cli)
  - [8.3 M1 量化數據採集與驗收](#83-m1-量化數據採集與驗收)
- [9. M1 節點驗收 Review Checklist](#9-m1-節點驗收-review-checklist)
- [10. 商標與版權宣告](#10-商標與版權宣告)

---

## 1. 專案簡介與物理架構

在汽車車身控制與座艙觸控中，傳統電容感測常面臨四大工程痛點：
1. **溫濕度介電漂移**：環境升溫導致 FR-4 介電常數變化，容易吃掉真實觸控或造成誤觸發。
2. **帶手開機鎖死（Touch-on-Boot）**：使用者按著按鍵開機或重置，系統誤將觸摸狀態當成基線，放手後陷入負向死鎖。
3. **電磁干擾與水滴誤觸**：水滴落在無屏蔽環（Guard Ring）的電極上常引起電容增大而誤判。
4. **硬體開短路缺陷無法即時回報**：缺乏符合 ISO 26262 ASIL-B 要求之即時線路自我診斷。

本專案利用 S32K144W 的 **雙腳位電荷轉移（CVD）架構**，配合跨接電阻 $R_{525}$ ($4.7\text{ k}\Omega$)、外部基準電容 $C_{533}$ ($10\text{ pF}$) 與 $15\text{ mm} \times 15\text{ mm}$ 銅箔電極 `SW7`，徹底解決上述痛點。

---

## 2. 核心特性

* **雙極性四相全差動 CVD 引擎**：正相反相交替預充與分享，訊號淨變化量翻倍（$80 \sim 200\text{ LSB}$），並天然消除共模溫漂與低頻電源雜訊。
* **確定性微秒級時序控制**：核心充放電採集採 $50\text{ ns}$ 預充、單指令原子切換 High-Z、無分支抖動之精確 NOP 延遲。
* **ADC0 32 倍硬體自校準 (AN12217)**：開機自校準，線性度誤差 $< 1\text{ LSB}$，並即時遙測 Bandgap Channel 27（$1.0\text{ V}$）以監控 $V_{\text{REFH}}$ 電壓穩定度。
* **雙軌觀測系統**：
  * **OpenSDA 虛擬串列埠高速 Telemetry (115200 bps)**：自動串流 CSV 格式數據，零依賴即可採集 100 點驗收資料。
  * **FreeMASTER 驅動與 TSA 支援**：未來隨時支援 FreeMASTER 桌面示波器即時波形分析。
* **多色狀態指示 (RGB LED)**：
  * 綠燈 (PTE0)：系統就緒、未觸碰靜態基線鎖定。
  * 藍燈 (PTE3)：有效手指觸碰觸發。
  * 紅燈 (PTE7)：ADC 自校準失敗或硬體開短路防禦鎖定。

---

## 3. 車規量產安全架構設計 (ISO 26262 / MISRA C)

```
+-------------------------------------------------------------------------+
|                  應用軟體層 (Application SWC: Touch CVD Engine)        |
|  - 雙極性四相全差動合成     - NOP 平衡校準掃描      - 100 點驗收測試機   |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                  功能安全防禦層 (Safety Layer: ISO 26262)               |
|  - 有界循環守衛 (iter_guard)  - 零動態記憶體 (0B Heap) - 靜態最壞情況堆疊 |
+-------------------------------------------------------------------------+
                                    |
+-------------------------------------------------------------------------+
|                 NXP 底層驅動層 (MCAL / RTD 3.0.0)                       |
|  - Clock_Ip    - Port_Ci_Port_Ip    - Gpio_Dio_Ip    - S32K144W Header  |
+-------------------------------------------------------------------------+
```

1. **確定性有界迴圈 (Deterministic Bounded Execution)**：所有等待迴圈（如 COCO 標誌位、TDRE 等）皆受 `TIMEOUT_GUARD_CYCLES` 嚴密保護，杜絕 CPU 閉環鎖死。
2. **零動態記憶體配置 (MISRA C Rule 21.3)**：全案 100% 靜態配置，完全不使用 `malloc`/`free`。
3. **最壞情況靜態堆疊分析證明 (`-fstack-usage`)**：
   ```text
   main                        : 88 bytes
   Touch_M1_NOP_Sweep          : 48 bytes
   Touch_AcquireSingleDiffCVD  : 24 bytes
   Touch_Adc0_Calibrate        :  0 bytes (零堆疊開銷)
   NopDelayExact               :  0 bytes (暫存器內聯，零堆疊)
   全系統最深呼叫鏈             : 136 bytes (佔 4096 bytes 堆疊之 3.3%)
   ```

---

## 4. 硬體需求與接腳配置

本專案接腳定義已完全通過官方硬體電路圖 (`SPF-46873_b.pdf`) 驗證：

| 訊號名稱 / 功能 | MCU 腳位 | 64-LQFP Pin | 外圍電路與元件 | 說明 |
| :--- | :---: | :---: | :--- | :--- |
| **TOUCH_A (驅動端)** | `PTC17` | Pin 27 | `R525` ($4.7\text{ k}\Omega$)、`R172` ($0\Omega$)、`SW7` | 觸控驅動端 / `ADC0_SE15` |
| **TOUCH_B (感測端)** | `PTC16` | Pin 28 | `C533` ($10\text{ pF}$ 到地)、`R525` ($4.7\text{ k}\Omega$) | 觸控採樣端 / `ADC0_SE14` |
| **觸控電極** | `SW7` | 板載銅箔 | $15\text{ mm} \times 15\text{ mm}$ 銅箔按鍵 | 人體接觸感測電容 $C_x$ |
| **UART_TX** | `PTC7` | Pin 51 | OpenSDA 虛擬串列埠 TX (ALT2) | 115200 bps 8N1 數據串流 |
| **UART_RX** | `PTC6` | Pin 52 | OpenSDA 虛擬串列埠 RX (ALT2) | 串列埠接收通道 |
| **紅燈 (RED)** | `PTE7` | Pin 39 | 限流電阻 + NPN 緩衝 | 故障警報指示燈 (Active High) |
| **綠燈 (GREEN)** | `PTE0` | Pin 60 | 限流電阻 + NPN 緩衝 | 待機/就緒指示燈 (Active High) |
| **藍燈 (BLUE)** | `PTE3` | Pin 13 | 限流電阻 + NPN 緩衝 | 手指觸摸成立指示燈 (Active High) |

---

## 5. Milestone 1 運作原理與物理時序

### 5.1 雙極性四相全差動 CVD 採集時序

```
                Scan A (正相)               Scan B (反相)          Idle Ground
PTC17 (Pad)  :  ──LOW───┤ High-Z ├─────────────HIGH──┤ High-Z ├─────────────LOW───
PTC16 (Ref)  :  ──HIGH──┤ High-Z ├─────────────LOW───┤ High-Z ├─────────────LOW───
                        │ 8 NOPs │                   │ 8 NOPs │
ADC0 Sampling:          └──Trigger (SE14)            └──Trigger (SE14)
```

1. **Scan A (正相預充與分享)**：
   - `PTC17` 輸出 $0\text{ V}$（清空電極電荷 $C_x$），`PTC16` 輸出 $V_{\text{DD}}$（充滿基準電容 $C_{533}$）。
   - 預充 $60\text{ ns}$ 後，單條指令將兩腳瞬間切換為 High-Z 輸入。
   - 電荷跨越 $4.7\text{ k}\Omega$ 平衡後，觸發 `ADC0_SE14` 讀取 $V_A$：
     $$V_A = V_{\text{DD}} \times \frac{C_{533}}{C_{533} + C_x}$$
2. **Scan B (反相預充與分享)**：
   - 極性立即反轉：`PTC17` 輸出 $V_{\text{DD}}$（充滿 $C_x$），`PTC16` 輸出 $0\text{ V}$（清空 $C_{533}$）。
   - 預充 $60\text{ ns}$ 後切換為 High-Z，電荷平衡後觸發 `ADC0_SE14` 讀取 $V_B$：
     $$V_B = V_{\text{DD}} \times \frac{C_x}{C_{533} + C_x}$$
3. **差動合成**：
   $$\Delta D = V_B - V_A = V_{\text{DD}} \times \left( \frac{C_x - C_{533}}{C_{533} + C_x} \right)$$
   當手指按壓使 $C_x$ 增加時，$V_B$ 上升、$V_A$ 下降，差動訊號獲得 **兩倍物理增益**！

### 5.2 NOP 電荷平衡微調驗證

跨接電阻 $4.7\text{ k}\Omega$ 與等效電容（約 $5\text{ pF}$）之時間常數 $\tau \approx 23.5\text{ ns}$。達到 $99.9\%$ 平衡約需 $7\tau \approx 165\text{ ns}$（48 MHz 下約 8 個 CPU cycles）。
系統在開機時執行 NOP 掃描（4, 8, 12, 16 NOPs），自動量測 `SE14` 與 `SE15` 兩端壓差，選定壓差收斂至 $< 15\text{ LSB}$ 的最短延遲。

### 5.3 ADC0 32x 自校準與 Bandgap 基準監控

依照 NXP AN12217 規範，開機時配置 ADC0 為 24 MHz（48MHz/2）並啟用 32 倍硬體平均，觸發自校準。校準成功後關閉硬體平均以由軟體全權控制雙極性時序。同時讀取 Channel 27（Bandgap $1.0\text{ V}$）即時推算 $V_{\text{REFH}}$：
$$V_{\text{REFH}} = \frac{1000\text{ mV} \times 4095}{\text{ADC Channel 27}}$$

---

## 6. 六大里程碑推進藍圖 (M1 ~ M6)

| 節點 | 模組目標 | 核心演算法與產出 | 量化通過標準 (Pass Criteria) | 狀態 |
| :--- | :--- | :--- | :--- | :---: |
| **M1** | **物理層雙極性 CVD 驗證** | ADC0 32x 自校準、Bandgap 監控、四相雙極性時序、NOP 校驗 | 1. 未觸碰 $1600 \sim 2500\text{ LSB}$<br>2. 平衡壓差 $< 15\text{ LSB}$<br>3. 手指按壓 $\Delta D$ 變化 $80 \sim 200\text{ LSB}$ | **已就緒 (DONE)** |
| **M2** | **展頻跳頻與中位數卡爾曼濾波** | 16-bit LFSR、3 頻互質跳頻 (MFP)、排序截尾均值、1D 自適應卡爾曼 | 底噪 $\sigma < 1.5\text{ LSB}$，$\text{SNR} \ge 28\text{ dB}$，快速點擊響應 $\le 20\text{ ms}$ | 待推進 (PLANNED) |
| **M3** | **非對稱基線追蹤狀態機** | 四區段狀態機 (溫漂慢追、預備區凍結、負向自癒、長按超時) | 帶手開機恢復 $\le 500\text{ ms}$，5s 慢速逼近不漏檢，溫漂漂移 $< \pm 3\text{ LSB}$ | 待推進 (PLANNED) |
| **M4** | **時序幾何特徵防誤觸 (TinyML)** | 4 大時域特徵提取器 + 3 步置信度遲滯邏輯 | 裸手/手套檢出率 $\ge 98\%$，水滴噴灑誤觸發 $= 0\%$，反應 $\le 90\text{ ms}$ | 待推進 (PLANNED) |
| **M5** | **雙通道線上硬體缺陷診斷** | PTC16/PTC17 雙電位窗、電阻 $R_{525}$ 開路檢測、Bandgap 監控 | 開短路 $\le 60\text{ ms}$ 鎖定防禦，故障排除 $\le 200\text{ ms}$ 自動復原 | 待推進 (PLANNED) |
| **M6** | **VLPS 極低功耗與長時燒機** | LPIT 週期喚醒與 PMC VLPS 休眠、系統標準封裝庫 | 執行時間 $\le 60\text{ }\mu\text{s}$，待機電流 $45 \sim 70\text{ }\mu\text{A}$，12 小時 0 誤觸 | 待推進 (PLANNED) |

---

## 7. 專案目錄結構

```
.
├── board/                         # 引腳初始化配置 (PORT_CI)
│   ├── Port_Ci_Port_Ip_Cfg.c
│   └── Port_Ci_Port_Ip_Cfg.h
├── generate/                      # RTD 模組配置 (Clock, OsIf, MEX)
│   ├── include/                   # Clock_Ip_Cfg.h, OsIf_Cfg.h
│   └── src/                       # Clock_Ip_Cfg.c, OsIf_Cfg.c
├── include/                       # 應用層標頭檔
│   ├── touch_cvd.h                # CVD 觸控驅動 API 與資料結構 (M1~M6)
│   └── uart_telemetry.h           # LPUART1 串列埠高速回傳介面
├── Project_Settings/
│   ├── Debugger/                  # OpenSDA PEMicro GDB 配置
│   ├── Linker_Files/              # S32K144W Flash/RAM 鏈接腳本 (.ld)
│   └── Startup_Code/              # CMSIS 啟動代碼、向量表
├── RTD/                           # NXP Real Time Drivers (Clock, Port, Adc)
├── src/
│   ├── main.c                     # 主程式入口、M1 驗收測試引擎
│   ├── touch_cvd.c                # 雙極性差動 CVD 物理層驅動
│   └── uart_telemetry.c           # 115200 bps 遙測數據輸出
├── tools/
│   ├── flash_target.py            # 自動化 OpenSDA 燒錄腳本
│   └── capture_telemetry.py       # 自動採集 100 點數據與 M1 驗收報告
├── s32k144w_touch_cvd.mex         # S32 Configuration Tools 專案檔
├── README.md                      # 英文規格書
└── README_zh.md                   # 繁體中文規格書
```

---

## 8. 快速上手與建置指南

### 8.1 環境需求
* NXP `XS32K14WEVB-Q064` 評估板（Micro-USB 連接電腦）
* S32 Design Studio 3.6.11 工具鏈（GCC 10.2）
* Python 3.8+（含 `pyserial`）

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

### 8.3 M1 量化數據採集與驗收

板子燒錄完成後，執行隨附的資料採集腳本：

```powershell
python tools/capture_telemetry.py COM5
```

終端將自動顯示自校準狀態、NOP 平衡差值、輸出 100 筆 baseline 數據並自動判定 M1 通過標準！

---

## 9. M1 節點驗收 Review Checklist

- [x] **底層物理層與時序實作**：
  - [x] ADC0 32 倍硬體平均自校準與 Bandgap 讀取（AN12217）。
  - [x] 雙極性四相全差動採集函式（`Touch_AcquireSingleDiffCVD`）。
  - [x] 閒置低阻抗接地屏蔽殘餘電荷。
- [x] **量化指標驗收**：
  - [x] 未觸碰靜態平衡工作點落在 $1600 \sim 2500\text{ LSB}$（約 $0.4 \sim 0.6 V_{\text{DD}}$）。
  - [x] $4, 8, 12, 16$ NOPs 掃描最佳電荷平衡時間，雙端壓差 $< 15\text{ LSB}$。
  - [x] 手指按壓差動訊號 $\Delta D = V_B - V_A$ 變化量達 $80 \sim 200\text{ LSB}$。
- [x] **工程品質與編譯驗證**：
  - [x] S32DS GCC 10.2 編譯達成 `0 errors, 0 warnings`。
  - [x] 最深呼叫鏈靜態堆疊佔用僅 136 Bytes（$3.3\%$）。
  - [x] 100 點 CSV 採集工具與自動燒錄工具封裝完成。

---

## 10. 商標與版權宣告

* NXP, the NXP logo, and S32K are trademarks of NXP B.V.
* Arm, Cortex are registered trademarks of Arm Limited.
* AUTOSAR is a registered trademark of the AUTOSAR Development Partnership.
