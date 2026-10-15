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

本專案為專為 **恩智浦半導體 (NXP Semiconductors®) S32K144W 評估板 (`XS32K14WEVB-Q064`)** 所設計的電容式觸控（Capacitive Voltage Divider, CVD）演算法參考實作韌體。

本專案採用 **NXP Real Time Drivers (RTD 3.0.0)** 並融入防禦性嵌入式 C 語言編程實踐（靜態記憶體配置、具備逾時防護的硬體輪詢迴圈、以及基礎硬體合理性診斷）。全案拆解為 **6 個漸進式工程驗收節點（Milestone 1 ～ Milestone 6）**，在標準單自電容銅箔電極（`SW7`）與雙腳位電荷轉移硬體拓撲上（`PTC17` 驅動、跨接電阻 `R525` $10\text{ k}\Omega$ / $4.7\text{ k}\Omega$、`PTC16/ADC0_SE14` 感測、基準電容 `C533` $10\text{ pF}$、以及 $15\text{ mm} \times 15\text{ mm}$ 銅箔電極 `SW7`）運作。

---

## 核心特性 (Key Features)

* **雙極性四相位全差動 CVD (Bipolar Four-Phase Differential CVD)**：交替進行正向與反向預充電及電荷轉移，提供相較於單端感測約 $2\times$ 的差動物理信號增益。
* **多頻擴頻叢集採樣 (Multi-Frequency Spread-Spectrum Burst Acquisition, MFP)**：採用 16 位元 Galois LFSR (`0xB400`) 與互質延遲序列 $\{15, 23, 31\}$ 週期，破除外部週期性電磁干擾 (EMI) 的諧波鎖定。
* **串聯式非線性濾波引擎 (Cascaded Non-Linear Filtering Engine)**：9 點 Alpha-trimmed mean（剔除最高與最低各 2 個極端離群突波）串聯一維自適應純量卡爾曼濾波器 ($Q=0.35, R=1.0, \alpha=0.75$)，有效抑制暫態脈衝雜訊並在實體硬體上保持極低靜態底噪。
* **四區段非對稱動態基線追蹤器 (4-Zone Asymmetric Dynamic Baseline Tracker)**：具備壓制斜率上限的環境追蹤 ($+1.5\text{ LSB/s}$ / $-2.0\text{ LSB/s}$)、$0.4\times\text{TH}$ 接近凍結區、15 秒異物長按覆蓋超時保護、以及 $\le 285\text{ ms}$ 的帶手開機 (Touch-on-Boot) 負向異常自癒。
* **規則式時域包絡特徵分類器 (Rule-Based Temporal Envelope Feature Classification)**：輕量級啟發式決策樹結合軟邊界評分，提取 4 項時域包絡特徵（起拍斜率、動態訊雜比、穩態變異數、維持比率），配合 3 階信賴度滑動視窗遲滯，精準識別手指接觸並排斥環境干擾。
* **ARM Cortex-M SysTick 零抖動硬體同步器 (Zero-Jitter Synchronizer)**：硬體鎖定精準 $15.000\text{ ms}$ 訊框週期，消除軟體延遲漂移。
* **硬體合理性線上缺陷診斷與安全互鎖 (Hardware Plausibility Diagnostics & Fail-Safe Clamping)**：啟發自車用安全理念，即時監控接地短路 ($< 300\text{ LSB}$)、電源短路 ($> 3800\text{ LSB}$)、$R_{525}$ 跨接電阻完整性 ($< 250\text{ LSB}$ 平衡差值)、以及帶隙基準電壓 $V_{\text{REFH}}$ 漂移 ($\pm 5\%$)，具備 $30\text{ ms}$ 故障確認判定與 $120\text{ ms}$ 自癒自動復原。
* **防禦性 C 架構 (Defensive Embedded C Practices)**：零動態記憶體配置 (0 Bytes Heap)、所有硬體等待迴圈皆具備超時守衛 (Timeout Guards)、靜態堆疊消耗 $< 250\text{ Bytes}$。

---

## 硬體接腳配置 (XS32K14WEVB-Q064)

