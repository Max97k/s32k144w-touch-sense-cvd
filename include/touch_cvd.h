/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : ADC0, PORTC, GPIO
*   Dependencies         : NXP RTD 3.0.0
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
*   SPDX-License-Identifier: BSD-3-Clause
==================================================================================================*/

#ifndef TOUCH_CVD_H
#define TOUCH_CVD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "StandardTypes.h"
#include "S32K144W.h"

/*==================================================================================================
*                                      DEFINES AND CONSTANTS
* Hardware configuration based on XS32K14WEVB-Q064 / S32K144EVB-Q100:
*   TOUCH_A: PTC17 (Pin 27) -> Driving pad, cross-resistor R525 (4.7k), ADC0_SE15
*   TOUCH_B: PTC16 (Pin 28) -> Sensing pad SW7, reference capacitor C533 (10pF), ADC0_SE14
==================================================================================================*/
#define TOUCH_PORT_BASE           IP_PORTC
#define TOUCH_GPIO_BASE           IP_PTC

#define TOUCH_PIN_DRV_A           (17U)   /* PTC17 / ADC0_SE15 / TOUCH_A */
#define TOUCH_PIN_SENSE_B         (16U)   /* PTC16 / ADC0_SE14 / TOUCH_B */

#define TOUCH_ADC0_CH_SENSE_B     (14U)   /* ADC0_SE14 */
#define TOUCH_ADC0_CH_DRV_A       (15U)   /* ADC0_SE15 (for balance verification) */
#define TOUCH_ADC0_CH_BANDGAP     (27U)   /* Internal 1.0V Bandgap reference */

/* ADC Status and Control 3 Calibration Failed Flag (Bit 6, per S32K1xx RM & AN12217) */
#ifndef ADC_SC3_CALF_MASK
#define ADC_SC3_CALF_MASK         (0x40U)
#endif

/* M1 Quantitative Pass Criteria Thresholds (from milestone plan) */
#define M1_UNTOUCHED_MIN_LSB      (1600)  /* 0.4 * VDD (12-bit = 4095) */
#define M1_UNTOUCHED_MAX_LSB      (2500)  /* 0.6 * VDD */
#define M1_BALANCE_DIFF_MAX_LSB   (15)    /* Balanced壓差 < 15 LSB */
#define M1_TOUCH_DELTA_MIN_LSB    (80)    /* Hand press delta shift >= 80 LSB */
#define M1_TOUCH_DELTA_MAX_LSB    (200)   /* Hand press delta shift <= 200 LSB */

/* LED Indicator definitions on XS32K14WEVB-Q064 */
#define LED_RED_PORT              IP_PORTE
#define LED_RED_GPIO              IP_PTE
#define LED_RED_PIN               (7U)

#define LED_GREEN_PORT            IP_PORTE
#define LED_GREEN_GPIO            IP_PTE
#define LED_GREEN_PIN             (0U)

#define LED_BLUE_PORT             IP_PORTE
#define LED_BLUE_GPIO             IP_PTE
#define LED_BLUE_PIN              (3U)

/*==================================================================================================
*                                      STRUCTURES AND TYPEDEFS
==================================================================================================*/
typedef struct {
    uint16_t sample_A;          /* Scan A raw reading on ADC0_SE14 (LSB) */
    uint16_t sample_B;          /* Scan B raw reading on ADC0_SE14 (LSB) */
    int16_t  delta_D;           /* Differential synthesis: sample_B - sample_A */
    uint16_t sample_A_se15;     /* Scan A SE15 voltage for NOP balance check */
    int16_t  balance_diff;      /* |sample_A - sample_A_se15| (LSB) */
} Touch_RawSample_t;

typedef struct {
    boolean  adc_calib_pass;
    uint16_t bandgap_raw;
    uint32_t vrefh_mv;
    uint8_t  optimal_nop;
    int16_t  min_balance_diff;    /* Optimal minimum balance diff across NOP sweep */
    int16_t  nop_balance_diff[4]; /* Balance error for NOP = 4, 8, 12, 16 */
    uint16_t baseline_sample_A;
    uint16_t baseline_sample_B;
    int16_t  baseline_delta;
    int16_t  touch_delta_shift;
    boolean  criteria_1_workpoint_pass; /* 1600 ~ 2500 LSB */
    boolean  criteria_2_balance_pass;   /* < 15 LSB */
    boolean  criteria_3_gain_pass;      /* 80 ~ 200 LSB */
    boolean  overall_m1_pass;
} Touch_M1_Report_t;

/*==================================================================================================
*                                      FUNCTION PROTOTYPES
==================================================================================================*/

/*!
 * @brief Initialize Touch sensing GPIO pins, ADC0 hardware, and status indicators.
 */
void Touch_Init(void);

/*!
 * @brief Execute ADC0 32x hardware averaging self-calibration per NXP AN12217.
 * @return TRUE if calibration passed, FALSE if CALF raised.
 */
boolean Touch_Adc0_Calibrate(void);

/*!
 * @brief Read internal 1.0V Bandgap (Channel 27) and compute VREFH in millivolts.
 * @return Estimated VREFH voltage in mV (nominal ~5000 mV or ~3300 mV).
 */
uint32_t Touch_ReadBandgap_mV(uint16_t *out_raw_bandgap);

/*!
 * @brief Core 4-phase bipolar differential CVD acquisition.
 * Executes:
 *   Phase 1: Scan A (PTC17=0V, PTC16=VDD -> High-Z -> NOP wait -> ADC0_SE14 read)
 *   Phase 2: Scan B (PTC17=VDD, PTC16=0V -> High-Z -> NOP wait -> ADC0_SE14 read)
 *   Phase 3: Idle Grounding (PTC17=0V, PTC16=0V low-impedance discharge)
 *   Phase 4: Synthesis delta_D = sample_B - sample_A
 *
 * @param[in] nop_balance_count Number of CPU NOP cycles for charge equilibration across R525.
 * @return Raw differential sample structure.
 */
Touch_RawSample_t Touch_AcquireSingleDiffCVD(uint8_t nop_balance_count);

/*!
 * @brief Measure charge balancing voltage difference across R525 (SE14 vs SE15).
 * Used during NOP calibration scan.
 */
int16_t Touch_MeasureBalanceDiff(uint8_t nop_count);

/*!
 * @brief Perform NOP sweep across 4, 8, 12, 16 NOPs to find shortest delay satisfying < 15 LSB.
 */
void Touch_M1_NOP_Sweep(Touch_M1_Report_t *report);

/*!
 * @brief Update RGB LED status according to M1 state.
 */
void Touch_SetStatusLeds(boolean red_on, boolean green_on, boolean blue_on);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_CVD_H */
