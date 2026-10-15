/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : ADC0, PORTC, LPUART1, GPIO
*   Dependencies         : NXP RTD 3.0.0
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright 2026 Max97k
*   All Rights Reserved.
==================================================================================================*/

#include "Clock_Ip.h"
#include "touch_cvd.h"
#include "uart_telemetry.h"

/* Volatile symbols for GDB / FreeMASTER live debugging */
volatile Touch_M1_Report_t g_m1_report;
volatile Touch_RawSample_t g_current_sample;
volatile uint32_t          g_cycle_counter = 0U;
volatile boolean           g_touch_active  = FALSE;

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
    Telemetry_SendString("   Milestone 1: Bipolar Differential Physical Layer\r\n");
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

    /* Verify Criterion 2: Balance difference < 15 LSB */
    if (g_m1_report.nop_balance_diff[1] < M1_BALANCE_DIFF_MAX_LSB)
    {
        g_m1_report.criteria_2_balance_pass = TRUE;
        Telemetry_SendString("    [+] Criterion 2 (Balanced Diff < 15 LSB): PASSED\r\n");
    }
    else
    {
        g_m1_report.criteria_2_balance_pass = FALSE;
        Telemetry_SendString("    [-] Criterion 2 (Balanced Diff < 15 LSB): FAILED\r\n");
    }

    /* 8. Live Real-Time Continuous Acquisition Loop */
    Telemetry_SendString("\r\n[*] Entering Continuous Touch Sensing Loop (Touch SW7 to test)...\r\n");
    Touch_SetStatusLeds(FALSE, TRUE, FALSE); /* Green LED = Ready */

    while (1)
    {
        g_cycle_counter++;
        SimpleDelay(30000U); /* ~15 ms cycle time */

        g_current_sample = Touch_AcquireSingleDiffCVD(g_m1_report.optimal_nop);

        /* Touch detection threshold: delta_D shifts by >= 50 LSB from baseline */
        int16_t net_shift = g_current_sample.delta_D - g_m1_report.baseline_delta;

        if (net_shift >= 50)
        {
            g_touch_active = TRUE;
            /* Turn on Blue LED when touched, Green off */
            Touch_SetStatusLeds(FALSE, FALSE, TRUE);

            if (net_shift >= M1_TOUCH_DELTA_MIN_LSB)
            {
                g_m1_report.criteria_3_gain_pass = TRUE;
                g_m1_report.touch_delta_shift = net_shift;
            }
        }
        else
        {
            g_touch_active = FALSE;
            /* Turn on Green LED when idle */
            Touch_SetStatusLeds(FALSE, TRUE, FALSE);
        }

        /* Periodic telemetry output every 10 cycles (~150 ms) */
        if ((g_cycle_counter % 10U) == 0U)
        {
            Telemetry_SendString("LIVE,A:");
            Telemetry_SendUInt(g_current_sample.sample_A);
            Telemetry_SendString(",B:");
            Telemetry_SendUInt(g_current_sample.sample_B);
            Telemetry_SendString(",Delta:");
            Telemetry_SendInt(g_current_sample.delta_D);
            Telemetry_SendString(",NetShift:");
            Telemetry_SendInt(net_shift);
            Telemetry_SendString(g_touch_active ? " [TOUCHED]\r\n" : " [IDLE]\r\n");
        }
    }

    return 0;
}