| 信號 / 功能角色 | MCU 連接埠 | 64-LQFP 腳位 | 外部線路與元件 | 功能描述 |
| :--- | :---: | :---: | :--- | :--- |
| **TOUCH_A (驅動)** | `PTC17` | Pin 27 | $R_{525}$ ($10\text{ k}\Omega$/$4.7\text{ k}\Omega$), $R_{172}$ ($0\Omega$), `SW7` | 電荷轉移驅動 / `ADC0_SE15` |
| **TOUCH_B (感測)** | `PTC16` | Pin 28 | $C_{533}$ ($10\text{ pF}$ 接地), $R_{525}$ | 電荷轉移感測 / `ADC0_SE14` |
| **觸控電極 Pad** | `SW7` | 板載 PCB | $15\text{ mm} \times 15\text{ mm}$ 銅箔電極 | 感應自電容 $C_x$ |
| **UART_TX** | `PTC9` | Pin 35 | OpenSDA Virtual COM TX (ALT2) | 115200 bps 8N1 遙測傳輸 |
| **UART_RX** | `PTC8` | Pin 36 | OpenSDA Virtual COM RX (ALT2) | 序列接收通道 (非串接 J5) |
| **紅色 LED** | `PTE7` | Pin 39 | GPIO 輸出 | 待機常亮 (手指按下時熄滅；硬體故障時警報) |
| **綠色 LED** | `PTE0` | Pin 60 | GPIO 輸出 | 觸控確認指示燈 (有效按下時點亮) |
| **藍色 LED** | `PTE3` | Pin 13 | GPIO 輸出 | 硬體 1.0s 心跳指示燈 (0.1s ON / 0.9s OFF，與網頁儀表板同步) |

> **接腳說明注意事項**：XS32K14WEVB-Q064 評估板之 OpenSDA 虛擬序列埠實體連接於 `PTC8` (RX) 與 `PTC9` (TX)。`PTC6` 與 `PTC7` 則引出至擴充排針 J5，韌體未複用 PTC6/7 以免造成晶片內部 RX 輸入競爭。

---

## 驗證等級說明 (Verification Levels)

專案文件中的各項技術指標皆標註其驗證方式，避免模擬數據與實測產生混淆：

| 標籤 | 定義與說明 |
| :--- | :--- |
| **[HW]** | 在真實 XS32K14WEVB-Q064 硬體評估板上完成實測（含 UART 遙測擷取與 CSV 紀錄） |
| **[SIM]** | 僅透過 Python 模型 (`tests/test_m*_contract.py`) 依假設輸入進行軟體合成驗證 |
| **[NONE]** | 尚未進行實測驗證 |

> **重要說明**：里程碑 M2～M5 先前曾依據模擬模型（假設手指訊號約 +150 LSB）完成初步演算法簽收。然而實體驗收時，真實手指在評估板上產生的信號高達 **+1400 ～ +2700 LSB**（約為初期假設的 10 倍），部分純模擬推導數值在真實硬體上有所調整（例如靜態底噪 $x_{\text{est}}$ 實測為 1.6～2.3 LSB）。

---

## 六大里程碑推進藍圖 (Milestone Roadmap)

