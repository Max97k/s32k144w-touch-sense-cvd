/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Temporal Feature Envelope & TinyML Classifier Engine
*   Dependencies         : touch_tinyml.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
==================================================================================================*/

#include "touch_tinyml.h"
#include <math.h>

/*==================================================================================================
*                                   FUNCTION IMPLEMENTATIONS
==================================================================================================*/

void Touch_TinyML_Init(Touch_TinyMLClassifier_t *classifier)
{
    if (classifier == NULL_PTR)
    {
        return;
    }

    for (uint8_t i = 0U; i < TOUCH_TINYML_WINDOW_SIZE; i++)
    {
        classifier->history_delta[i]  = 0.0f;
        classifier->history_spread[i] = 1.0f;
    }
    classifier->head_idx     = 0U;
    classifier->sample_count = 0U;

    for (uint8_t i = 0U; i < TOUCH_TINYML_CONF_WINDOW_SIZE; i++)
    {
        classifier->conf_window[i] = 0.0f;
    }
    classifier->conf_head = 0U;

    classifier->features.attack_slope      = 0.0f;
    classifier->features.dynamic_snr_db    = 0.0f;
    classifier->features.plateau_variance  = 0.0f;
    classifier->features.release_symmetry  = 1.0f;
    classifier->features.peak_delta        = 0.0f;
    classifier->features.contact_duration  = 0U;

    classifier->detected_class         = TOUCH_CLASS_NONE;
    classifier->confidence             = 0.0f;
    classifier->is_valid_touch         = FALSE;
    classifier->total_classifications  = 0U;
    classifier->water_rejections_count = 0U;
    classifier->hover_rejections_count = 0U;
}

void Touch_TinyML_ExtractFeatures(Touch_TinyMLClassifier_t *classifier)
{
    if ((classifier == NULL_PTR) || (classifier->sample_count == 0U))
    {
        return;
    }

    /* 1. Linearize chronological order: ordered[0] is oldest, ordered[N-1] is newest */
    float ordered_delta[TOUCH_TINYML_WINDOW_SIZE];
    float ordered_spread[TOUCH_TINYML_WINDOW_SIZE];
    uint8_t n = classifier->sample_count;

    for (uint8_t i = 0U; i < n; i++)
    {
        uint8_t idx = (uint8_t)((classifier->head_idx + TOUCH_TINYML_WINDOW_SIZE - n + i) % TOUCH_TINYML_WINDOW_SIZE);
        ordered_delta[i]  = classifier->history_delta[idx];
        ordered_spread[i] = classifier->history_spread[idx];
    }

    /* 2. Compute Peak Delta, Sum Spread, and Contact Duration */
    float peak = 0.0f;
    uint8_t duration = 0U;
    float sum_spread = 0.0f;

    for (uint8_t i = 0U; i < n; i++)
    {
        if (ordered_delta[i] > peak)
        {
            peak = ordered_delta[i];
        }
        sum_spread += ordered_spread[i];
    }

    /* Contact duration: count consecutive recent samples >= 20.0 LSB (approach threshold) */
    for (int8_t i = (int8_t)(n - 1U); i >= 0; i--)
    {
        if (ordered_delta[i] >= 20.0f)
        {
            duration++;
        }
        else
        {
            break;
        }
    }

    classifier->features.peak_delta       = peak;
    classifier->features.contact_duration = duration;

    /* 3. Compute Attack Slope: S_attack = max_i (delta[i] - delta[i-2]) / (2 * dt) */
    float max_slope = 0.0f;
    if (n >= 3U)
    {
        for (uint8_t i = 2U; i < n; i++)
        {
            float s = (ordered_delta[i] - ordered_delta[i - 2U]) / 2.0f;
            if (s > max_slope)
            {
                max_slope = s;
            }
        }
    }
    else if (n >= 2U)
    {
        max_slope = ordered_delta[n - 1U] - ordered_delta[n - 2U];
    }
    else
    {
        max_slope = 0.0f;
    }
    classifier->features.attack_slope = (max_slope > 0.0f) ? max_slope : 0.0f;

    /* 4. Compute Plateau Variance (stability during contact)
     * Evaluated ONLY when contact has persisted for >= 4 cycles (60 ms),
     * preventing rising attack edges from artificially inflating variance. */
    if (classifier->features.contact_duration >= 4U)
    {
        float p_sum = 0.0f;
        for (uint8_t i = n - 4U; i < n; i++)
        {
            p_sum += ordered_delta[i];
        }
        float p_mean = p_sum / 4.0f;

        float var_sum = 0.0f;
        for (uint8_t i = n - 4U; i < n; i++)
        {
            float diff = ordered_delta[i] - p_mean;
            var_sum += (diff * diff);
        }
        classifier->features.plateau_variance = var_sum / 4.0f;
    }
    else
    {
        classifier->features.plateau_variance = 0.0f;
    }

    /* 5. Compute Dynamic SNR */
    float avg_spread = (n > 0U) ? (sum_spread / (float)n) : 1.0f;
    if (avg_spread < 0.2f)
    {
        avg_spread = 0.2f;
    }
    if (peak > 0.5f)
    {
        classifier->features.dynamic_snr_db = 20.0f * log10f(peak / avg_spread);
    }
    else
    {
        classifier->features.dynamic_snr_db = 0.0f;
    }

    /* 6. Release Symmetry */
    classifier->features.release_symmetry = 1.0f;
}

