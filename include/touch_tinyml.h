/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Temporal Feature Envelope & TinyML Classifier Engine
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

/* History sliding window depth for temporal geometric feature extraction */
#define TOUCH_TINYML_WINDOW_SIZE        (10U)   /* 10 cycles * 15 ms = 150 ms history */

/* Confidence sliding window depth for multi-cycle hysteresis smoothing */
#define TOUCH_TINYML_CONF_WINDOW_SIZE   (3U)    /* 3 cycles = 45 ms latency, well within 60~90 ms */

/* Classification Confidence Thresholds */
#define TOUCH_CONF_TRIGGER_THRESHOLD    (0.65f) /* Confidence required to trigger touch */
#define TOUCH_CONF_RELEASE_THRESHOLD    (0.35f) /* Confidence required to release touch */

/* Geometric feature decision thresholds */
#define TOUCH_MIN_BARE_DELTA_LSB        (80.0f) /* Minimum delta for bare finger */
#define TOUCH_MIN_GLOVE_DELTA_LSB       (35.0f) /* Minimum delta for glove / 2mm acrylic */
#define TOUCH_WATER_MAX_DELTA_LSB       (45.0f) /* Water droplets typically stay below 45 LSB */
#define TOUCH_MIN_ATTACK_SLOPE          (15.0f) /* Minimum attack slope (LSB/cycle) for genuine touch */
#define TOUCH_MAX_PLATEAU_VAR_LSB2      (9.0f)  /* Maximum plateau variance for stable finger contact */

/*==================================================================================================
*                                STRUCTURES AND DATA TYPES
==================================================================================================*/

/*!
 * @brief TinyML Classification Output Class Enum
 */
typedef enum {
    TOUCH_CLASS_NONE = 0U,          /*!< Untouched / Idle quiescent state */
    TOUCH_CLASS_BARE_FINGER,        /*!< Verified bare finger touch */
    TOUCH_CLASS_GLOVE_OVERLAY,      /*!< Verified glove / 2mm acrylic overlay touch */
    TOUCH_CLASS_WATER_DROPLET,      /*!< Water droplet / spray condensation disturbance (REJECTED) */
    TOUCH_CLASS_EDGE_HOVER          /*!< Transient edge hover / grazing swipe (REJECTED) */
} Touch_ClassificationClass_t;

/*!
 * @brief Extracted Temporal Envelope Features
 */
typedef struct {
    float attack_slope;             /*!< Rising edge attack slope (LSB / cycle) */
    float dynamic_snr_db;           /*!< Dynamic signal-to-noise ratio (dB) */
    float plateau_variance;         /*!< Variance during held contact plateau (LSB^2) */
    float release_symmetry;         /*!< Symmetry ratio of release slope to attack slope */
    float peak_delta;               /*!< Maximum delta in current window (LSB) */
    uint8_t contact_duration;       /*!< Consecutive cycles above approach threshold */
} Touch_EnvelopeFeatures_t;

/*!
 * @brief TinyML Classifier Context Structure
 */
typedef struct {
    float                       history_delta[TOUCH_TINYML_WINDOW_SIZE]; /* Circular delta buffer */
    float                       history_spread[TOUCH_TINYML_WINDOW_SIZE];/* Circular spread buffer */
    uint8_t                     head_idx;                                /* Circular buffer head */
    uint8_t                     sample_count;                            /* Populated samples count */

    float                       conf_window[TOUCH_TINYML_CONF_WINDOW_SIZE]; /* Probability history */
    uint8_t                     conf_head;                                  /* Confidence head */

    Touch_EnvelopeFeatures_t    features;                                /* Latest extracted features */
    Touch_ClassificationClass_t detected_class;                          /* Latest evaluated class */
    float                       confidence;                              /* Smoothed confidence [0.0 ~ 1.0] */
    boolean                     is_valid_touch;                          /* Final filtered touch decision */

    uint32_t                    total_classifications;                   /* Total evaluations count */
    uint32_t                    water_rejections_count;                  /* Total water droplets rejected */
    uint32_t                    hover_rejections_count;                  /* Total edge hovers rejected */
} Touch_TinyMLClassifier_t;

/*==================================================================================================
*                                FUNCTION PROTOTYPES
==================================================================================================*/

/*!
 * @brief Initialize the TinyML Classifier and envelope buffers
 * @param[out] classifier Pointer to classifier instance
 */
void Touch_TinyML_Init(Touch_TinyMLClassifier_t *classifier);

/*!
 * @brief Push a new delta and spread sample into the temporal window and update classification
 * @param[in,out] classifier Pointer to classifier instance
 * @param[in]     delta Current baseline-corrected net signal (LSB)
 * @param[in]     spread High-frequency noise spread metric from alpha-trim (LSB)
 * @return Detected classification class
 */
Touch_ClassificationClass_t Touch_TinyML_Update(Touch_TinyMLClassifier_t *classifier, float delta, float spread);

/*!
 * @brief Extract temporal envelope features from the history buffer
 * @param[in,out] classifier Pointer to classifier instance
 */
void Touch_TinyML_ExtractFeatures(Touch_TinyMLClassifier_t *classifier);

/*!
 * @brief Query whether current state is a confirmed, debounced touch
 * @param[in] classifier Pointer to classifier instance
 * @return TRUE if valid touch (bare finger or glove), FALSE otherwise
 */
boolean Touch_TinyML_IsValidTouch(const Touch_TinyMLClassifier_t *classifier);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_TINYML_H */
