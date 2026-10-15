/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : TinyML Envelope Classifier & Water Droplet Rejection Engine (M4)
*   Dependencies         : StandardTypes.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
==================================================================================================*/

#ifndef TOUCH_TINYML_H
#define TOUCH_TINYML_H

#ifdef __cplusplus
extern "C" {
#endif

#include "StandardTypes.h"
#include <stdint.h>

/*==================================================================================================
*                                      DEFINES AND CONSTANTS
==================================================================================================*/

/* Sliding window length (10 cycles @ 15 ms = 150 ms temporal envelope window) */
#define TOUCH_TINYML_WINDOW_SIZE        (10U)

/* 3-Step confidence hysteresis sliding window length (Pyeon et al., Sensors 2023) */
#define TOUCH_TINYML_CONF_WINDOW        (3U)
#define TOUCH_TINYML_CONF_WINDOW_SIZE   TOUCH_TINYML_CONF_WINDOW

/* Dynamic Threshold Adjustment (DTA) bounds (TI SLAY045B) */
#define TOUCH_DTA_GLOVE_MIN_LSB         (34.0f)   /* Lower DTA bound in quiet environment for 2mm acrylic/glove */
#define TOUCH_DTA_NOMINAL_LSB           (48.0f)   /* Nominal touch threshold */
#define TOUCH_DTA_NOISY_MAX_LSB         (58.0f)   /* Elevated DTA bound under high spread / wet mist */
#define TOUCH_DTA_HYSTERESIS_RATIO      (0.78f)   /* Release hysteresis ratio */

/* Feature decision boundaries tuned to S32K144W SW7 empirical physics */
#define TOUCH_FEAT_MIN_ATTACK_SLOPE     (8.5f)    /* Minimum 2-cycle attack slope (LSB) to reject slow water film */
#define TOUCH_FEAT_MIN_DYNAMIC_SNR      (14.0f)   /* Minimum Dynamic SNR to reject wet mist / EMI noise */
#define TOUCH_FEAT_MAX_WATER_SPREAD     (3.2f)    /* Maximum burst spread (LSB) allowed for valid touch */
#define TOUCH_FEAT_MAX_CURVATURE_RATIO  (1.35f)   /* Maximum normalized 2nd-diff curvature to reject splash/graze */
#define TOUCH_FEAT_MIN_SUSTAIN_RATIO    (0.68f)   /* Minimum envelope sustain ratio (delta / window_peak) */

/* 3-Step confidence hysteresis thresholds [0.0 ~ 1.0] */
#define TOUCH_CONF_TRIGGER_THRESH       (0.64f)   /* Trigger valid touch when 3-step mean confidence >= 0.64 */
#define TOUCH_CONF_RELEASE_THRESH       (0.38f)   /* Release touch when 3-step mean confidence < 0.38 */

/* Legacy threshold macros for backward compatibility */
#define TOUCH_CONF_TRIGGER_THRESHOLD    TOUCH_CONF_TRIGGER_THRESH
#define TOUCH_CONF_RELEASE_THRESHOLD    TOUCH_CONF_RELEASE_THRESH
#define TOUCH_MIN_GLOVE_DELTA_LSB       TOUCH_DTA_GLOVE_MIN_LSB
#define TOUCH_MIN_BARE_DELTA_LSB        (78.0f)

/*==================================================================================================
*                                STRUCTURES AND DATA TYPES
==================================================================================================*/

/*!
 * @brief Milestone 4 TinyML Classification Output Category
 */
typedef enum {
    TOUCH_CLASS_IDLE = 0U,          /*!< Untouched idle state */
    TOUCH_CLASS_VALID_BARE,         /*!< Valid bare finger press (high amplitude, clean SNR) */
    TOUCH_CLASS_VALID_GLOVE,        /*!< Valid thin glove / 2mm acrylic press (moderate amplitude, clean SNR) */
    TOUCH_CLASS_REJECT_WATER,       /*!< Rejected water droplet drip / mist spray anomaly */
    TOUCH_CLASS_REJECT_SWIPE        /*!< Rejected fast edge graze / single-cycle transient */
} Touch_TinyML_Class_t;

/* Backward compatibility enum aliases */
typedef Touch_TinyML_Class_t Touch_ClassificationClass_t;
#define TOUCH_CLASS_NONE          TOUCH_CLASS_IDLE
#define TOUCH_CLASS_BARE_FINGER   TOUCH_CLASS_VALID_BARE
#define TOUCH_CLASS_GLOVE_OVERLAY TOUCH_CLASS_VALID_GLOVE
#define TOUCH_CLASS_WATER_DROPLET TOUCH_CLASS_REJECT_WATER
#define TOUCH_CLASS_EDGE_HOVER    TOUCH_CLASS_REJECT_SWIPE

/*!
 * @brief 4 Time-Domain Envelope Features Extracted Over the Sliding Window
 */
typedef struct {
    float attack_slope;      /*!< Feature 1: Max 2-cycle rising slope in window (LSB / 2-cycles) */
    float dynamic_snr;       /*!< Feature 2: Peak-to-local-spread dynamic SNR ratio */
    float plateau_variance;  /*!< Feature 3: Normalized 2nd-difference envelope curvature variance */
    float release_symmetry;  /*!< Feature 4: Envelope sustain ratio (current delta / window max) */
    float dta_threshold;     /*!< Instantaneous Dynamic Threshold Adjustment (DTA) level (LSB) */
} Touch_TinyML_Features_t;

/*!
 * @brief Milestone 4 TinyML Classifier Context & Telemetry Structure
 */
typedef struct {
    float                   delta_win[TOUCH_TINYML_WINDOW_SIZE];  /*!< Sliding window of net delta (LSB) */
    float                   spread_win[TOUCH_TINYML_WINDOW_SIZE]; /*!< Sliding window of burst spread (LSB) */
    float                   conf_win[TOUCH_TINYML_CONF_WINDOW];   /*!< 3-step instantaneous confidence buffer */
    uint8_t                 win_head;                             /*!< Circular buffer write index */
    uint8_t                 sample_count;                         /*!< Number of valid samples in window */
    uint8_t                 active_sustain_cycles;                /*!< Consecutive cycles above DTA threshold */
    Touch_TinyML_Features_t features;                             /*!< Latest extracted 4 envelope features */
    float                   raw_confidence;                       /*!< Instantaneous frame confidence P_k [0..1] */
    float                   smoothed_confidence;                  /*!< 3-step moving average confidence [0..1] */
    Touch_TinyML_Class_t    predicted_class;                      /*!< Current classified physical event */
    boolean                 is_valid_touch;                       /*!< Final debounced touch decision output */
    float                   confidence;                           /*!< Backward-compatibility alias for smoothed_confidence */
} Touch_TinyML_Classifier_t;

/* Backward compatibility struct alias */
typedef Touch_TinyML_Classifier_t Touch_TinyMLClassifier_t;

/*==================================================================================================
*                                FUNCTION PROTOTYPES
==================================================================================================*/

/*!
 * @brief Initialize the Milestone 4 TinyML Envelope Feature Extractor & Classifier
 * @param[out] clf Pointer to classifier instance
 */
void Touch_TinyML_Init(Touch_TinyML_Classifier_t *clf);

/*!
 * @brief Process one scan cycle through the 10-point feature extractor and 3-step confidence classifier
 * @param[in,out] clf Pointer to classifier instance
 * @param[in]     delta Current baseline-compensated net shift (x_est - baseline) in LSB
 * @param[in]     spread Current burst spread metric from M2 Alpha-Trimmed Mean in LSB
 * @param[in]     spike_rejected TRUE if M2 detected an EFT/ESD impulse spike in the burst
 * @return TRUE if a valid human touch (bare finger or glove/acrylic) is confirmed, FALSE otherwise
 */
boolean Touch_TinyML_Update(Touch_TinyML_Classifier_t *clf, float delta, float spread, boolean spike_rejected);

/*!
 * @brief Query the latest classified category (Idle, Bare Finger, Glove/Acrylic, Water Reject, Swipe Reject)
 * @param[in] clf Pointer to classifier instance
 * @return Current Touch_TinyML_Class_t enum value
 */
Touch_TinyML_Class_t Touch_TinyML_GetClass(const Touch_TinyML_Classifier_t *clf);

/*!
 * @brief Query whether current state is a confirmed, debounced touch
 * @param[in] clf Pointer to classifier instance
 * @return TRUE if valid touch (bare finger or glove), FALSE otherwise
 */
boolean Touch_TinyML_IsValidTouch(const Touch_TinyML_Classifier_t *clf);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_TINYML_H */