Touch_ClassificationClass_t Touch_TinyML_Update(Touch_TinyMLClassifier_t *classifier, float delta, float spread)
{
    if (classifier == NULL_PTR)
    {
        return TOUCH_CLASS_NONE;
    }

    classifier->total_classifications++;

    /* 1. Append new sample into circular history buffer */
    classifier->history_delta[classifier->head_idx]  = delta;
    classifier->history_spread[classifier->head_idx] = spread;
    classifier->head_idx = (uint8_t)((classifier->head_idx + 1U) % TOUCH_TINYML_WINDOW_SIZE);
    if (classifier->sample_count < TOUCH_TINYML_WINDOW_SIZE)
    {
        classifier->sample_count++;
    }

    /* 2. Extract multi-dimensional temporal geometric envelope features */
    Touch_TinyML_ExtractFeatures(classifier);

    Touch_EnvelopeFeatures_t *feat = &classifier->features;
    Touch_ClassificationClass_t inst_class = TOUCH_CLASS_NONE;
    float inst_prob = 0.0f;

    /* 3. TinyML Decision Tree Feature Classification Engine */
    if (feat->peak_delta < 20.0f)
    {
        /* Quiescent / Untouched */
        inst_class = TOUCH_CLASS_NONE;
        inst_prob  = 0.0f;
    }
    else if ((feat->peak_delta >= 25.0f) && (feat->contact_duration <= 2U) && (delta < 25.0f))
    {
        /* Edge Swipe / Grazing Hover: short transient burst (< 45 ms) without sustained contact */
        inst_class = TOUCH_CLASS_EDGE_HOVER;
        inst_prob  = 0.05f;
        classifier->hover_rejections_count++;
    }
    else if ((feat->peak_delta <= TOUCH_WATER_MAX_DELTA_LSB) && (feat->dynamic_snr_db < 32.0f) && (feat->attack_slope < TOUCH_MIN_ATTACK_SLOPE))
    {
        /* Water Droplet / Condensation: low-to-moderate delta, lacking human capacitive coupling */
        inst_class = TOUCH_CLASS_WATER_DROPLET;
        inst_prob  = 0.02f;
        classifier->water_rejections_count++;
    }
    else if ((feat->plateau_variance > TOUCH_MAX_PLATEAU_VAR_LSB2) && (feat->contact_duration >= 4U))
    {
        /* Water Droplet Splash / Slosh: unstable contact plateau with meniscus ringing oscillation */
        inst_class = TOUCH_CLASS_WATER_DROPLET;
        inst_prob  = 0.05f;
        classifier->water_rejections_count++;
    }
    else if ((feat->peak_delta <= TOUCH_WATER_MAX_DELTA_LSB) && (feat->dynamic_snr_db < 32.0f) && (feat->attack_slope < TOUCH_MIN_ATTACK_SLOPE))
    {
        /* Water Droplet / Condensation: low-to-moderate delta, lacking human capacitive coupling */
        inst_class = TOUCH_CLASS_WATER_DROPLET;
        inst_prob  = 0.02f;
        classifier->water_rejections_count++;
    }
    else if ((feat->peak_delta <= TOUCH_WATER_MAX_DELTA_LSB) && (spread > 1.5f))
    {
        /* Water spray mist: high dispersion metric */
        inst_class = TOUCH_CLASS_WATER_DROPLET;
        inst_prob  = 0.02f;
        classifier->water_rejections_count++;
    }
    else if ((feat->peak_delta >= TOUCH_MIN_BARE_DELTA_LSB) &&
             (feat->attack_slope >= TOUCH_MIN_ATTACK_SLOPE) &&
             (feat->plateau_variance <= TOUCH_MAX_PLATEAU_VAR_LSB2))
    {
        /* Verified Bare Finger: strong signal, fast attack, stable fleshy plateau */
        inst_class = TOUCH_CLASS_BARE_FINGER;
        inst_prob  = 1.0f;
    }
    else if ((feat->peak_delta >= TOUCH_MIN_GLOVE_DELTA_LSB) &&
             (feat->contact_duration >= 2U) &&
             (feat->plateau_variance <= TOUCH_MAX_PLATEAU_VAR_LSB2) &&
             (spread <= 1.2f))
    {
        /* Verified Glove / Acrylic Overlay: moderate signal with stable human contact plateau */
        inst_class = TOUCH_CLASS_GLOVE_OVERLAY;
        inst_prob  = 1.0f;
    }
    else if ((feat->peak_delta >= 50.0f) &&
             (feat->contact_duration >= 2U) &&
             (feat->plateau_variance <= TOUCH_MAX_PLATEAU_VAR_LSB2) &&
             (spread <= 1.2f))
    {
        /* Fallback threshold touch with verified stable contact and low noise */
        inst_class = TOUCH_CLASS_BARE_FINGER;
        inst_prob  = 0.85f;
    }
    else
    {
        inst_class = TOUCH_CLASS_NONE;
        inst_prob  = 0.10f;
    }

    classifier->detected_class = inst_class;

    /* 4. Multi-Cycle Confidence Sliding Window Smoothing */
    classifier->conf_window[classifier->conf_head] = inst_prob;
    classifier->conf_head = (uint8_t)((classifier->conf_head + 1U) % TOUCH_TINYML_CONF_WINDOW_SIZE);

    float sum_conf = 0.0f;
    for (uint8_t i = 0U; i < TOUCH_TINYML_CONF_WINDOW_SIZE; i++)
    {
        sum_conf += classifier->conf_window[i];
    }
    classifier->confidence = sum_conf / (float)TOUCH_TINYML_CONF_WINDOW_SIZE;

    /* 5. 3-Step Confidence Hysteresis Filtering */
    if (classifier->is_valid_touch)
    {
        if (classifier->confidence < TOUCH_CONF_RELEASE_THRESHOLD)
        {
            classifier->is_valid_touch = FALSE;
        }
    }
    else
    {
        if ((classifier->confidence >= TOUCH_CONF_TRIGGER_THRESHOLD) &&
            ((inst_class == TOUCH_CLASS_BARE_FINGER) || (inst_class == TOUCH_CLASS_GLOVE_OVERLAY)))
        {
            classifier->is_valid_touch = TRUE;
        }
    }

    return classifier->detected_class;
}

boolean Touch_TinyML_IsValidTouch(const Touch_TinyMLClassifier_t *classifier)
{
    if (classifier == NULL_PTR)
    {
        return FALSE;
    }
    return classifier->is_valid_touch;
}
