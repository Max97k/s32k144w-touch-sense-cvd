/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Hardware Plausibility Diagnostics & Defensive Safety Engine
*   Dependencies         : StandardTypes.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
*   SPDX-License-Identifier: BSD-3-Clause
==================================================================================================*/

#ifndef TOUCH_DIAG_H
#define TOUCH_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "StandardTypes.h"
#include <stdint.h>

/*==================================================================================================
*                                      DEFINES AND CONSTANTS
*==================================================================================================*/

/* Voltage physical window limits (12-bit ADC, full scale 0 ~ 4095 LSB) */
#define TOUCH_DIAG_SAMPLE_MIN_LSB       (300U)    /*!< Lower limit: readings < 300 LSB indicate short to GND */
#define TOUCH_DIAG_SAMPLE_MAX_LSB       (3800U)   /*!< Upper limit: readings > 3800 LSB indicate short to VDD */

/* Bridging resistor R525 (10 kOhm) integrity threshold:
 * Healthy charge transfer across R525 yields balance difference ~83 LSB (< 120 LSB).
 * Resistor degradation or open-circuit prevents charge equalization (|diff| > 250 LSB up to 4095 LSB). */
#define TOUCH_DIAG_BALANCE_DIFF_MAX_LSB (250)

/* Reference voltage VREFH drift limits (Nominal 5000 mV, +/- 5% tolerance) */
#define TOUCH_DIAG_VREFH_NOMINAL_MV     (5000U)   /*!< Target EVB supply voltage */
#define TOUCH_DIAG_VREFH_MIN_MV         (4750U)   /*!< 5000 mV - 5% (Under-voltage threshold) */
#define TOUCH_DIAG_VREFH_MAX_MV         (5250U)   /*!< 5000 mV + 5% (Over-voltage threshold) */

/* Fault qualification debounce cycles (at 15 ms/cycle scan rate) */
#define TOUCH_DIAG_FAULT_SET_CYCLES     (2U)      /*!< 2 cycles = 30 ms reaction time (Target <= 3 cycles / 60 ms) */
#define TOUCH_DIAG_FAULT_CLEAR_CYCLES   (8U)      /*!< 8 cycles = 120 ms auto-recovery time (Target <= 200 ms) */

/*==================================================================================================
*                                  FAULT CODES & BITMASKS
*==================================================================================================*/

#define TOUCH_FAULT_NONE                (0x00000000U) /*!< No fault: system is fully operational and safe */
#define TOUCH_FAULT_SHORT_GND_A         (0x00000001U) /*!< Electrode TOUCH_A (SW7 pad) shorted to GND */
#define TOUCH_FAULT_SHORT_GND_B         (0x00000002U) /*!< Reference TOUCH_B (C533) shorted to GND */
#define TOUCH_FAULT_SHORT_VDD_A         (0x00000004U) /*!< Electrode TOUCH_A (SW7 pad) shorted to VDD */
#define TOUCH_FAULT_SHORT_VDD_B         (0x00000008U) /*!< Reference TOUCH_B (C533) shorted to VDD */
#define TOUCH_FAULT_RESISTOR_OPEN       (0x00000010U) /*!< Bridging resistor R525 (10k) degraded or open-circuit */
#define TOUCH_FAULT_VREF_DRIFT          (0x00000020U) /*!< ADC VREFH supply drifted outside +/- 5% window */
#define TOUCH_FAULT_ELECTRODE_OPEN      (0x00000040U) /*!< Physical electrode pad detached or floating */

/*==================================================================================================
*                                STRUCTURES AND DATA TYPES
*==================================================================================================*/

/*!
 * @brief On-line Diagnostic Engine Context Structure
 */
typedef struct {
    uint32_t active_faults;             /*!< Bitmask of currently active, qualified faults */
    uint32_t latched_faults;            /*!< Cumulative latched faults since system boot */
    boolean  is_healthy;                /*!< Overall health status (TRUE = No active faults) */

    /* Debounce counters for fault qualification */
    uint8_t  short_gnd_a_cnt;
    uint8_t  short_gnd_b_cnt;
    uint8_t  short_vdd_a_cnt;
    uint8_t  short_vdd_b_cnt;
    uint8_t  r525_open_cnt;
    uint8_t  vref_drift_cnt;
    uint8_t  healthy_cycles_cnt;

    /* Monitored telemetry metrics */
    uint16_t last_sample_A;
    uint16_t last_sample_B;
    int16_t  last_balance_diff;
    uint32_t last_vrefh_mv;

    /* ISO 26262 Diagnostic Coverage Statistics */
    uint32_t total_fault_events;        /*!< Total fault occurrences detected */
    uint32_t total_recoveries;          /*!< Total self-healing recoveries completed */
} Touch_Diagnostics_t;

/*==================================================================================================
*                                FUNCTION PROTOTYPES
*==================================================================================================*/

/*!
 * @brief Initialize the On-line Hardware Diagnostics Engine
 * @param[out] diag Pointer to diagnostics instance
 */
void Touch_Diag_Init(Touch_Diagnostics_t *diag);

/*!
 * @brief Run one on-line diagnostic cycle evaluating physical bounds, R525 integrity, and VREFH
 * @param[in,out] diag Pointer to diagnostics instance
 * @param[in]     sample_A PTC17 ADC raw reading (SE15)
 * @param[in]     sample_B PTC16 ADC raw reading (SE14)
 * @param[in]     balance_diff Measured charge balance difference across R525
 * @param[in]     vrefh_mv Estimated VREFH supply voltage in millivolts (from Bandgap Ch27)
 * @return Bitmask of currently active qualified faults (0 = All healthy)
 */
uint32_t Touch_Diag_RunCycle(Touch_Diagnostics_t *diag,
                             uint16_t sample_A,
                             uint16_t sample_B,
                             int16_t balance_diff,
                             uint32_t vrefh_mv);

/*!
 * @brief Query whether the touch sensor hardware is healthy and safe to operate
 * @param[in] diag Pointer to diagnostics instance
 * @return TRUE if healthy (no active faults), FALSE if any fault is active
 */
boolean Touch_Diag_IsHealthy(const Touch_Diagnostics_t *diag);

/*!
 * @brief Get the currently active qualified fault bitmask
 * @param[in] diag Pointer to diagnostics instance
 * @return Bitmask of active faults
 */
uint32_t Touch_Diag_GetFaults(const Touch_Diagnostics_t *diag);

/*!
 * @brief Reset latched faults history (e.g. upon ECU diagnostic command)
 * @param[in,out] diag Pointer to diagnostics instance
 */
void Touch_Diag_ResetLatched(Touch_Diagnostics_t *diag);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_DIAG_H */
