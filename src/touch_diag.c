/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Hardware Plausibility Diagnostics & Defensive Safety Engine
*   Dependencies         : touch_diag.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
*   SPDX-License-Identifier: BSD-3-Clause
==================================================================================================*/

#include "touch_diag.h"

/*==================================================================================================
*                                   FUNCTION IMPLEMENTATIONS
*==================================================================================================*/

void Touch_Diag_Init(Touch_Diagnostics_t *diag)
{
    if (diag == NULL_PTR)
    {
        return;
    }

    diag->active_faults       = TOUCH_FAULT_NONE;
    diag->latched_faults      = TOUCH_FAULT_NONE;
    diag->is_healthy          = TRUE;

    diag->short_gnd_a_cnt     = 0U;
    diag->short_gnd_b_cnt     = 0U;
    diag->short_vdd_a_cnt     = 0U;
    diag->short_vdd_b_cnt     = 0U;
    diag->r525_open_cnt       = 0U;
    diag->vref_drift_cnt      = 0U;
    diag->healthy_cycles_cnt  = 0U;

    diag->last_sample_A       = 2048U;
    diag->last_sample_B       = 2048U;
    diag->last_balance_diff   = 0;
    diag->last_vrefh_mv       = TOUCH_DIAG_VREFH_NOMINAL_MV;

    diag->total_fault_events  = 0U;
    diag->total_recoveries    = 0U;
}

uint32_t Touch_Diag_RunCycle(Touch_Diagnostics_t *diag,
                             uint16_t sample_A,
                             uint16_t sample_B,
                             int16_t balance_diff,
                             uint32_t vrefh_mv)
{
    if (diag == NULL_PTR)
    {
        return TOUCH_FAULT_NONE;
    }

    diag->last_sample_A     = sample_A;
    diag->last_sample_B     = sample_B;
    diag->last_balance_diff = balance_diff;
    diag->last_vrefh_mv     = vrefh_mv;

    uint32_t raw_faults = TOUCH_FAULT_NONE;

    /* ---------------------------------------------------------------------------------------------
     * Diagnostic 1: Electrode Short-to-GND Detection (< 300 LSB)
     * --------------------------------------------------------------------------------------------- */
    if (sample_A < TOUCH_DIAG_SAMPLE_MIN_LSB)
    {
        if (diag->short_gnd_a_cnt < TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            diag->short_gnd_a_cnt++;
        }
        if (diag->short_gnd_a_cnt >= TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            raw_faults |= TOUCH_FAULT_SHORT_GND_A;
        }
    }
    else
    {
        diag->short_gnd_a_cnt = 0U;
    }

    if (sample_B < TOUCH_DIAG_SAMPLE_MIN_LSB)
    {
        if (diag->short_gnd_b_cnt < TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            diag->short_gnd_b_cnt++;
        }
        if (diag->short_gnd_b_cnt >= TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            raw_faults |= TOUCH_FAULT_SHORT_GND_B;
        }
    }
    else
    {
        diag->short_gnd_b_cnt = 0U;
    }

    /* ---------------------------------------------------------------------------------------------
     * Diagnostic 2: Electrode Short-to-VDD Detection (> 3800 LSB)
     * --------------------------------------------------------------------------------------------- */
    if (sample_A > TOUCH_DIAG_SAMPLE_MAX_LSB)
    {
        if (diag->short_vdd_a_cnt < TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            diag->short_vdd_a_cnt++;
        }
        if (diag->short_vdd_a_cnt >= TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            raw_faults |= TOUCH_FAULT_SHORT_VDD_A;
        }
    }
    else
    {
        diag->short_vdd_a_cnt = 0U;
    }

    if (sample_B > TOUCH_DIAG_SAMPLE_MAX_LSB)
    {
        if (diag->short_vdd_b_cnt < TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            diag->short_vdd_b_cnt++;
        }
        if (diag->short_vdd_b_cnt >= TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            raw_faults |= TOUCH_FAULT_SHORT_VDD_B;
        }
    }
    else
    {
        diag->short_vdd_b_cnt = 0U;
    }

    /* ---------------------------------------------------------------------------------------------
     * Diagnostic 3: Bridging Resistor R525 (10k) Degradation / Open Circuit (|diff| > 250 LSB)
     * --------------------------------------------------------------------------------------------- */
    int16_t abs_bal_diff = (balance_diff < 0) ? (int16_t)(-balance_diff) : balance_diff;
    if (abs_bal_diff > (int16_t)TOUCH_DIAG_BALANCE_DIFF_MAX_LSB)
    {
        if (diag->r525_open_cnt < TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            diag->r525_open_cnt++;
        }
        if (diag->r525_open_cnt >= TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            raw_faults |= TOUCH_FAULT_RESISTOR_OPEN;
        }
    }
    else
    {
        diag->r525_open_cnt = 0U;
    }

    /* ---------------------------------------------------------------------------------------------
     * Diagnostic 4: Reference Voltage VREFH Drift Monitoring (4750 mV ~ 5250 mV, +/- 5%)
     * --------------------------------------------------------------------------------------------- */
    if ((vrefh_mv < TOUCH_DIAG_VREFH_MIN_MV) || (vrefh_mv > TOUCH_DIAG_VREFH_MAX_MV))
    {
        if (diag->vref_drift_cnt < TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            diag->vref_drift_cnt++;
        }
        if (diag->vref_drift_cnt >= TOUCH_DIAG_FAULT_SET_CYCLES)
        {
            raw_faults |= TOUCH_FAULT_VREF_DRIFT;
        }
    }
    else
    {
        diag->vref_drift_cnt = 0U;
    }

    /* ---------------------------------------------------------------------------------------------
     * Fault Qualification, Latched History & Self-Healing Auto-Recovery State Machine
     * --------------------------------------------------------------------------------------------- */
    if (raw_faults != TOUCH_FAULT_NONE)
    {
        if (diag->active_faults == TOUCH_FAULT_NONE)
        {
            diag->total_fault_events++;
        }
        diag->active_faults     = raw_faults;
        diag->latched_faults   |= raw_faults;
        diag->is_healthy        = FALSE;
        diag->healthy_cycles_cnt = 0U;
    }
    else
    {
        /* All physical parameters are within healthy window */
        if (diag->active_faults != TOUCH_FAULT_NONE)
        {
            diag->healthy_cycles_cnt++;
            if (diag->healthy_cycles_cnt >= TOUCH_DIAG_FAULT_CLEAR_CYCLES)
            {
                /* Self-healing auto-recovery: cleared after 8 consecutive healthy cycles (120 ms) */
                diag->active_faults      = TOUCH_FAULT_NONE;
                diag->is_healthy         = TRUE;
                diag->healthy_cycles_cnt = 0U;
                diag->total_recoveries++;
            }
        }
        else
        {
            diag->is_healthy         = TRUE;
            diag->healthy_cycles_cnt = 0U;
        }
    }

    return diag->active_faults;
}

boolean Touch_Diag_IsHealthy(const Touch_Diagnostics_t *diag)
{
    if (diag == NULL_PTR)
    {
        return FALSE;
    }
    return diag->is_healthy;
}

uint32_t Touch_Diag_GetFaults(const Touch_Diagnostics_t *diag)
{
    if (diag == NULL_PTR)
    {
        return TOUCH_FAULT_NONE;
    }
    return diag->active_faults;
}

void Touch_Diag_ResetLatched(Touch_Diagnostics_t *diag)
{
    if (diag != NULL_PTR)
    {
        diag->latched_faults = TOUCH_FAULT_NONE;
    }
}
