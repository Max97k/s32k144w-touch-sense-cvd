# S32K144W 電容觸控演算法參考實作 (CVD & 規則式包絡分類器)

[![Target: NXP S32K144W](https://img.shields.io/badge/Target-NXP%20S32K144W-005a9c.svg)](https://www.nxp.com/products/processors-and-microcontrollers/arm-microcontrollers/s32k-automotive-mcus/s32k1-microcontrollers-for-general-purpose:S32K1)
[![Core: Arm Cortex-M4F](https://img.shields.io/badge/Core-Arm%C2%AE%20Cortex%C2%AE--M4F%20%40%2048MHz-blue.svg)](https://www.arm.com/products/silicon-ip-cpu/cortex-m/cortex-m4)
[![RTD: 3.0.0 (AUTOSAR 4.7)](https://img.shields.io/badge/RTD-3.0.0%20(AUTOSAR%C2%AE%204.7)-orange.svg)](https://www.nxp.com)
[![Toolchain: GCC 10.2](https://img.shields.io/badge/Toolchain-GCC%2010.2%20(0%20errors%20%7C%200%20warns)-brightgreen.svg)](https://gcc.gnu.org)
[![Design Style: Defensive C / Zero-Heap](https://img.shields.io/badge/Design-Defensive%20C%20%2F%20Zero--Heap-blue.svg)](include/touch_diag.h)
[![Diagnostics: Plausibility & Fault Checks](https://img.shields.io/badge/Diagnostics-Plausibility%20%26%20Fault%20Checks-brightgreen.svg)](include/touch_diag.h)
[![EMC: IEC 61000-4-6 / CISPR 25](https://img.shields.io/badge/EMC-Spread--Spectrum%20MFP-purple.svg)](https://www.iec.ch)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD--3--Clause-blue.svg)](LICENSE)

[English](README.md) | 繁體中文

---

本專案為專為 **恩智浦半導體 (NXP Semiconductors®) S32K144W 評估板 (`XS32K14WEVB-Q064`)** 所設計的嵌入式電容觸控（Capacitive Voltage Divider, CVD）演算法參考實作韌體。

韌體採用板載 `SW7` 銅箔電極上的雙腳位差動電荷轉移架構（`PTC17` 驅動、跨接電阻 `R525` $10\text{ k}\Omega$/$4.7\text{ k}\Omega$、`PTC16/ADC0_SE14` 感測、基準電容 `C533` $10\text{ pF}$）。系統基於 **NXP Real Time Drivers (RTD 3.0.0)** 與防禦性嵌入式 C 語言實踐（零動態記憶體配置、全迴圈逾時保護、以及持續線上硬體合理性診斷）構建。

---

## 核心特性 (Key Features)

* **雙極性四相位差動 CVD (Bipolar Four-Phase Differential CVD)**：交替進行正向與反向預充電及電荷轉移，提供相較於單端感測約 $2\times$ 的差動物理信號增益，並自然抵消電源共模雜訊。
* **多頻擴頻叢集採樣 (Multi-Frequency Spread-Spectrum Burst Acquisition, MFP)**：採用 16 位元 Galois LFSR (`0xB400`) 與互質延遲序列 $\{15, 23, 31\}$ 週期進行採樣抖動，破除週期性電磁干擾 (EMI) 的諧波同步鎖定。
* **串聯式非線性濾波引擎 (Cascaded Non-Linear Filtering Engine)**：9 點叢集採樣經由 Alpha-trimmed mean（剔除最高與最低各 2 個極端離群值）後，串聯一維自適應純量卡爾曼濾波器 ($Q=0.35, R=1.0, \alpha=0.75$)，並配備大斜率快速旁路通道 (Fast Path)。
* **四區段非對稱動態基線追蹤器 (4-Zone Asymmetric Dynamic Baseline Tracker)**：具備斜率限制的環境漂移追蹤 ($+1.5\text{ LSB/s}$ / $-2.0\text{ LSB/s}$)、接近凍結區 ($0.4\times\text{TH}$)、15 秒長按超時重置保護、以及帶手開機 (Touch-on-Boot) 快速負向異常自癒 ($\le 285\text{ ms}$)。
* **規則式時域包絡特徵分類器 (Rule-Based Temporal Envelope Feature Classification)**：輕量級啟發式決策樹結合軟邊界評分，提取 4 項時域特徵（起拍斜率、動態訊雜比、正規化曲率變異數、維持比率），配合 3 階信賴度滑動視窗遲滯 (0.64 觸發 / 0.38 釋放) 與接觸維持保護 (Dropout Hold)。
* **硬體合理性線上缺陷診斷與安全互鎖 (Hardware Plausibility Diagnostics & Fail-Safe Interlock)**：即時監控接地短路 ($< 300\text{ LSB}$)、電源短路 ($> 3800\text{ LSB}$)、$R_{525}$ 跨接電阻完整性 ($< 250\text{ LSB}$ 平衡差值)、以及透過內部帶隙通道 27 監控 ADC 參考電壓 $V_{\text{REFH}}$ 漂移 ($\pm 5\%$)；偵測到異常時自動切斷觸控輸出以防誤動作。
* **硬體鎖定 15.0 ms 訊框同步器 (Hardware-Locked Frame Synchronizer)**：由 ARM Cortex-M SysTick ($48\text{ MHz}$) 驅動無抖動的 66.7 Hz 訊框週期，核心演算法單次實測耗時僅約 $195\sim 215\text{ }\mu\text{s}$（約 1.4% CPU 負載）。
* **防禦性嵌入式 C 架構 (Defensive Embedded C Architecture)**：保證 0 Bytes 堆積記憶體（僅使用靜態配置）、硬體輪詢迴圈皆有逾時保護、靜態堆疊預算 $< 250\text{ Bytes}$。

---

## 硬體接腳配置 (XS32K14WEVB-Q064)

| 信號 / 功能角色 | MCU 連接埠 | 64-LQFP 腳位 | 外部線路與元件 | 功能描述 |
| :--- | :---: | :---: | :--- | :--- |
| **TOUCH_A (驅動)** | `PTC17` | Pin 27 | $R_{525}$ ($10\text{ k}\Omega$/$4.7\text{ k}\Omega$), $R_{172}$ ($0\Omega$), `SW7` | CVD 電荷轉移驅動 / `ADC0_SE15` |
| **TOUCH_B (感測)** | `PTC16` | Pin 28 | $C_{533}$ ($10\text{ pF}$ 接地), $R_{525}$ | CVD 電荷轉移感測 / `ADC0_SE14` |
| **觸控電極 Pad** | `SW7` | 板載 PCB | $15\text{ mm} \times 15\text{ mm}$ 銅箔電極 | 感應自電容 $C_x$ |
| **UART_TX** | `PTC9` | Pin 35 | OpenSDA Virtual COM TX (ALT2) | 115200 bps 8N1 遙測串流輸出 |
| **UART_RX** | `PTC8` | Pin 36 | OpenSDA Virtual COM RX (ALT2) | 序列接收通道 |
| **紅色 LED** | `PTE7` | Pin 39 | GPIO 輸出 | 待機指示燈 (待機/放開時亮，觸控按下時滅；故障警報) |
| **綠色 LED** | `PTE0` | Pin 60 | GPIO 輸出 | 觸控確認指示燈 (有效按下時點亮) |
| **藍色 LED** | `PTE3` | Pin 13 | GPIO 輸出 | 1.0s 心跳指示燈 (0.1s 亮 / 0.9s 暗，與網頁儀表板同步) |

> **接腳說明**：XS32K14WEVB-Q064 評估板之 OpenSDA 虛擬序列埠實體連接至 `PTC8` (RX) 與 `PTC9` (TX)。`PTC6` 與 `PTC7` 則引出至擴充排針 J5，韌體未複用 PTC6/7 以免造成晶片內部 RX 輸入訊號競爭。

---

## 系統架構與韌體處理流程 (Firmware Pipeline)

```
                  ┌───────────────────────────────────────────────┐
                  │   SW7 觸控電極 (PTC17 / PTC16 CVD 架構)       │
                  └───────────────────────┬───────────────────────┘
                                          │ 18 次 SAR 類比轉換 (4 相位差動)
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ 階段 1: 多頻擴頻叢集採集 (MFP + LFSR 跳頻)    │
                  └───────────────────────┬───────────────────────┘
                                          │ 原始叢集樣本
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ 階段 2: Alpha-Trimmed Mean 排序與突波過濾     │
                  └───────────────────────┬───────────────────────┘
                                          │ 無離群值量測量 z_meas, 擴散度 spread
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ 階段 3: 一維自適應純量卡爾曼濾波 (Kalman)     │
                  └───────────────────────┬───────────────────────┘
                                          │ 濾波後狀態估計值 x_est
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ 階段 4: 四區段非對稱動態基線追蹤器            │
                  └───────────────────────┬───────────────────────┘
                                          │ 動態信號位移量 Δdyn = x_est - baseline
                                          ▼
                  ┌───────────────────────────────────────────────┐
                  │ 階段 5: 規則式時域包絡特徵分類器              │
                  │ (起拍斜率、動態 SNR、曲率變異、維持比率)      │
                  └───────────────────────┬───────────────────────┘
                                          │ 候選觸控決策
                                          ▼
┌───────────────────────────────┐         │
│ 階段 6: 硬體開短路缺陷診斷    │         │
│ 與內部帶隙基準電壓監控        ├─────────┴───────────────────────┐
└───────────────┬───────────────┘                                 │
                │ 啟用中故障代碼                                  ▼
                ▼                                   ┌───────────────────────────┐
  ┌───────────────────────────┐                     │ 最終合格觸控確認輸出      │
  │ 安全互鎖防護機制          ├────────────────────►│ (LED、序列遙測、示波器)   │
  │ (任一硬體故障時強制禁能)  │                     └───────────────────────────┘
  └───────────────────────────┘
```

---

## 即時遙測通訊協議 (Real-Time Telemetry Protocol)

評估板透過 OpenSDA 虛擬序列埠 (`115200` bps, `8N1`) 以 66.7 Hz (每 15.0 ms 一訊框) 持續輸出即時 ASCII CSV 遙測資料。

### CSV 串流格式：
```
D,<Cycle>,<A>,<B>,<DeltaD>,<Xest>,<Base>,<DynDelta>,<Class>,<Conf>,<Touch>,<ExecUs>,<Heartbeat>,<Status>
```

| 欄位名稱 | 型別 | 功能與數值說明 |
| :--- | :---: | :--- |
| `Cycle` | uint32 | 系統運作訊框計數器（每 15.0 ms 累加 1） |
| `A` | uint16 | PTC17 電極原始 ADC 取樣值 (`ADC0_SE15`) |
| `B` | uint16 | PTC16 基準端原始 ADC 取樣值 (`ADC0_SE14`) |
| `DeltaD` | int16 | 雙極性差動 CVD 原始合成值：$V_B - V_A$ |
| `Xest` | float | 一維自適應純量卡爾曼濾波器輸出狀態 |
| `Base` | float | 四區段非對稱動態基線值 |
| `DynDelta` | float | 基線補償後之淨位移量：$x_{\text{est}} - \text{baseline}$ |
| `Class` | uint32 | 分類代碼：`0` 待機, `1` 裸指觸碰, `2` 手套/壓克力, `3` 水滴干擾, `4` 邊緣滑過 |
| `Conf` | float | 3 階平滑信賴度數值 ($0.00 \sim 1.00$) |
| `Touch` | uint32 | 最終確認觸控狀態（`1` 觸碰中, `0` 釋放待機） |
| `ExecUs` | uint32 | 核心演算法運算時間（SysTick 實測微秒數，約 $\sim 210\text{ }\mu\text{s}$） |
| `Heartbeat`| uint32 | 心跳狀態（`1` 點亮 0.1s, `0` 熄滅 0.9s） |
| `Status` | string | ASCII 狀態字串：`TOUCH` 或 `IDLE` |

---

## 專案目錄結構 (Project Structure)

```
.
├── board/                         # 接腳初始化配置代碼 (PORT_CI)
├── data/                          # 遙測與驗證資料集
│   ├── hardware/                  # 真實板載 UART 遙測錄製資料 (.csv)
│   └── simulated/                 # 契約驗證合成資料集 (.csv)
├── freemaster/                    # FreeMASTER Lite 1.5.1 即時示波器網頁儀表板
├── generate/                      # RTD 模組配置代碼 (Clock, OsIf, MEX)
├── include/                       # 公用標頭檔
│   ├── touch_cvd.h                # CVD 物理層採集與 ADC 自我校正
│   ├── touch_filter.h             # LFSR 擴頻、叢集採樣、Alpha-trim、自適應卡爾曼
│   ├── touch_baseline.h           # 4 區段非對稱動態基線追蹤狀態機
│   ├── touch_tinyml.h             # 規則式時域包絡特徵分類器
│   ├── touch_diag.h               # 硬體缺陷合理性診斷與安全互鎖
│   ├── touch_sense.h              # 統一觸控感測引擎介面
│   └── uart_telemetry.h           # 高速 ASCII 串流遙測驅動
├── Project_Settings/              # 鏈結腳本、啟動代碼、除錯設定檔
├── RTD/                           # NXP Real Time Drivers 3.0.0
├── src/                           # 程式原始碼
│   ├── main.c                     # 系統核心排程、SysTick 同步與主迴圈
│   ├── touch_cvd.c                # CVD 物理驅動與 32x ADC 自我校正
│   ├── touch_filter.c             # 多頻展頻採樣與自適應卡爾曼濾波
│   ├── touch_baseline.c           # 動態基線狀態機與開機異常自癒
│   ├── touch_tinyml.c             # 包絡特徵提取與軟邊界評分引擎
│   ├── touch_diag.c               # 硬體缺陷診斷與自動復原
│   ├── touch_sense.c              # 統一觸控感測驅動
│   └── uart_telemetry.c           # 115200 bps 序列遙測傳輸
├── tests/                         # 自動化 Python 契約驗證測試套件
│   ├── test_m1_contract.py        # M1: 靜態基線分布與物理層驗證
│   ├── test_m2_contract.py        # M2: 濾波鏈、雜訊抑制、脈衝抗擾
│   ├── test_m3_contract.py        # M3: 帶手開機、慢速貼近、熱漂移追蹤
│   ├── test_m4_contract.py        # M4: 混淆矩陣、水霧抑制、手套識別
│   └── test_m5_contract.py        # M5: 故障注入、安全互鎖、診斷復原
├── tools/                         # 自動化燒錄、即時遙測錄製與分析工具
│   ├── analyze_press.py           # 硬體按壓測試報告分析腳本
│   ├── capture_telemetry.py       # 100 筆原始採樣擷取工具
│   ├── flash_target.py            # OpenSDA PEMicro 自動化燒錄工具
│   └── record_live.py             # 即時 UART 遙測串流錄製腳本
├── run_freemaster.bat             # 一鍵啟動 FreeMASTER Lite 儀表板批次檔
├── s32k144w_touch_cvd.mex         # S32 Configuration Tools 專案檔
├── README.md                      # 英文技術規格書與說明文件
└── README_zh.md                   # 繁體中文技術規格書與說明文件
```

---

## 建置與燒錄指南 (Build & Flashing)

### 環境需求
* **S32 Design Studio for S32 Platform** (Version 3.6.11)，含 GCC 10.2 Bare-Metal 工具鏈與 NXP S32K1xx RTD 3.0.0。
* Python 3.8+（含 `pyserial` 套件，供燒錄與遙測錄製使用）。

### 透過 Make 編譯韌體：
```powershell
# 1. 設定 S32DS 工具鏈路徑環境變數
$env:PATH = "C:\NXP\S32DS.3.6.11\S32DS\build_tools\msys32\usr\bin;C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin;" + $env:PATH

# 2. 透過 GNU Make 進行編譯 (0 錯誤、0 警告)
cd Debug_FLASH
make -j8 all
```

### 燒錄至評估板：
```powershell
# 透過 OpenSDA PEMicro 進行自動燒錄
python tools/flash_target.py
```

### 執行自動化契約測試套件：
```powershell
python -B tests/test_m1_contract.py
python -B tests/test_m2_contract.py
python -B tests/test_m3_contract.py
python -B tests/test_m4_contract.py
python -B tests/test_m5_contract.py
```

---

## FreeMASTER Lite 1.5.1 即時示波器網頁儀表板

專案內建支援 **NXP FreeMASTER Lite 1.5.1**，提供 HTML5 即時動態示波器儀表板，可直接在瀏覽器中視覺化觀察 CVD 原始差動值、卡爾曼濾波波形、動態基線追蹤狀況與信賴度指標。

```
 [ S32K144W 硬體 EVB ]
            │ (PTC9 UART TX @ 115200 bps, 66.7 Hz)
            ▼
 [ telemetry_bridge.py ] ───── (WebSocket ws://localhost:8090) ─────► [ FreeMASTER Lite 1.5.1 ]
                                                                             │         │
                                     ┌───────────────────────────────────────┘         │
                                     ▼                                                 ▼
                        [ touch_scope.html 儀表板 ]                        [ NXP FreeMASTER MCP ]
                        - 4 通道即時滾動示波器                             - ws.connect()
                        - 霓虹觸發狀態指示燈                               - rpc.StartComm()
                        - 包絡信賴度儀表                                   - rpc.ReadVariable()
                        - 執行時間監控標籤 (~210 µs)                       - rpc.WriteVariable()
```

### 啟動方式：
雙擊執行專案根目錄的批次檔：
```cmd
run_freemaster.bat
```
瀏覽器將自動開啟儀表板頁面：
```
http://localhost:8090/touch_scope.html
```

---

## 參考文獻與技術規範 (References & Technical Standards)

本參考實作之演算法公式與物理層設計參考以下權威學術文獻與半導體原廠技術手冊：

1. **[1]** S. M. Hong, J.-W. Park, J.-H. Jeong, and S. Y. Kim, "Real-Time Defect Detection of Capacitive Touch Pads for Hands-Off Detection in Advanced Driver Assistance Systems," *Applied Sciences*, vol. 16, no. 4, art. no. 1675, Feb. 2026.
2. **[2]** X. Feng et al., "A Kalman Filter-Assisted Data-Predictive SAR ADC With Reduced Switching Energy for Low-Power Applications," *arXiv preprint arXiv:2607.16139*, Jul. 2026.
3. **[3]** X. Ouyang, H. Kuang, D. Kong, Z. Cheng, and H. Yuan, "A Phase-Adjustable Noise-Shaping SAR ADC for Mitigating Parasitic Capacitance Effects from PIP Capacitors," *Sensors*, vol. 25, no. 19, art. no. 6029, Oct. 2025.
4. **[4]** Y.-H. Hwang, "Study of Circuit Techniques for Enhancing Display Noise Immunity in Analog Front-end of Mutual-capacitive Touch Systems," *JSTS*, vol. 24, no. 5, pp. 473–482, Oct. 2024.
5. **[5]** S. Seong, S. Lee, S. Bae, and M. Lee, "Self Capacitance Mismatch Calibration Technique for Fully-Differential Touch Screen Panel Self Capacitance Sensing System," *Sensors*, vol. 23, no. 7, art. no. 3779, Apr. 2023.
6. **[6]** H. Pyeon, H. Kim, R. C. Kim, G. Oh, and S. Lim, "Deep Learning-Based Driver's Hands on/off Prediction System Using In-Vehicle Data," *Sensors*, vol. 23, no. 3, art. no. 1442, Jan. 2023.
7. **[7]** H. Nam, K.-H. Seol, J. Lee, H. Cho, and S. W. Jung, "Review of Capacitive Touchscreen Technologies: Overview, Research Trends, and Machine Learning Approaches," *Sensors*, vol. 21, no. 14, art. no. 4776, Jul. 2021.
8. **[8]** Y.-H. Yu and T.-Y. Sun, "A Pseudo-Differential Measuring Approach for Implementing Microcontroller-Based Capacitive Touch Sensing in Low-Power Quality Situation," *IEEE Sensors Journal*, vol. 16, no. 2, pp. 390–399, Jan. 2016.
9. **[9]** S. Sun, "GPIS Touch Sense Solution," NXP Semiconductors, Automotive CAS Technical Presentation, Doc. No. APF-AUT-T3280, Sep. 2018.
10. **[10]** B. Davison, "mTouch™ Sensing Solution Acquisition Methods Capacitive Voltage Divider," Microchip Technology Inc., Application Note AN1478, 2012.
11. **[11]** Microchip Technology Inc., "Touch Sensing Using Analog-to-Digital Converter With Computation (ADCC)," Microchip Developer Help, 2026.
12. **[12]** Texas Instruments Inc., "Enabling Noise Tolerant Capacitive Touch HMIs With MSP CapTIvate™ Microcontrollers," Application Report SLAY045B, 2017.
13. **[13]** NXP Semiconductors, "S32K1xx ADC Guidelines, Spec, and Configuration," Application Note AN12217, Rev. 0, Jun. 2018.
14. **[14]** NXP Semiconductors, "S32K144EVB-Q100 Evaluation Board Schematic," Doc. No. SCH-29248 / SPF-29248, Rev. C, Mar. 2018; and "S32K1xx Data Sheet," Rev. 14, 2021.

---

## 授權條款 (License)

BSD-3-Clause License. Copyright (c) 2026, Max97k. All rights reserved.
