/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Integrated Touch Engine, VLPS Low-Power & LPIT Periodic Wakeup
*   Dependencies         : touch_sense.h, S32K144W.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
*   SPDX-License-Identifier: BSD-3-Clause
==================================================================================================*/

#include "touch_sense.h"
#include "S32K144W.h"
#include "S32K144W_SCG.h"
#include "S32K144W_PCC.h"
#include "S32K144W_SMC.h"
#include "S32K144W_LPIT.h"
#include "S32K144W_SYSTICK.h"

/*==================================================================================================
*                                   FUNCTION IMPLEMENTATIONS
*==================================================================================================*/

boolean Touch_Sense_Init(Touch_SenseEngine_t *engine)
{
    if (engine == NULL_PTR)
    {
        return FALSE;
    }

    /* 1. Initialize GPIO Pins and Status RGB LEDs */
    Touch_Init();

    /* 2. Execute ADC0 32x Self-Calibration per AN12217 */
    boolean calib_ok = Touch_Adc0_Calibrate();
    engine->m1_report.adc_calib_pass = calib_ok;

    /* 3. Read initial Bandgap Channel 27 and VREFH supply */
    uint16_t bg_raw = 0U;
    engine->m1_report.vrefh_mv = Touch_ReadBandgap_mV(&bg_raw);
    engine->m1_report.bandgap_raw = bg_raw;

    /* 4. Perform NOP Balancing Sweep across R525 */
    Touch_M1_NOP_Sweep(&engine->m1_report);

    /* 5. Collect 100 baseline samples to establish initial baseline delta */
    int32_t sum_A = 0;
    int32_t sum_B = 0;
    int32_t sum_delta = 0;

    for (uint32_t i = 0U; i < 100U; i++)
    {
        /* Short settle delay between initial calibration samples */
        for (volatile uint32_t d = 0U; d < 2000U; d++) { __asm__ volatile ("nop"); }

        Touch_RawSample_t s = Touch_AcquireSingleDiffCVD(engine->m1_report.optimal_nop);
        sum_A += (int32_t)s.sample_A;
        sum_B += (int32_t)s.sample_B;
        sum_delta += (int32_t)s.delta_D;
    }

    engine->m1_report.baseline_sample_A = (uint16_t)(sum_A / 100);
    engine->m1_report.baseline_sample_B = (uint16_t)(sum_B / 100);
    engine->m1_report.baseline_delta    = (int16_t)(sum_delta / 100);

    /* 6. Initialize Milestone 2 1D Adaptive Scalar Kalman Filter */
    Touch_Kalman_Init(&engine->kalman,
                      (float)engine->m1_report.baseline_delta,
                      TOUCH_KALMAN_DEFAULT_Q,
                      TOUCH_KALMAN_DEFAULT_RBASE,
                      TOUCH_KALMAN_DEFAULT_ALPHA);

    /* 7. Initialize Milestone 3 4-Zone Asymmetric Baseline Tracker */
    Touch_Baseline_Init(&engine->baseline_tracker,
                        (float)engine->m1_report.baseline_delta,
                        TOUCH_THRESH_TOUCH_LSB);

    /* 8. Initialize Milestone 4 TinyML Temporal Envelope Classifier */
    Touch_TinyML_Init(&engine->tinyml);

    /* 9. Initialize Milestone 5 On-line Defect Diagnostics Engine */
    Touch_Diag_Init(&engine->diag);
    engine->diag.last_balance_diff = engine->m1_report.min_balance_diff;

    /* 10. Initialize Low-Power VLPS & LPIT Hardware Subsystem */
    Touch_Sense_LowPower_Init();

    /* 11. Initialize Engine State Context */
    engine->lfsr_state              = TOUCH_LFSR_DEFAULT_SEED;
    engine->cycle_count             = 0U;
    engine->burnin_cycles           = 0U;
    engine->burnin_false_positives  = 0U;
    engine->idle_cycles_count       = 0U;
    engine->current_power_mode      = TOUCH_POWER_RUN_HIGH_RATE;
    engine->last_exec_time_ticks    = 0U;
    engine->max_exec_time_us        = 0U;

    return calib_ok;
}

