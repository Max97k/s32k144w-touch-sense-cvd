/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : TinyML Envelope Classifier & Water Droplet Rejection Engine (M4)
*   Dependencies         : touch_tinyml.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
==================================================================================================*/

#include "touch_tinyml.h"

/*==================================================================================================
*                                   PRIVATE HELPER FUNCTIONS
==================================================================================================*/

static inline float AbsFloat(float v)
{
    return (v >= 0.0f) ? v : -v;
}

static inline float ClampFloat(float v, float lo, float hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

/*==================================================================================================
*                                   FUNCTION IMPLEMENTATIONS
==================================================================================================*/

void Touch_TinyML_Init(Touch_TinyML_Classifier_t *clf)
{
    if (clf == NULL_PTR)
    {
        return;
    }

    for (uint8_t i = 0U; i < TOUCH_TINYML_WINDOW_SIZE; i++)
    {
        clf->delta_win[i]  = 0.0f;
        clf->spread_win[i] = 0.6f;
    }

    for (uint8_t j = 0U; j < TOUCH_TINYML_CONF_WINDOW; j++)
    {
        clf->conf_win[j] = 0.0f;
    }

    clf->win_head                       = 0U;
    clf->sample_count                   = 0U;
    clf->active_sustain_cycles          = 0U;
    clf->features.attack_slope          = 0.0f;
    clf->features.dynamic_snr           = 0.0f;
    clf->features.plateau_variance      = 0.0f;
    clf->features.release_symmetry      = 0.0f;
    clf->features.dta_threshold         = TOUCH_DTA_GLOVE_MIN_LSB;
    clf->raw_confidence                 = 0.0f;
    clf->smoothed_confidence            = 0.0f;
    clf->confidence                     = 0.0f;
    clf->miss_cnt                       = 0U;
    clf->predicted_class                = TOUCH_CLASS_IDLE;
    clf->is_valid_touch                 = FALSE;
}

boolean Touch_TinyML_Update(Touch_TinyML_Classifier_t *clf, float delta, float spread, boolean spike_rejected)
{
    if (clf == NULL_PTR)
    {
        return FALSE;
    }

    /* 1. Push new observation into deterministic 10-sample circular sliding window */
    clf->delta_win[clf->win_head]  = delta;
    clf->spread_win[clf->win_head] = spread;

    uint8_t newest_idx = clf->win_head;
    clf->win_head = (uint8_t)((clf->win_head + 1U) % TOUCH_TINYML_WINDOW_SIZE);
    if (clf->sample_count < TOUCH_TINYML_WINDOW_SIZE)
    {
        clf->sample_count++;
    }

    /* Reconstruct chronological sequence d_seq[0 .. 9] where d_seq[9] is newest */
    float d_seq[TOUCH_TINYML_WINDOW_SIZE];
    float s_seq[TOUCH_TINYML_WINDOW_SIZE];
    for (uint8_t i = 0U; i < TOUCH_TINYML_WINDOW_SIZE; i++)
    {
        uint8_t idx = (uint8_t)((newest_idx + 1U + i) % TOUCH_TINYML_WINDOW_SIZE);
        d_seq[i] = clf->delta_win[idx];
        s_seq[i] = clf->spread_win[idx];
    }

    /* 2. Compute local 4-cycle average burst spread & Dynamic Threshold Adjustment (DTA) */
    float mean_spread_4 = 0.25f * (s_seq[6] + s_seq[7] + s_seq[8] + s_seq[9]);

    /* DTA lowers entry threshold to 34 LSB in quiet conditions (enabling 2mm acrylic / thin glove),
     * and raises it up to 58 LSB when high-frequency spread indicates water mist or EMI */
    float dta_th = TOUCH_DTA_GLOVE_MIN_LSB + (6.0f * (mean_spread_4 - 0.8f));
    dta_th = ClampFloat(dta_th, TOUCH_DTA_GLOVE_MIN_LSB, TOUCH_DTA_NOISY_MAX_LSB);
    clf->features.dta_threshold = dta_th;

    /* 3. Extract Feature 1: Attack Slope (Max 2-cycle positive rise across the 10-point window) */
    float max_attack_slope = 0.0f;
    float max_window_delta = 0.0f;
    for (uint8_t i = 0U; i < TOUCH_TINYML_WINDOW_SIZE; i++)
    {
        if (d_seq[i] > max_window_delta)
        {
            max_window_delta = d_seq[i];
        }
        if (i >= 2U)
        {
            float slope_2 = d_seq[i] - d_seq[i - 2U];
            if (slope_2 > max_attack_slope)
            {
                max_attack_slope = slope_2;
            }
        }
    }
    clf->features.attack_slope = max_attack_slope;

    /* 4. Extract Feature 2: Dynamic Peak SNR (Nam et al., Sensors 2021) */
    float snr_denom = mean_spread_4 + 0.5f;
    if (snr_denom < 0.8f)
    {
        snr_denom = 0.8f;
    }
    float dyn_snr = (delta > 0.0f) ? (delta / snr_denom) : 0.0f;
    clf->features.dynamic_snr = dyn_snr;

    /* 5. Extract Feature 3: Plateau Stability / Normalized Curvature Variance
     * Evaluates 2nd-order difference oscillation over recent 4 cycles:
     * Smooth finger landing/hold has low curvature relative to amplitude;
     * splashing water droplets or single-cycle edge swipes have high reversal curvature. */
    float diff1_a = d_seq[9] - d_seq[8];
    float diff1_b = d_seq[8] - d_seq[7];
    float diff1_c = d_seq[7] - d_seq[6];
    float curv_1  = AbsFloat(diff1_a - diff1_b);
    float curv_2  = AbsFloat(diff1_b - diff1_c);
    float norm_denom = (max_window_delta > 15.0f) ? max_window_delta : 15.0f;
    float plateau_var = ((0.6f * curv_1) + (0.4f * curv_2)) / norm_denom;
    clf->features.plateau_variance = plateau_var;

    /* 6. Extract Feature 4: Envelope Sustain & Release Symmetry Ratio */
    float sustain_ratio = (max_window_delta > 5.0f) ? (delta / max_window_delta) : 0.0f;
    sustain_ratio = ClampFloat(sustain_ratio, 0.0f, 1.0f);
    clf->features.release_symmetry = sustain_ratio;

    /* Track consecutive cycles above active DTA threshold (with hysteresis) */
    float active_gate = clf->is_valid_touch ? (dta_th * TOUCH_DTA_HYSTERESIS_RATIO) : dta_th;
    if (delta >= active_gate)
    {
        if (clf->active_sustain_cycles < 250U)
        {
            clf->active_sustain_cycles++;
        }
    }
    else
    {
        clf->active_sustain_cycles = 0U;
    }

    /* 7. TinyML Decision Tree & Soft-Margin Confidence Scoring */
    float p_inst = 0.0f;
    Touch_TinyML_Class_t pred_class = TOUCH_CLASS_IDLE;

    if (delta < 15.0f)
    {
        /* Clearly idle */
        p_inst = 0.0f;
        pred_class = TOUCH_CLASS_IDLE;
    }
    else if (((mean_spread_4 > TOUCH_FEAT_MAX_WATER_SPREAD) && (delta < TOUCH_MIN_BARE_DELTA_LSB)) ||
             (spike_rejected && (clf->active_sustain_cycles <= 1U) && (delta < TOUCH_MIN_BARE_DELTA_LSB)) ||
             ((delta < dta_th) && (max_attack_slope < TOUCH_FEAT_MIN_ATTACK_SLOPE)) ||
             ((dyn_snr < TOUCH_FEAT_MIN_DYNAMIC_SNR) && (delta < 65.0f)))
    {
        /* Water Droplet Drip / Mist Spray Signature:
         * - Detached droplet has weak floating coupling (< dta_th) or slow film creep, OR
         * - Droplet impact/mist causes elevated high-frequency spread (> 3.2 LSB) and low dynamic SNR */
        p_inst = 0.08f;
        pred_class = TOUCH_CLASS_REJECT_WATER;
    }
    else if ((delta < 150.0f) &&
             ((sustain_ratio < TOUCH_FEAT_MIN_SUSTAIN_RATIO) ||
              ((clf->active_sustain_cycles <= 1U) && (diff1_a < -8.0f)) ||
              ((plateau_var > TOUCH_FEAT_MAX_CURVATURE_RATIO) && (clf->active_sustain_cycles < 3U))))
    {
        /* Edge Grazing / Accidental Brush-By Signature:
         * - Brief 1~2 cycle transient peak that immediately reverses (diff1_a < -8 LSB or low sustain_ratio) */
        p_inst = 0.12f;
        pred_class = TOUCH_CLASS_REJECT_SWIPE;
    }
    else if (delta >= active_gate)
    {
        /* Valid Human Touch Envelope Confirmed:
         * Compute soft-margin confidence score combining SNR, attack slope, and plateau stability */
        float snr_score     = ClampFloat((dyn_snr - 12.0f) / 25.0f, 0.0f, 1.0f);
        float slope_score   = ClampFloat((max_attack_slope - 6.0f) / 30.0f, 0.0f, 1.0f);
        float sustain_score = ClampFloat(sustain_ratio, 0.0f, 1.0f);
        float stability_pen = (plateau_var > 0.85f) ? 0.15f : 0.0f;

        p_inst = (0.45f * snr_score) + (0.30f * slope_score) + (0.25f * sustain_score) - stability_pen;
        if (clf->active_sustain_cycles >= 2U)
        {
            p_inst += 0.18f; /* Boost confidence once sustained for >= 2 cycles (30 ms) */
        }
        p_inst = ClampFloat(p_inst, 0.0f, 1.0f);

        if (max_window_delta >= 78.0f)
        {
            pred_class = TOUCH_CLASS_VALID_BARE;
        }
        else
        {
            pred_class = TOUCH_CLASS_VALID_GLOVE;
        }
    }
    else
    {
        /* Approaching pre-touch zone below DTA gate */
        p_inst = 0.20f;
        pred_class = TOUCH_CLASS_IDLE;
    }

    clf->raw_confidence  = p_inst;
    clf->predicted_class = pred_class;

    /* 8. 3-Step Sliding Window Confidence Hysteresis (Pyeon et al., Sensors 2023) */
    clf->conf_win[0] = clf->conf_win[1];
    clf->conf_win[1] = clf->conf_win[2];
    clf->conf_win[2] = p_inst;

    float conf_avg = (clf->conf_win[0] + clf->conf_win[1] + clf->conf_win[2]) / 3.0f;
    clf->smoothed_confidence = conf_avg;
    clf->confidence          = conf_avg;

    if (clf->is_valid_touch)
    {
        if ((conf_avg < TOUCH_CONF_RELEASE_THRESH) || (delta < (dta_th * TOUCH_DTA_HYSTERESIS_RATIO)))
        {
            if (clf->miss_cnt < 255U)
            {
                clf->miss_cnt++;
            }
            if (clf->miss_cnt >= 4U)
            {
                clf->is_valid_touch = FALSE;
            }
        }
        else
        {
            clf->miss_cnt = 0U;
        }
    }
    else
    {
        clf->miss_cnt = 0U;
        if ((conf_avg >= TOUCH_CONF_TRIGGER_THRESH) && (clf->active_sustain_cycles >= 2U))
        {
            clf->is_valid_touch = TRUE;
        }
    }

    return clf->is_valid_touch;
}

Touch_TinyML_Class_t Touch_TinyML_GetClass(const Touch_TinyML_Classifier_t *clf)
{
    if (clf == NULL_PTR)
    {
        return TOUCH_CLASS_IDLE;
    }
    return clf->predicted_class;
}

boolean Touch_TinyML_IsValidTouch(const Touch_TinyML_Classifier_t *clf)
{
    if (clf == NULL_PTR)
    {
        return FALSE;
    }
    return clf->is_valid_touch;
}
