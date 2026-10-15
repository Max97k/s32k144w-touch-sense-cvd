/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Integrated Touch Engine, VLPS Low-Power & LPIT Periodic Wakeup
*   Dependencies         : touch_cvd.h, touch_filter.h, touch_baseline.h, touch_tinyml.h, touch_diag.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
==================================================================================================*/

#ifndef TOUCH_SENSE_H
#define TOUCH_SENSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "StandardTypes.h"
#include "touch_cvd.h"
#include "touch_filter.h"
#include "touch_baseline.h"
#include "touch_tinyml.h"
#include "touch_diag.h"
#include <stdint.h>

/*==================================================================================================
*                                      DEFINES AND CONSTANTS
*==================================================================================================*/

/* Low-Power & Fast Execution Timing Constraints (Milestone 6) */
#define TOUCH_MAX_EXEC_TIME_US          (60U)     /*!< Max allowable execution time per cycle (Target <= 60 us) */
#define TOUCH_LPIT_WAKEUP_PERIOD_MS     (25U)     /*!< Low-power standby inspection wakeup interval */
#define TOUCH_RUN_FRAME_PERIOD_MS       (15U)     /*!< High-rate verified sensing frame period */

/* Gear Shift Thresholds */
#define TOUCH_GEAR_WAKE_DELTA_LSB       (18.0f)   /*!< Sensed net shift to immediately gear-shift into RUN mode */
#define TOUCH_GEAR_IDLE_CYCLES_BEFORE_SLEEP (20U) /*!< Consecutive idle cycles before dropping into VLPS standby */

/* Standby Current Limits (Reference at 25 deg C) */
#define TOUCH_STANDBY_CURRENT_MIN_UA    (45U)     /*!< Pass criteria lower bound */
#define TOUCH_STANDBY_CURRENT_MAX_UA    (70U)     /*!< Pass criteria upper bound */

/*==================================================================================================
*                                STRUCTURES AND DATA TYPES
*==================================================================================================*/

/*!
 * @brief System Power Operation Modes
 */
typedef enum {
    TOUCH_POWER_RUN_HIGH_RATE = 0U,   /*!< High-rate active scan (15 ms frame rate, 66.7 Hz) */
    TOUCH_POWER_VLPS_LOW_RATE = 1U    /*!< Ultra-low power standby (25 ms LPIT wake, VLPS sleep) */
} Touch_PowerMode_t;

/*!
 * @brief High-Level Touch Sensing Output Result
 */
typedef struct {
    boolean                     is_touched;        /*!< Debounced, verified touch flag */
    Touch_ClassificationClass_t detected_class;    /*!< Classification result (Bare, Glove, Water, Hover) */
    float                       dynamic_delta;     /*!< Dynamic signal shift relative to baseline (LSB) */
    float                       x_est;             /*!< Filtered differential signal (LSB) */
    float                       baseline;          /*!< Current dynamic baseline reference (LSB) */
    float                       confidence;        /*!< Multi-frame sliding window confidence */
    boolean                     is_healthy;        /*!< Hardware diagnostic status (TRUE = No active defect) */
    uint32_t                    active_faults;     /*!< Active hardware fault bitmask */
    uint32_t                    exec_time_us;      /*!< Execution time of current cycle in microseconds */
    Touch_PowerMode_t           recommended_mode;  /*!< Recommended next power mode (RUN or VLPS) */
} Touch_SenseResult_t;

/*!
 * @brief Unified Touch Sensing Engine Context Structure
 */
typedef struct {
    Touch_M1_Report_t           m1_report;
    Touch_Kalman_t              kalman;
    Touch_BaselineTracker_t     baseline_tracker;
    Touch_TinyMLClassifier_t    tinyml;
    Touch_Diagnostics_t         diag;
    uint16_t                    lfsr_state;
    uint32_t                    cycle_count;
    uint32_t                    burnin_cycles;
    uint32_t                    burnin_false_positives;
    uint32_t                    idle_cycles_count;
    Touch_PowerMode_t           current_power_mode;
    uint32_t                    last_exec_time_ticks;
    uint32_t                    max_exec_time_us;
} Touch_SenseEngine_t;

/*==================================================================================================
*                                FUNCTION PROTOTYPES
*==================================================================================================*/

/*!
 * @brief Initialize all subsystems of the unified Touch Sensing Engine
 * @param[out] engine Pointer to engine instance
 * @return TRUE if all calibrations and checks pass, FALSE otherwise
 */
boolean Touch_Sense_Init(Touch_SenseEngine_t *engine);

/*!
 * @brief Execute one synchronous scan, filter, baseline, classify and diagnostic cycle
 * @param[in,out] engine Pointer to engine instance
 * @param[out]    result Pointer to output result structure
 */
void Touch_Sense_ProcessCycle(Touch_SenseEngine_t *engine, Touch_SenseResult_t *result);

/*!
 * @brief Initialize LPIT and SMC registers for Very-Low-Power Stop (VLPS) mode
 */
void Touch_Sense_LowPower_Init(void);

/*!
 * @brief Safely transition MCU into VLPS mode awaiting periodic LPIT wakeup
 */
void Touch_Sense_EnterVLPS(void);

/*!
 * @brief Query if hardware is fully healthy and ready
 * @param[in] engine Pointer to engine instance
 * @return TRUE if healthy, FALSE if any ASIL-B fault is active
 */
boolean Touch_Sense_IsHealthy(const Touch_SenseEngine_t *engine);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_SENSE_H */