void Touch_Sense_ProcessCycle(Touch_SenseEngine_t *engine, Touch_SenseResult_t *result)
{
    if ((engine == NULL_PTR) || (result == NULL_PTR))
    {
        return;
    }

    /* Start execution cycle timer using SysTick CVR down-counter (48 ticks per microsecond) */
    uint32_t start_cvr = S32_SysTick->CVR;

    engine->cycle_count++;

    /* Milestone 2 Step 1: 9x Coprime Multi-Frequency Jittered Burst Acquisition */
    int16_t burst[TOUCH_BURST_SAMPLE_COUNT];
    Touch_AcquireBurstMFP(burst, TOUCH_BURST_SAMPLE_COUNT, engine->m1_report.optimal_nop, &engine->lfsr_state);

    /* Cache representative raw midpoint sample */
    Touch_RawSample_t cur_sample = g_last_burst_raw_sample;

    /* Milestone 2 Step 2: Non-linear Alpha-Trimmed Mean */
    float z_meas = 0.0f;
    float spread = 0.0f;
    Touch_AlphaTrimmedMean(burst, TOUCH_BURST_SAMPLE_COUNT, &z_meas, &spread);
    boolean spike_rejected = Touch_DetectBurstSpike(burst, TOUCH_BURST_SAMPLE_COUNT);

    /* Milestone 2 Step 3: 1D Adaptive Scalar Kalman Filter Update */
    float x_est = Touch_Kalman_Update(&engine->kalman, z_meas, spread);

    /* Milestone 3: 4-Zone Asymmetric Dynamic Baseline Tracking */
    uint32_t frame_period_ms = (engine->current_power_mode == TOUCH_POWER_VLPS_LOW_RATE) ?
                               TOUCH_LPIT_WAKEUP_PERIOD_MS : TOUCH_RUN_FRAME_PERIOD_MS;
    float dynamic_delta = Touch_Baseline_Update(&engine->baseline_tracker, x_est, frame_period_ms);

    /* Milestone 4: TinyML Temporal Envelope Classifier Engine */
    boolean tinyml_touch = Touch_TinyML_Update(&engine->tinyml, dynamic_delta, spread, spike_rejected);
    Touch_ClassificationClass_t detected_class = Touch_TinyML_GetClass(&engine->tinyml);

    /* Periodic Background Diagnostic Refresh (every 10 cycles, ~150 ms) */
    if ((engine->cycle_count % 10U) == 0U)
    {
        uint16_t bg_raw = 0U;
        engine->m1_report.vrefh_mv = Touch_ReadBandgap_mV(&bg_raw);
        engine->m1_report.bandgap_raw = bg_raw;
        engine->diag.last_balance_diff = Touch_MeasureBalanceDiff(engine->m1_report.optimal_nop);
    }

    /* Milestone 5: Execute On-line Hardware Defect Diagnostics Cycle */
    uint32_t active_faults = Touch_Diag_RunCycle(&engine->diag,
                                                 cur_sample.sample_A,
                                                 cur_sample.sample_B,
                                                 engine->diag.last_balance_diff,
                                                 engine->m1_report.vrefh_mv);

    /* Multi-dimensional Touch Decision Fusion */
    boolean touch_candidate = tinyml_touch;

    /* Milestone 5: Hardware Plausibility Diagnostics Fail-Safe Interlock Clamp */
    boolean final_touch = FALSE;
    if (active_faults != TOUCH_FAULT_NONE)
    {
        /* Hard Safety Interlock: When any hardware fault is active, force touch to FALSE */
        final_touch = FALSE;
        Touch_SetStatusLeds(TRUE, FALSE, FALSE); /* Red LED = Fault */
    }
    else
    {
        final_touch = touch_candidate;
        if (final_touch)
        {
            Touch_SetStatusLeds(FALSE, FALSE, TRUE); /* Blue LED = Touch Confirmed */
        }
        else
        {
            Touch_SetStatusLeds(FALSE, TRUE, FALSE); /* Green LED = Ready / Idle */
        }
    }

    /* Power Mode Gear Shift Decision */
    if (final_touch || (dynamic_delta >= TOUCH_GEAR_WAKE_DELTA_LSB) || (active_faults != TOUCH_FAULT_NONE))
    {
        /* Active or approaching: maintain high-rate 15 ms frame rate */
        engine->current_power_mode = TOUCH_POWER_RUN_HIGH_RATE;
        engine->idle_cycles_count = 0U;
        result->recommended_mode = TOUCH_POWER_RUN_HIGH_RATE;
    }
    else
    {
        engine->idle_cycles_count++;
        if (engine->idle_cycles_count >= TOUCH_GEAR_IDLE_CYCLES_BEFORE_SLEEP)
        {
            /* Settle into ultra-low power 25 ms VLPS standby */
            engine->current_power_mode = TOUCH_POWER_VLPS_LOW_RATE;
            result->recommended_mode = TOUCH_POWER_VLPS_LOW_RATE;
        }
        else
        {
            result->recommended_mode = TOUCH_POWER_RUN_HIGH_RATE;
        }
    }

    /* Burn-in Statistics Tracking */
    engine->burnin_cycles++;
    if ((active_faults == TOUCH_FAULT_NONE) && final_touch && (dynamic_delta < TOUCH_MIN_GLOVE_DELTA_LSB))
    {
        engine->burnin_false_positives++;
    }

    /* Stop execution cycle timer and compute execution time in microseconds */
    uint32_t end_cvr = S32_SysTick->CVR;
    uint32_t elapsed_ticks = (start_cvr >= end_cvr) ? (start_cvr - end_cvr) : (start_cvr + 720000U - end_cvr);
    uint32_t exec_us = elapsed_ticks / 48U;
    if (exec_us > engine->max_exec_time_us)
    {
        engine->max_exec_time_us = exec_us;
    }
    engine->last_exec_time_ticks = elapsed_ticks;

    /* Fill Result Structure */
    result->is_touched       = final_touch;
    result->detected_class   = detected_class;
    result->dynamic_delta    = dynamic_delta;
    result->x_est            = x_est;
    result->baseline         = engine->baseline_tracker.baseline;
    result->confidence       = engine->tinyml.confidence;
    result->is_healthy       = (active_faults == TOUCH_FAULT_NONE);
    result->active_faults    = active_faults;
    result->exec_time_us     = exec_us;
}