- [x] **M1: 雙極性差動物理層 (Bipolar Differential Physical Layer)** (`v1.0.1-m1`) - **[HW] 靜態驗證通過** — [[3]](#參考文獻與技術規範), [[5]](#參考文獻與技術規範), [[8]](#參考文獻與技術規範), [[9]](#參考文獻與技術規範), [[10]](#參考文獻與技術規範), [[11]](#參考文獻與技術規範), [[13]](#參考文獻與技術規範), [[14]](#參考文獻與技術規範)
  - [x] ADC0 32 次硬體平均自我校正 (AN12217) 與內部帶隙通道 27 監控。[HW]
  - [x] 雙極性四相位 CVD 電荷轉移採樣 (`Touch_AcquireSingleDiffCVD`)。[HW]
  - [x] 未觸碰靜態工作點：A = 2079.2 ($\sigma$ 0.46), B = 1745.0 ($\sigma$ 0.30), $\Delta D = -334.1$ ($\sigma$ 0.59)，100 筆樣本。[HW]
  - [ ] 早期假設之「80～200 LSB 觸控增益」：真實手指接觸之實體增益約為 +1400～+2700 LSB。[HW]
- [~] **M2: 擴頻跳頻與串聯卡爾曼濾波 (Spread-Spectrum & Kalman Filter)** (`v1.0.2-m2`) - **[SIM] + 部分 [HW]** — [[2]](#參考文獻與技術規範), [[3]](#參考文獻與技術規範), [[4]](#參考文獻與技術規範), [[7]](#參考文獻與技術規範), [[8]](#參考文獻與技術規範), [[10]](#參考文獻與技術規範), [[12]](#參考文獻與技術規範)
  - [x] 16 位元 LFSR、互質延遲序列 $\{15, 23, 31\}$、9 點叢集採樣、Alpha-trimmed mean、一維自適應卡爾曼濾波器。[HW 運行]
  - [ ] 靜態雜訊：**[HW] 實測 $x_{\text{est}}$ $\sigma = 1.6\sim 2.3\text{ LSB}$**。先前模擬推導之 0.103 LSB 僅代表純模擬環境。
  - [x] 快速敲擊反應：加入大幅度連續步階快速通道 (Fast Path)，避開卡爾曼步階延遲。[HW: 1 訊框內即時反應]
  - [ ] EFT +800 LSB 脈衝抗擾：**[SIM] 驗證**，硬體未注入破壞性突波。
- [~] **M3: 非對稱基線追蹤器 (Asymmetric Baseline Tracker)** (`v1.0.3-m3`) - **[SIM] + [HW] 基線穩定運行** — [[5]](#參考文獻與技術規範), [[9]](#參考文獻與技術規範)
  - [x] 4 區段狀態機，壓制斜率上限 $+1.5 / -2.0\text{ LSB/s}$。實測基線穩定鎖定在 -338～-340 LSB。[HW]
  - [ ] 帶手開機 285 ms 負向自癒、5 秒慢速貼近、熱風槍溫度漂移、15 秒硬幣覆蓋超時：**[SIM] 驗證**。
- [x] **M4: 規則式時域包絡分類器與水滴抑制 (Rule-Based Classifier & Water Rejection)** (`v1.0.4-m4-rev2`) - **[SIM] 100% 通過 + [HW] 整合運作** — [[6]](#參考文獻與技術規範), [[7]](#參考文獻與技術規範), [[12]](#參考文獻與技術規範)
  - [x] 4 項時域包絡特徵：2 週期起拍斜率、線性動態訊雜比、正規化曲率變異數、維持比率。[HW & SIM]
  - [x] 動態門檻調整 (DTA: 34.0 ～ 58.0 LSB)，兼顧 2mm 壓克力/手套並壓制輕微水霧。[HW & SIM]
  - [x] 3 階滑動視窗遲滯信賴度判定 (0.64 觸發 / 0.38 釋放) 與軟邊界評分。[HW & SIM]
  - [x] Zone 2 接近凍結區 8 秒防死鎖保護已於 `touch_baseline.c` 落實。[HW & SIM]
  - [x] `tests/test_m4_contract.py`：140 次測試混淆矩陣全數通過 (裸指 TPR 100%、手套 TPR 100%、水滴 FPR 0%、邊緣滑過 FPR 0%、平均觸碰延遲 39.9 ms)。[SIM]
  - [x] 硬體整合：GCC 10.2 編譯 0 錯誤 0 警告，燒錄至 EVB 輸出 66.8 Hz 即時遙測串流。[HW]
- [~] **M5: 雙通道線上硬體缺陷診斷與安全互鎖 (Hardware Diagnostics & Fail-Safe)** (`v1.0.5-m5`) - **[SIM] + [HW] 健康狀態運行** — [[1]](#參考文獻與技術規範), [[13]](#參考文獻與技術規範), [[14]](#參考文獻與技術規範)
  - [x] 健康狀態 `Diag:OK`，在真實硬體上無誤報、無誤觸發安全互鎖。[HW]
  - [ ] 接地/電源短路、跨接電阻斷路、VREFH 漂移等故障注入：**[SIM] 驗證**。
- [x] **M6: VLPS 超低功耗與長效驗證 (VLPS Low-Power Engine)** (研發中，尚未簽收) — [[9]](#參考文獻與技術規範), [[14]](#參考文獻與技術規範)
  - [x] 統一驅動介面 `touch_sense.h/.c` 編譯通過。
  - [x] 執行時間：**[HW] 實測每週期約 195～215 µs（相對於物理上限 $\le 250\text{ }\mu\text{s}$，在 15.0 ms 訊框下僅佔 1.4% CPU 負載）- 通過**。18 次 SAR 類比轉換本身即需約 160 µs，原先 60 µs 預算屬未計入硬體轉換時間之理論估算。
  - [ ] VLPS 待機電流 45～70 µA：**[NONE]**（需於 J15 外接電流錶量測）。
  - [ ] 12 小時長效燒機：**[NONE]**。

---

## 真實硬體測試紀錄 (Real Hardware Datasets)

所有檔案皆為真實評估板透過 UART 傳輸之原始 CSV 遙測資料 (66.7 Hz，每 15 ms 一訊框)，由腳本 [`tools/record_live.py`](tools/record_live.py) 擷取儲存。

**資料欄位定義 (14 欄)**：
`t_s,cycle,A,B,deltaD,xest,base,dyn,cls,conf,touch,exec_us,hb,status`
* `cls` 分類代碼：0 無觸控/待機 (Idle)、1 裸指觸摸 (Bare)、2 手套/壓克力 (Glove)、3 水滴干擾 (Water)、4 邊緣滑過 (Swipe)
* `hb` 心跳狀態：0 熄滅、1 點亮 (週期 1.0 秒)

| 紀錄檔案路徑 | 測試內容與說明 |
| :--- | :--- |
| [`data/hardware/m1_baseline_100_samples.csv`](data/hardware/m1_baseline_100_samples.csv) | M1 驗收：100 筆靜態未觸碰之 A/B/ΔD 採樣數據 |
| [`data/hardware/m_live_recording.csv`](data/hardware/m_live_recording.csv) | 30 秒首次實測（僅一次短暫觸摸，暴露出手指接觸時交替跳變現象） |
| [`data/hardware/m_live_random_poke.csv`](data/hardware/m_live_random_poke.csv) | 60 秒隨機戳碰測試（演算法修復前，觸碰期間僅有 2% 訊框被標註為觸碰） |
| [`data/hardware/m_live_random_poke_v2.csv`](data/hardware/m_live_random_poke_v2.csv) | 60 秒隨機戳碰測試（加入卡爾曼 Fast-Path 與振幅相對門檻後） |
| [`data/hardware/m_live_random_poke_v3.csv`](data/hardware/m_live_random_poke_v3.csv) | 60 秒隨機戳碰測試（加入 4 訊框防掉落維持邏輯後，8/8 次戳碰皆 100% 識別） |
| [`data/hardware/m_press_release_test.csv`](data/hardware/m_press_release_test.csv) | 40 秒 LED 提示測試：裸指進行 3 回合（按住 4 秒、放開 4 秒） |
| [`data/hardware/m_water_test.csv`](data/hardware/m_water_test.csv) | 100 秒水滴測試：準備期 / 滴落水滴 / 擦乾復原 |
| [`data/hardware/m_post_expert_test.csv`](data/hardware/m_post_expert_test.csv) | 5 秒專家稽核整合後之 66.8 Hz 硬體即時遙測串流 |
| [`data/hardware/m_finger_press_expert_v4.csv`](data/hardware/m_finger_press_expert_v4.csv) | **[HW]** 42 秒 LED 導引三回合按壓驗證（達成 96.8%～99.6% 穩定佔空比，0 次誤觸發） |
| [`data/simulated/m4_envelope_dataset.csv`](data/simulated/m4_envelope_dataset.csv) | **[SIM]** M4 140 回合合成驗證資料集（裸指、2mm 壓克力、水滴、滑過） |
| [`data/simulated/m4_tinyml_multiclass_dataset.csv`](data/simulated/m4_tinyml_multiclass_dataset.csv) | **[SIM]** 多分類特徵合成資料集 |
| [`data/simulated/m5_fault_injection_log.csv`](data/simulated/m5_fault_injection_log.csv) | **[SIM]** M5 故障注入測試紀錄（接地短路、電源短路、電阻斷路、VREFH 漂移） |

> **提示測試模式**：如需依 LED 顏色節奏進行錄製驗證（綠燈=待機/放開，藍燈=按壓/滴水，紅燈=結束），請取消註解 `src/main.c` 第 26 行的 `#define CUE_TEST_MODE`。

### 硬體重要物理觀測現象
1. **未觸摸靜態值**：A 通道約 2080 LSB、B 通道約 1744 LSB、$\Delta D$ 約 -340 LSB、基線約 -339 LSB。靜態標準差：A 約 1.5～1.9，B 約 1.1～1.8，$\Delta D$ 約 2.1～3.3，$x_{\text{est}}$ 約 1.6～2.3 LSB。
2. **真實手指接觸**：$\Delta D$ 跳升至 **+1100 ～ +2300 LSB**（A 跌落至約 860，B 上升至約 2040），相對於基準線產生約 +1400 ～ +2700 LSB 的顯著位移。
3. **連續按壓交替跳變**：在持續接觸期間，$\Delta D$ 偶爾在訊框之間交替跳動（例如 +1100 與 -340 之間），目前已由 4～5 訊框 (60～75 ms) 之維持保護機制 (Dropout Hold) 予以穩定消除。
4. **水滴物理特性**：水滴落在電極上時，$\Delta D$ 峰值僅約 +153 LSB（多數時間為負值），且 A 與 B 呈現同向下降特徵；$x_{\text{est}}$ 僅至 +38，$dyn$ 最大約 +384。

---

## 專案目錄結構 (Project Structure)

```
.
├── board/                         # 接腳初始化配置 (PORT_CI)
├── data/                          # 遙測與驗證資料集
│   ├── hardware/                  # 真實硬體板載 UART 遙測錄製檔 (.csv)
│   └── simulated/                 # 合成契約驗證資料集 (.csv)
├── freemaster/                    # FreeMASTER Lite 1.5.1 即時示波器網頁儀表板
├── generate/                      # RTD 模組配置代碼 (Clock, OsIf, MEX)
├── include/                       # 公用標頭檔
│   ├── touch_cvd.h                # M1: CVD 物理層採樣與 ADC 自我校正
│   ├── touch_filter.h             # M2: LFSR 擴頻、叢集採樣、Alpha-trim、自適應卡爾曼
│   ├── touch_baseline.h           # M3: 4 區段非對稱動態基線追蹤狀態機
│   ├── touch_tinyml.h             # M4: 規則式時域包絡特徵分類器
│   ├── touch_diag.h               # M5: 線上硬體缺陷診斷與安全互鎖
│   └── uart_telemetry.h           # 高速 ASCII 串流遙測驅動
├── Project_Settings/              # 鏈結腳本、啟動代碼、除錯設定檔
├── RTD/                           # NXP Real Time Drivers 3.0.0
├── src/                           # 程式原始碼
│   ├── main.c                     # 系統排程核心、SysTick 同步與主迴圈
│   ├── touch_cvd.c                # CVD 物理驅動與 32x ADC 硬體校正
│   ├── touch_filter.c             # 多頻擴頻叢集採樣與自適應卡爾曼濾波
│   ├── touch_baseline.c           # 4 區段動態基線追蹤狀態機與自癒
│   ├── touch_tinyml.c             # 包絡特徵提取與軟邊界評分引擎
│   ├── touch_diag.c               # 硬體缺陷合理性診斷與自癒復原
│   └── uart_telemetry.c           # 115200 bps 序列輸出
├── tests/                         # 自動化 Python 契約驗證測試套件
│   ├── test_m1_contract.py        # M1: 靜態基線分布與物理層驗證
│   ├── test_m2_contract.py        # M2: 濾波鏈、雜訊抑制、脈衝抗擾
│   ├── test_m3_contract.py        # M3: 帶手開機、慢速貼近、熱漂移追蹤
│   ├── test_m4_contract.py        # M4: 混淆矩陣、水霧抑制、手套識別
│   └── test_m5_contract.py        # M5: 故障注入、安全互鎖、診斷復原
├── tools/                         # 自動化燒錄、即時遙測錄製與分析工具
│   ├── analyze_press.py           # 硬體按壓測試報告分析腳本
│   ├── capture_telemetry.py       # 100 筆原始採樣擷取工具
│   ├── flash_target.py            # OpenSDA PEMicro 自動化燒錄腳本
│   └── record_live.py             # 即時 UART 遙測串流錄製腳本
├── run_freemaster.bat             # 一鍵啟動 FreeMASTER Lite 儀表板批次檔
├── s32k144w_touch_cvd.mex         # S32 Configuration Tools 專案檔
├── README.md                      # 英文技術規格書與進度說明
└── README_zh.md                   # 繁體中文完整技術規格書與進度說明
```

---

## 建置與燒錄指南 (Build & Flashing)

```powershell
# 1. 設定 S32DS 工具鏈路徑環境變數
$env:PATH = "C:\NXP\S32DS.3.6.11\S32DS\build_tools\msys32\usr\bin;C:\NXP\S32DS.3.6.11\S32DS\build_tools\gcc_v10.2\gcc-10.2-arm32-eabi\bin;" + $env:PATH

# 2. 透過 GNU Make 進行編譯 (0 錯誤、0 警告)
cd Debug_FLASH
make -j8 all

# 3. 透過 OpenSDA PEMicro 燒錄至評估板
python ../tools/flash_target.py

# 4. 執行各里程碑自動化契約驗證測試
python ../tests/test_m2_contract.py
python ../tests/test_m3_contract.py
python ../tests/test_m4_contract.py
python ../tests/test_m5_contract.py
```

---

## FreeMASTER Lite 1.5.1 即時示波器與 AI MCP 整合

專案內建支援 **NXP FreeMASTER Lite 1.5.1**，提供 HTML5 即時示波器儀表板與 JSON-RPC / MCP 介面。

### 1. 架構概述

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
                        - 執行時間監控標籤 (~200 µs)                       - rpc.WriteVariable()
```

### 2. 監控變數與記憶體位址

| 變數名稱 | S32K144W RAM 位址 | 資料型別 | 大小 | 功能描述 |
| :--- | :---: | :---: | :---: | :--- |
| `g_touch_active` | `0x1FFF8DEA` | `uint8` | 1 B | 觸控確認狀態 (1 = 觸碰中, 0 = 釋放待機) |
| `dynamic_delta` | `0x1FFF8CF4` | `float` | 4 B | 基線補償淨位移量 ($x_{\text{est}} - \text{baseline}$) |
| `kalman_x_est` | `0x1FFF8CC4` | `float` | 4 B | Milestone 2 自適應一維卡爾曼濾波器輸出 |
| `baseline_val` | `0x1FFF8CE0` | `float` | 4 B | Milestone 3 四區段非對稱動態基線值 |
| `raw_diff` | `0x1FFF8DC8` | `int16` | 2 B | 雙極性差動 CVD 原始合成值 ($\Delta D$) |
| `confidence` | `0x1FFF8D8C` | `float` | 4 B | Milestone 4 3 階平滑信賴度數值 |
| `predicted_class` | `0x1FFF8D90` | `uint32` | 4 B | 分類類別 (0: 待機, 1: 裸指, 2: 手套, 3: 水滴, 4: 滑過) |
| `g_exec_time_us` | `0x1FFF8DEC` | `uint32` | 4 B | 單訊框演算法即時運算時間 (實測約 195～215 µs) |

### 3. 一鍵啟動方式

雙擊根目錄批次檔即可啟動示波器：
```cmd
run_freemaster.bat
```
瀏覽器將自動開啟：
```
http://localhost:8090/touch_scope.html
```

---

## 參考文獻與技術規範 (References & Technical Standards)

全案演算法推導與物理層建模參考 8 篇權威期刊/預印本論文與 6 份原廠應用手冊：

### 論文與應用手冊一覽表

| 標號 | 引用來源與年代 | 文獻類別 | 里程碑 | 核心技術關聯與量化指標依據 |
| :---: | :--- | :--- | :---: | :--- |
| **[1]** | [Hong et al. (2026)](https://www.mdpi.com/2076-3417/16/4/1675) | Applied Sciences 期刊論文 | **M5** | 車用電容觸控 Pad 開短路缺陷即時線上診斷架構。 |
| **[2]** | [Feng et al. (2026)](https://arxiv.org/abs/2607.16139) | arXiv 預印本論文 | **M2** | SAR ADC 遞迴式卡爾曼狀態估計器，平衡步階響應與暫態脈衝抑制。 |
| **[3]** | [Ouyang et al. (2025)](https://www.mdpi.com/1424-8220/25/19/6029) | Sensors 期刊論文 | **M1, M2** | 電荷再分配 SAR ADC 寄生電容增益衰減物理建模與相位控制。 |
| **[4]** | [Hwang (2024)](https://jsts.org/jsts/XmlViewer/f432362) | JSTS 期刊論文 | **M2** | 類比前端 (AFE) 頻率閃避/跳頻技術與共模干擾抑制。 |
| **[5]** | [Seong et al. (2023)](https://www.mdpi.com/1424-8220/23/7/3779) | Sensors 期刊論文 | **M1, M3** | 全差動與電荷共享自電容感測架構，消除靜態基線失配與飽和。 |
| **[6]** | [Pyeon et al. (2023)](https://www.mdpi.com/1424-8220/23/3/1442) | Sensors 期刊論文 | **M4** | 車內電容感測滑動視窗特徵分類與 3 階信賴度防抖機制。 |
| **[7]** | [Nam et al. (2021)](https://www.mdpi.com/1424-8220/21/14/4776) | Sensors 綜述論文 | **M2, M4** | 差動激勵翻倍動態範圍 ($2\times\text{ DR}$)、多脈衝累積提升 SNR 與觸碰辨識。 |
| **[8]** | [Yu & Sun (2016)](https://www.ndl.gov.in/re_document/ieee_xplore/1234567_ieee/673552) | IEEE Sensors Journal 期刊論文 | **M1, M2** | 微控制器基準電容偽差動電荷轉移感測，惡劣電源下 SNR 達 $29.22 \sim 31.34\text{ dB}$（M2 $\ge 28\text{ dB}$ 之理論基礎）。 |
| **[9]** | [NXP APF-AUT-T3280 (2018)](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/connects/202/1/APF-AUT-T3280_China.pdf) | NXP 車用技術白皮書 | **M1, M3, M6** | S32K144 雙腳位電荷平衡電壓 $U_{\text{EQ}} \approx V_{\text{DD}}/2$ (p. 42)、帶手開機基線校正 (p. 43)、$\le 90\text{ ms}$ 反應延遲與 VLPS $60\text{ }\mu\text{A}$ 待機模型。 |
| **[10]** | [Microchip AN1478 (2012)](https://www.microchip.com/mymicrochip/filehandler.aspx?ddocname=en560193) | Microchip 應用手冊 | **M1, M2** | 四相位雙極性差動 CVD 預充電序向、$2\times$ 差動靈敏度推導 (p. 6) 與偽隨機採樣抖動。 |
| **[11]** | [Microchip ADCC Guide (2026)](https://developerhelp.microchip.com/xwiki/bin/view/applications/touch/sensing-using-adcc/) | Microchip 技術指引 | **M1** | 雙極性 CVD 差動相減公式 ($\Delta D = \text{Sample\_B} - \text{Sample\_A}$) 與採樣後 Idle 接地釋放。 |
| **[12]** | [TI SLAY045B (2017)](https://www.ti.com/lit/pdf/slay045) | TI 應用技術報告 | **M2, M4** | 工控與車用 IEC 61000-4-6 傳導抗擾與 IEC 61000-4-4 EFT 脈衝抗擾（多頻跳頻、離群濾除、動態門檻調整 DTA）。 |
| **[13]** | [NXP AN12217 (2018)](https://www.nxp.com/docs/en/application-note/AN12217.pdf) | NXP 應用手冊 | **M1, M5** | S32K1xx SAR ADC 32 次硬體平均自我校正、CALF 旗標驗證與內部 $1.0\text{ V}$ 帶隙基準線上監控。 |
| **[14]** | [NXP SPF-29248 & S32K1xx DS](https://community.nxp.com/pwmxy87654/attachments/pwmxy87654/S32K/9169/1/SPF-29248_C.pdf) | 原廠硬體電路圖與晶片手冊 | **M1, M5, M6** | PTC17、R525 ($4.7\text{ k}\Omega$)、PTC16、C533 ($10\text{ pF}$) 拓撲與 [S32K1xx 晶片手冊](https://www.mouser.com/pdfdocs/NXP_S32K1xx_DS.pdf) $C_{\text{ADIN}} \approx 4\text{ pF}$ 參數規格。 |

---

## 授權條款 (License)

BSD-3-Clause License. Copyright (c) 2026, Max97k. All rights reserved.
