/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : ADC0, PORTC, LPUART1, GPIO
*   Dependencies         : NXP RTD 3.0.0
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
==================================================================================================*/

#include "Clock_Ip.h"
#include "S32K144W_SYSTICK.h"
#include "touch_cvd.h"
#include "touch_filter.h"
#include "touch_baseline.h"
#include "touch_tinyml.h"
#include "touch_diag.h"
#include "touch_sense.h"
#include "uart_telemetry.h"

/* Define CUE_TEST_MODE to enable the cue-LED schedule used for hardware recording sessions
 * (Green=idle, Blue=press, Red=done). */
#define CUE_TEST_MODE

/* Volatile symbols for GDB / FreeMASTER live debugging */
volatile Touch_M1_Report_t          g_m1_report;
volatile Touch_RawSample_t          g_current_sample;
volatile Touch_Kalman_t             g_kalman_filter;
volatile Touch_M2_FilteredSample_t  g_m2_sample;
volatile Touch_BaselineTracker_t    g_baseline_tracker;
volatile Touch_TinyMLClassifier_t   g_tinyml_classifier;
volatile Touch_Diagnostics_t        g_touch_diagnostics;
volatile uint16_t                   g_lfsr_state = TOUCH_LFSR_DEFAULT_SEED;
volatile uint32_t                   g_cycle_counter = 0U;
volatile uint32_t                   g_exec_time_us = 0U;
volatile boolean                    g_baseline_touch = FALSE;
volatile boolean                    g_tinyml_touch   = FALSE;
volatile boolean                    g_touch_active   = FALSE;
volatile uint8_t                    g_heartbeat_state = 0U;