void Touch_Sense_LowPower_Init(void)
{
    /* 1. Ensure SIRC (8.0 MHz) remains enabled in Stop / VLPS mode */
    IP_SCG->SIRCCSR |= SCG_SIRCCSR_SIRCSTEN_MASK | SCG_SIRCCSR_SIRCLPEN_MASK;

    /* Set SIRCDIV2 to divide by 2 (4.0 MHz) */
    IP_SCG->SIRCDIV |= SCG_SIRCDIV_SIRCDIV2(2U);

    /* 2. Clock LPIT0 from SIRCDIV2 (PCS=2) */
    IP_PCC->PCCn[PCC_LPIT_INDEX] &= ~PCC_PCCn_CGC_MASK;
    IP_PCC->PCCn[PCC_LPIT_INDEX] = PCC_PCCn_PCS(2U) | PCC_PCCn_CGC_MASK;

    /* 3. Configure LPIT0 Module Control: Enable Module in Run & Debug/Doze */
    IP_LPIT0->MCR = LPIT_MCR_M_CEN_MASK | LPIT_MCR_DOZE_EN_MASK;

    /* Channel 0: 25 ms period @ 4.0 MHz = 100,000 counts */
    IP_LPIT0->TMR[0].TVAL = 100000U;
    IP_LPIT0->TMR[0].TCTRL = LPIT_TMR_TCTRL_T_EN_MASK;

    /* Enable Channel 0 Interrupt */
    IP_LPIT0->MIER |= LPIT_MIER_TIE0_MASK;

    /* NVIC line is enabled only inside Touch_Sense_EnterVLPS() with PRIMASK set:
     * the vector table entry is undefined_handler, so the IRQ must never be taken. */

    /* 4. Allow VLPS in SMC (Power Mode Protection) */
    IP_SMC->PMPROT |= SMC_PMPROT_AVLP_MASK;
}

void Touch_Sense_EnterVLPS(void)
{
    /* 1. Ensure idle grounding on touch electrodes before entering deep sleep */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);

    /* 2. Configure SMC to transition into VLPS on deep sleep (STOPM = 2) */
    IP_SMC->PMCTRL = (IP_SMC->PMCTRL & ~SMC_PMCTRL_STOPM_MASK) | SMC_PMCTRL_STOPM(2U);

    /* 3. Set ARM Cortex-M SLEEPDEEP bit */
    S32_SCB->SCR |= S32_SCB_SCR_SLEEPDEEP_MASK;

    /* 4. Disable IRQ dispatch so WFI wakes up and resumes inline without vectoring to undefined_handler */
    __asm__ volatile ("cpsid i");
    S32_NVIC->ISER[LPIT0_Ch0_IRQn / 32U] = (1U << (LPIT0_Ch0_IRQn % 32U));

    /* 5. Execute Wait-For-Interrupt instruction to enter VLPS sleep */
    __asm__ volatile ("wfi");

    /* Wake-up done: clear flag, then drop pending state and disable the NVIC line */
    IP_LPIT0->MSR = LPIT_MSR_TIF0_MASK;
    S32_NVIC->ICER[LPIT0_Ch0_IRQn / 32U] = (1U << (LPIT0_Ch0_IRQn % 32U));
    S32_NVIC->ICPR[LPIT0_Ch0_IRQn / 32U] = (1U << (LPIT0_Ch0_IRQn % 32U));

    /* 6. Re-enable IRQ dispatch */
    __asm__ volatile ("cpsie i");

    /* 7. Clear SLEEPDEEP upon wakeup */
    S32_SCB->SCR &= ~S32_SCB_SCR_SLEEPDEEP_MASK;

    /* 8. Clear LPIT0 channel 0 flag if fired */
    if ((IP_LPIT0->MSR & LPIT_MSR_TIF0_MASK) != 0U)
    {
        IP_LPIT0->MSR = LPIT_MSR_TIF0_MASK;
    }
}

boolean Touch_Sense_IsHealthy(const Touch_SenseEngine_t *engine)
{
    if (engine == NULL_PTR)
    {
        return FALSE;
    }
    return Touch_Diag_IsHealthy(&engine->diag);
}
