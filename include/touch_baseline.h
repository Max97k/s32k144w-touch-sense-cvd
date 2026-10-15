/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Baseline Tracking Engine (4-Zone Asymmetric DC Tracker)
*   Dependencies         : StandardTypes.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
==================================================================================================*/

#ifndef TOUCH_BASELINE_H
#define TOUCH_BASELINE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "StandardTypes.h"
#include <stdint.h>

/*==================================================================================================
*                                      DEFINES AND CONSTANTS
==================================================================================================*/

/* Nominal scan cycle interval (ms) */
#define TOUCH_CYCLE_INTERVAL_MS         (15U)

/* Touch detection & baseline tracking thresholds (LSB) */
#define TOUCH_THRESH_TOUCH_LSB          (50.0f)   /* Touch detection threshold (TH) */
#define TOUCH_THRESH_RELEASE_LSB        (40.0f)   /* Touch release threshold (TH - Hysteresis) */
#define TOUCH_APPROACH_RATIO            (0.40f)   /* Approach freeze ratio (0.4 * TH = 20 LSB) */
#define TOUCH_THRESH_NEG_RESET_LSB      (-15.0f)  /* Negative anomaly threshold */

/* Baseline slew rate limits for environmental thermal/humidity drift */
#define TOUCH_SLEW_RATE_POS_LSB_S       (1.5f)    /* Max positive drift tracking rate: 1.5 LSB / sec */
#define TOUCH_SLEW_RATE_NEG_LSB_S       (2.0f)    /* Max negative drift tracking rate: 2.0 LSB / sec */

/* Timeout and debounce intervals */
#define TOUCH_NEG_RESET_DURATION_MS     (300U)    /* Fast negative reset debounce time (<= 300 ms) */
#define TOUCH_HOLD_TIMEOUT_SEC          (15.0f)   /* Maximum continuous touch hold timeout (15.0 sec) */

/* Debounce cycles derived from interval */
#define TOUCH_NEG_RESET_CYCLES          (TOUCH_NEG_RESET_DURATION_MS / TOUCH_CYCLE_INTERVAL_MS) /* ~20 cycles */
#define TOUCH_HOLD_TIMEOUT_CYCLES       ((uint32_t)(TOUCH_HOLD_TIMEOUT_SEC * 1000.0f / (float)TOUCH_CYCLE_INTERVAL_MS)) /* ~1000 cycles */

/*==================================================================================================
*                                STRUCTURES AND DATA TYPES
==================================================================================================*/

/*!
 * @brief 4-Zone Baseline Tracking State Enum
 */
typedef enum {
    TOUCH_ZONE_IDLE_DRIFT = 0U,     /*!< Zone 1: Ambient slow thermal/humidity drift (Slew-rate limited) */
    TOUCH_ZONE_APPROACH_FREEZE,     /*!< Zone 2: Finger approaching (0.4*TH <= delta < TH) -> Baseline FROZEN */
    TOUCH_ZONE_TOUCH_ACTIVE,        /*!< Zone 3: Active touch detected (delta >= TH) -> Baseline FROZEN */
    TOUCH_ZONE_HOLD_TIMEOUT,        /*!< Zone 3-Timeout: Continuous press > 15s -> Force Baseline Reset */
    TOUCH_ZONE_NEG_RECOVERY         /*!< Zone 4: Touch-on-boot negative shift (delta < -15 LSB) -> Fast Drop */
} Touch_BaselineZone_t;

/*!
 * @brief Baseline Tracker Context Structure
 */
typedef struct {
    float                baseline;           /*!< Current tracking baseline estimate (LSB) */
    float                threshold_touch;    /*!< Touch trigger threshold (LSB) */
    float                threshold_release;  /*!< Touch release threshold (LSB) */
    float                threshold_prepare;  /*!< Approach freeze threshold (0.4 * TH) */
    float                threshold_neg;      /*!< Negative self-healing threshold */
    float                delta;              /*!< Net signal shift: x_est - baseline (LSB) */
    boolean              is_touched;         /*!< Filtered hysteresis touch state */
    Touch_BaselineZone_t active_zone;        /*!< Current operational zone */
    uint32_t             hold_cycle_count;   /*!< Continuous touch hold cycle counter */
    uint32_t             neg_cycle_count;    /*!< Negative anomaly persistence cycle counter */
    uint32_t             total_updates;      /*!< Total updates processed */
    boolean              force_reset_flag;   /*!< Set TRUE when a timeout or fast reset occurred */
} Touch_BaselineTracker_t;

/*==================================================================================================
*                                FUNCTION PROTOTYPES
==================================================================================================*/

/*!
 * @brief Initialize the Asymmetric Baseline Tracker
 * @param[out] tracker Pointer to tracker instance
 * @param[in]  initial_baseline Starting baseline value (from M1 untouched calibration)
 * @param[in]  touch_threshold Touch detection threshold TH (default 50.0 LSB)
 */
void Touch_Baseline_Init(Touch_BaselineTracker_t *tracker, float initial_baseline, float touch_threshold);

/*!
 * @brief Execute one cycle of 4-Zone Asymmetric Baseline Tracking
 * @param[in,out] tracker Pointer to tracker instance
 * @param[in]     x_est Current filtered signal from Adaptive Kalman filter
 * @param[in]     delta_time_ms Elapsed time since last update in milliseconds
 * @return Current net delta shift (x_est - baseline)
 */
float Touch_Baseline_Update(Touch_BaselineTracker_t *tracker, float x_est, uint32_t delta_time_ms);

/*!
 * @brief Force baseline to immediately align with current raw reading (e.g. after hard reset)
 * @param[in,out] tracker Pointer to tracker instance
 * @param[in]     new_baseline Baseline target value
 */
void Touch_Baseline_ForceAlign(Touch_BaselineTracker_t *tracker, float new_baseline);

/*!
 * @brief Query current touch state with hysteresis
 * @param[in] tracker Pointer to tracker instance
 * @return TRUE if button is touched, FALSE if released
 */
boolean Touch_Baseline_IsTouched(const Touch_BaselineTracker_t *tracker);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_BASELINE_H */