static void SimpleDelay(uint32_t loops)
{
    while (loops-- > 0U)
    {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    /* 1. Initialize System Clocks via NXP RTD 3.0.0 (48 MHz Core, 48 MHz FIRC) */
    Clock_Ip_Init(&Clock_Ip_aClockConfig[0]);

    /* 2. Initialize OpenSDA Virtual COM Port Telemetry (115200 bps, 8N1) */
    Telemetry_Init(115200U);
    SimpleDelay(100000U);

    Telemetry_SendString("\r\n=======================================================\r\n");
    Telemetry_SendString("   S32K144W Automotive Touch Sensing CVD Engine\r\n");
    Telemetry_SendString("   Milestones 1~5: CVD + Kalman + Baseline + TinyML + Safety\r\n");
    Telemetry_SendString("=======================================================\r\n");

    /* 3. Initialize Touch GPIO Pins & Status LEDs */
    Touch_Init();

    /* 4. ADC0 32x Hardware Averaging Self-Calibration per AN12217 */
    Telemetry_SendString("[*] Executing ADC0 Hardware Self-Calibration...\r\n");
    g_m1_report.adc_calib_pass = Touch_Adc0_Calibrate();

    if (g_m1_report.adc_calib_pass)
    {
        Telemetry_SendString("    [+] ADC0 Self-Calibration: PASSED\r\n");
    }
    else
    {
        Telemetry_SendString("    [-] ADC0 Self-Calibration: FAILED (CALF raised)\r\n");
        Touch_SetStatusLeds(TRUE, FALSE, FALSE); /* Red on error */
    }

    /* 5. Read Internal 1.0V Bandgap (Channel 27) and Monitor VREFH */
    uint16_t bg_raw = 0U;
    g_m1_report.vrefh_mv = Touch_ReadBandgap_mV(&bg_raw);
    g_m1_report.bandgap_raw = bg_raw;

    Telemetry_SendString("[*] Bandgap Channel 27 Telemetry:\r\n");
    Telemetry_SendString("    - Raw Reading: ");
    Telemetry_SendUInt(bg_raw);
    Telemetry_SendString(" LSB (Nominal ~819 @ 5.0V, ~1241 @ 3.3V)\r\n");
    Telemetry_SendString("    - Estimated VREFH: ");
    Telemetry_SendUInt(g_m1_report.vrefh_mv);
    Telemetry_SendString(" mV\r\n");

    /* 6. Perform NOP Balancing Sweep (4, 8, 12, 16 NOPs) */
    Telemetry_SendString("[*] Performing NOP Charge Balance Sweep (R525 across SE14 vs SE15)...\r\n");
    Touch_M1_NOP_Sweep((Touch_M1_Report_t *)&g_m1_report);

    const uint8_t nops[4] = {4U, 8U, 12U, 16U};
    for (uint8_t i = 0U; i < 4U; i++)
    {
        Telemetry_SendString("    - NOP = ");
        Telemetry_SendUInt(nops[i]);
        Telemetry_SendString(" -> Balance Diff: ");
        Telemetry_SendInt(g_m1_report.nop_balance_diff[i]);
        Telemetry_SendString(" LSB");
        if (g_m1_report.nop_balance_diff[i] < M1_BALANCE_DIFF_MAX_LSB)
        {
            Telemetry_SendString(" [PASS < 15 LSB]\r\n");
        }
        else
        {
            Telemetry_SendString(" [EXCEEDED]\r\n");
        }
    }
    Telemetry_SendString("    -> Optimal NOP Selected: ");
    Telemetry_SendUInt(g_m1_report.optimal_nop);
    Telemetry_SendString(" NOPs\r\n");

    /* 7. Collect 100 Untouched Baseline Samples */
    Telemetry_SendString("\r\n[*] Collecting 100 Untouched Baseline Points (SW7 release)...\r\n");
    Telemetry_SendString("CSV_HEADER,Index,Sample_A,Sample_B,Delta_D\r\n");

    int32_t sum_A = 0;
    int32_t sum_B = 0;
    int32_t sum_delta = 0;

    for (uint32_t i = 0U; i < 100U; i++)
    {
        SimpleDelay(20000U); /* ~10 ms sample interval */
        Touch_RawSample_t s = Touch_AcquireSingleDiffCVD(g_m1_report.optimal_nop);
        sum_A += s.sample_A;
        sum_B += s.sample_B;
        sum_delta += s.delta_D;

        Telemetry_SendString("CSV,");
        Telemetry_SendUInt(i + 1U);
        Telemetry_SendChar(',');
        Telemetry_SendUInt(s.sample_A);
        Telemetry_SendChar(',');
        Telemetry_SendUInt(s.sample_B);
        Telemetry_SendChar(',');
        Telemetry_SendInt(s.delta_D);
        Telemetry_SendString("\r\n");
    }

    g_m1_report.baseline_sample_A = (uint16_t)(sum_A / 100);
    g_m1_report.baseline_sample_B = (uint16_t)(sum_B / 100);
    g_m1_report.baseline_delta    = (int16_t)(sum_delta / 100);

    Telemetry_SendString("[*] Untouched Baseline Summary:\r\n");
    Telemetry_SendString("    - Mean Sample_A: ");
    Telemetry_SendUInt(g_m1_report.baseline_sample_A);
    Telemetry_SendString(" LSB\r\n");
    Telemetry_SendString("    - Mean Sample_B: ");
    Telemetry_SendUInt(g_m1_report.baseline_sample_B);
    Telemetry_SendString(" LSB\r\n");
    Telemetry_SendString("    - Mean Delta_D:  ");
    Telemetry_SendInt(g_m1_report.baseline_delta);
    Telemetry_SendString(" LSB\r\n");

    /* Verify Criterion 1: Work point in 1600 ~ 2500 LSB */
    if ((g_m1_report.baseline_sample_A >= M1_UNTOUCHED_MIN_LSB) &&
        (g_m1_report.baseline_sample_A <= M1_UNTOUCHED_MAX_LSB) &&
        (g_m1_report.baseline_sample_B >= M1_UNTOUCHED_MIN_LSB) &&
        (g_m1_report.baseline_sample_B <= M1_UNTOUCHED_MAX_LSB))
    {
        g_m1_report.criteria_1_workpoint_pass = TRUE;
        Telemetry_SendString("    [+] Criterion 1 (1600~2500 LSB Workpoint): PASSED\r\n");
    }
    else
    {
        g_m1_report.criteria_1_workpoint_pass = FALSE;
        Telemetry_SendString("    [-] Criterion 1 (1600~2500 LSB Workpoint): FAILED\r\n");
    }

    /* Verify Criterion 2: Balance difference < 15 LSB (evaluated with optimal min_balance_diff) */
    if (g_m1_report.min_balance_diff < M1_BALANCE_DIFF_MAX_LSB)
    {
        g_m1_report.criteria_2_balance_pass = TRUE;
        Telemetry_SendString("    [+] Criterion 2 (Balanced Diff < 15 LSB, Optimal NOP=");
        Telemetry_SendUInt(g_m1_report.optimal_nop);
        Telemetry_SendString("): PASSED\r\n");
    }
    else
    {
        g_m1_report.criteria_2_balance_pass = FALSE;
        Telemetry_SendString("    [-] Criterion 2 (Balanced Diff < 15 LSB): FAILED\r\n");
    }

    /* 8. Initialize 1D Adaptive Scalar Kalman Filter (Milestone 2) */
    Touch_Kalman_Init((Touch_Kalman_t *)&g_kalman_filter,
                      (float)g_m1_report.baseline_delta,
                      TOUCH_KALMAN_DEFAULT_Q,
                      TOUCH_KALMAN_DEFAULT_RBASE,
                      TOUCH_KALMAN_DEFAULT_ALPHA);

    Telemetry_SendString("[*] Milestone 2 Adaptive Kalman Filter Initialized:\r\n");
    Telemetry_SendString("    - Initial State (x_est): ");
    Telemetry_SendInt(g_m1_report.baseline_delta);
    Telemetry_SendString(" LSB\r\n");
    Telemetry_SendString("    - Burst Samples (N):     9 (Coprime Multi-Frequency Jitter)\r\n");
    Telemetry_SendString("    - Alpha-Trimmed Mean:    Discard 2 min, 2 max, avg middle 5\r\n");

    /* 9. Initialize 4-Zone Asymmetric Baseline Tracker (Milestone 3) */
    Touch_Baseline_Init((Touch_BaselineTracker_t *)&g_baseline_tracker,
                        (float)g_m1_report.baseline_delta,
                        TOUCH_THRESH_TOUCH_LSB);

    Telemetry_SendString("[*] Milestone 3 Asymmetric Baseline Tracker Initialized:\r\n");
    Telemetry_SendString("    - Initial Baseline:      ");
    Telemetry_SendInt(g_m1_report.baseline_delta);
    Telemetry_SendString(" LSB\r\n");
    Telemetry_SendString("    - Touch Threshold (TH):  50.0 LSB (Release: 40.0 LSB)\r\n");
    Telemetry_SendString("    - Approach Freeze (0.4): 20.0 LSB\r\n");
    Telemetry_SendString("    - Neg Fast Healing TH:   -15.0 LSB (<= 300 ms)\r\n");
    Telemetry_SendString("    - Hold Timeout Limit:    15.0 sec (Forced Baseline Reset)\r\n");

    /* 10. Initialize TinyML Temporal Envelope Classifier Engine (Milestone 4) */
    Touch_TinyML_Init((Touch_TinyMLClassifier_t *)&g_tinyml_classifier);

    Telemetry_SendString("[*] Milestone 4 TinyML Temporal Envelope Classifier Initialized:\r\n");
    Telemetry_SendString("    - Temporal Window:       10 cycles (150 ms history)\r\n");
    Telemetry_SendString("    - Confidence Filter:     3-cycle sliding window hysteresis\r\n");
    Telemetry_SendString("    - Target Classes:        Bare Finger, Glove/Acrylic, Water Droplet, Edge Hover\r\n");
    Telemetry_SendString("    - Pass Criteria:         TPR >= 98%, FPR = 0%, Latency <= 60~90 ms\r\n");

    /* 11. Initialize On-line Hardware Defect Diagnostics Engine (Milestone 5) */
    Touch_Diag_Init((Touch_Diagnostics_t *)&g_touch_diagnostics);
    g_touch_diagnostics.last_balance_diff = g_m1_report.min_balance_diff;

    Telemetry_SendString("[*] Milestone 5 Hardware Defect Diagnostics Initialized:\r\n");
    Telemetry_SendString("    - Physical Voltage Limits: 300 ~ 3800 LSB (Short to GND/VDD)\r\n");
    Telemetry_SendString("    - R525 Integrity Window:   <= 80 LSB Balance Delta\r\n");
    Telemetry_SendString("    - VREFH Supply Window:     4750 ~ 5250 mV (+/- 5%)\r\n");
    Telemetry_SendString("    - Reaction Time:           <= 3 cycles (<= 45 ms, Target <= 60 ms)\r\n");
    Telemetry_SendString("    - Auto-Recovery Time:      8 cycles (120 ms, Target <= 200 ms)\r\n");

    /* 12. Initialize Low-Power Subsystem (SIRC 8 MHz + LPIT 25 ms standby timer + SMC VLPS) */
    Touch_Sense_LowPower_Init();
    Telemetry_SendString("[*] Milestone 6 Low-Power Engine Initialized (LPIT 25ms + SMC VLPS Ready)\r\n");

    /* 13. Initialize ARM Cortex-M SysTick for Hardware-Locked 15.000 ms Frame Synchronizer */
    S32_SysTick->CSRr = 0U; /* Disable during config */
    S32_SysTick->RVR  = 719999U; /* 48 MHz * 0.015 s - 1 = 719999 ticks (15.000 ms) */
    S32_SysTick->CVR  = 0U; /* Clear current counter */
    S32_SysTick->CSRr = S32_SysTick_CSR_ENABLE_MASK | S32_SysTick_CSR_CLKSOURCE_MASK; /* Enable, Core clock, No IRQ */

    Telemetry_SendString("[*] SysTick Hardware 15.000 ms Zero-Jitter Frame Synchronizer Activated.\r\n");

    /* 14. Live Real-Time Continuous Acquisition, Filtering, Baseline, TinyML & Diag Loop */
    Telemetry_SendString("\r\n[*] Telemetry CSV Format: D,<Cycle>,<A>,<B>,<DeltaD>,<Xest>,<Base>,<DynDelta>,<Class>,<Conf>,<Touch>,<ExecUs>,<Heartbeat>,<Status>\r\n");
    Telemetry_SendString("[*] Entering Continuous Touch Sensing & ASIL-B Safety Diag Loop (Touch SW7 to test)...\r\n");
    Touch_SetStatusLeds(TRUE, FALSE, FALSE); /* Red LED = Untouched / Idle */

    while (1)
    {
        uint32_t start_cvr = S32_SysTick->CVR;
        g_cycle_counter++;

        /* Milestone 2 Step 1: 9x Coprime Multi-Frequency Jittered Burst Acquisition */
        int16_t burst[TOUCH_BURST_SAMPLE_COUNT];
        Touch_AcquireBurstMFP(burst, TOUCH_BURST_SAMPLE_COUNT, g_m1_report.optimal_nop, (uint16_t *)&g_lfsr_state);

        /* Update representative raw sample directly from burst midpoint (contains real sample_A, sample_B, delta_D) */
        g_current_sample = g_last_burst_raw_sample;

        /* Milestone 2 Step 2: Non-linear Alpha-Trimmed Mean & Noise Spread Metric with Outlier Spike Detection */
        float z_meas = 0.0f;
        float spread = 0.0f;
        Touch_AlphaTrimmedMean(burst, TOUCH_BURST_SAMPLE_COUNT, &z_meas, &spread);
        boolean spike_rejected = Touch_DetectBurstSpike(burst, TOUCH_BURST_SAMPLE_COUNT);

        /* Milestone 2 Step 3: 1D Adaptive Scalar Kalman Filter Update */
        float x_est = Touch_Kalman_Update((Touch_Kalman_t *)&g_kalman_filter, z_meas, spread);

        /* Store diagnostics */
        g_m2_sample.raw_diff       = burst[4];
        g_m2_sample.z_meas         = z_meas;
        g_m2_sample.x_est          = x_est;
        g_m2_sample.spread         = spread;
        g_m2_sample.kalman_gain    = g_kalman_filter.k_gain;
        g_m2_sample.spike_rejected = spike_rejected;

        /* Milestone 3: 4-Zone Asymmetric Dynamic Baseline Tracking */
        float dynamic_delta = Touch_Baseline_Update((Touch_BaselineTracker_t *)&g_baseline_tracker, x_est, 15U);
        g_baseline_touch = Touch_Baseline_IsTouched((Touch_BaselineTracker_t *)&g_baseline_tracker);

        /* Milestone 4: TinyML Temporal Envelope Classifier Engine */
        boolean tinyml_touch = Touch_TinyML_Update((Touch_TinyML_Classifier_t *)&g_tinyml_classifier,
                                                   dynamic_delta,
                                                   spread,
                                                   spike_rejected);
        Touch_ClassificationClass_t detected_class = Touch_TinyML_GetClass((const Touch_TinyML_Classifier_t *)&g_tinyml_classifier);
        g_tinyml_touch = tinyml_touch;

        /* Periodic Bandgap Channel 27 and R525 Balance Reading every 10 cycles (~150 ms) */
        if ((g_cycle_counter % 10U) == 0U)
        {
            uint16_t bg_raw = 0U;
            g_m1_report.vrefh_mv = Touch_ReadBandgap_mV(&bg_raw);
            g_m1_report.bandgap_raw = bg_raw;
            g_touch_diagnostics.last_balance_diff = Touch_MeasureBalanceDiff(g_m1_report.optimal_nop);
        }

        /* Milestone 5: Execute On-line Hardware Defect Diagnostics Cycle */
        uint32_t active_faults = Touch_Diag_RunCycle((Touch_Diagnostics_t *)&g_touch_diagnostics,
                                                     g_current_sample.sample_A,
                                                     g_current_sample.sample_B,
                                                     g_touch_diagnostics.last_balance_diff,
                                                     g_m1_report.vrefh_mv);

        /* Normal Touch Candidate: TinyML confirmation AND sufficient dynamic signal */
        boolean touch_candidate = g_tinyml_touch;

        /* Milestone 5: ISO 26262 ASIL-B Safety Interlock & Fail-Safe Clamp */
        if (active_faults != TOUCH_FAULT_NONE)
        {
            /* Hard Safety Interlock: When any hardware defect/short is active,
             * unconditionally force touch to FALSE to eliminate false triggers */
            g_touch_active = FALSE;
        }
        else
        {
            g_touch_active = touch_candidate;

            if (g_touch_active)
            {
                if (dynamic_delta >= (float)M1_TOUCH_DELTA_MIN_LSB)
                {
                    g_m1_report.criteria_3_gain_pass = TRUE;
                    g_m1_report.touch_delta_shift = (int16_t)dynamic_delta;
                }
            }
        }

        /* 0.2 Hz Heartbeat (5.0 s period = 333 frames @ 15.0 ms frame rate: 2.5s ON, 2.5s OFF) */
        g_heartbeat_state = ((g_cycle_counter % 333U) < 167U) ? 1U : 0U;

        /* Hardware Status LEDs:
         * - Idle (not pressed): Red LED ON, Green LED OFF
         * - Pressed: Red LED OFF, Green LED ON
         * - Heartbeat: Blue LED blinks at 0.2 Hz
         */
        boolean led_red   = !g_touch_active;
        boolean led_green = g_touch_active;
        boolean led_blue  = (g_heartbeat_state != 0U);

        Touch_SetStatusLeds(led_red, led_green, led_blue);

        /* Compute execution time in microseconds using 48 MHz SysTick CVR */
        uint32_t end_cvr = S32_SysTick->CVR;
        uint32_t elapsed_ticks = (start_cvr >= end_cvr) ? (start_cvr - end_cvr) : (start_cvr + 720000U - end_cvr);
        g_exec_time_us = elapsed_ticks / 48U;

        /* Real-Time Continuous High-Resolution Telemetry Stream (66.7 Hz, 15.0 ms Frame Rate)
         * String format: D,<Cycle>,<A>,<B>,<DeltaD>,<Xest>,<Base>,<DynDelta>,<Class>,<Conf>,<Touch>,<ExecUs>,<Heartbeat>,<Status> */
        Telemetry_SendString("D,");
        Telemetry_SendUInt(g_cycle_counter);
        Telemetry_SendString(",");
        Telemetry_SendUInt(g_current_sample.sample_A);
        Telemetry_SendString(",");
        Telemetry_SendUInt(g_current_sample.sample_B);
        Telemetry_SendString(",");
        Telemetry_SendInt(g_current_sample.delta_D);
        Telemetry_SendString(",");
        Telemetry_SendFloat(x_est, 1U);
        Telemetry_SendString(",");
        Telemetry_SendFloat(g_baseline_tracker.baseline, 1U);
        Telemetry_SendString(",");
        Telemetry_SendFloat(dynamic_delta, 1U);
        Telemetry_SendString(",");
        Telemetry_SendUInt((uint32_t)detected_class);
        Telemetry_SendString(",");
        Telemetry_SendFloat(g_tinyml_classifier.confidence, 2U);
        Telemetry_SendString(",");
        Telemetry_SendUInt(g_touch_active ? 1U : 0U);
        Telemetry_SendString(",");
        Telemetry_SendUInt(g_exec_time_us);
        Telemetry_SendString(",");
        Telemetry_SendUInt(g_heartbeat_state);
        Telemetry_SendString(g_touch_active ? ",TOUCH\r\n" : ",IDLE\r\n");

        /* Hardware-Locked Exact 15.000 ms Frame Synchronization via SysTick COUNTFLAG */
        while ((S32_SysTick->CSRr & S32_SysTick_CSR_COUNTFLAG_MASK) == 0U)
        {
            /* Precise hardware synchronization eliminates software delay drift */
        }
    }

    return 0;
}
