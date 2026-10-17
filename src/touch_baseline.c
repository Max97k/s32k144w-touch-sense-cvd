/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Baseline Tracking Engine (4-Zone Asymmetric DC Tracker)
*   Dependencies         : touch_baseline.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
*   SPDX-License-Identifier: BSD-3-Clause
==================================================================================================*/

#include "touch_baseline.h"

/*==================================================================================================
*                                   FUNCTION IMPLEMENTATIONS
==================================================================================================*/

void Touch_Baseline_Init(Touch_BaselineTracker_t *tracker, float initial_baseline, float touch_threshold)
{
    if (tracker == NULL_PTR)
    {
        return;
    }

    tracker->baseline          = initial_baseline;
    tracker->threshold_touch   = touch_threshold;
    tracker->threshold_release = (touch_threshold > 15.0f) ? (touch_threshold - 10.0f) : (touch_threshold * 0.75f);
    tracker->threshold_prepare = touch_threshold * TOUCH_APPROACH_RATIO;
    tracker->threshold_neg     = TOUCH_THRESH_NEG_RESET_LSB;
    tracker->delta             = 0.0f;
    tracker->is_touched        = FALSE;
    tracker->active_zone       = TOUCH_ZONE_IDLE_DRIFT;
    tracker->hold_cycle_count     = 0U;
    tracker->neg_cycle_count      = 0U;
    tracker->approach_cycle_count = 0U;
    tracker->total_updates        = 0U;
    tracker->force_reset_flag     = FALSE;
}

float Touch_Baseline_Update(Touch_BaselineTracker_t *tracker, float x_est, uint32_t delta_time_ms)
{
    if (tracker == NULL_PTR)
    {
        return 0.0f;
    }

    tracker->total_updates++;
    tracker->force_reset_flag = FALSE;

    float delta = x_est - tracker->baseline;
    float dt_sec = (delta_time_ms > 0U) ? ((float)delta_time_ms / 1000.0f) : ((float)TOUCH_CYCLE_INTERVAL_MS / 1000.0f);

    /* ---------------------------------------------------------------------------------------------
     * Zone 4: Negative Anomaly Fast Self-Healing (Touch-on-Boot Recovery)
     * Triggered when delta is significantly negative (< -15 LSB), e.g. finger released after boot.
     * --------------------------------------------------------------------------------------------- */
    if (delta < tracker->threshold_neg)
    {
        tracker->neg_cycle_count++;
        tracker->hold_cycle_count = 0U;
        tracker->is_touched       = FALSE;
        tracker->active_zone      = TOUCH_ZONE_NEG_RECOVERY;

        /* If negative deviation persists for >= 300 ms (TOUCH_NEG_RESET_CYCLES), snap baseline */
        if (tracker->neg_cycle_count >= TOUCH_NEG_RESET_CYCLES)
        {
            tracker->baseline         = x_est;
            tracker->delta            = 0.0f;
            tracker->neg_cycle_count  = 0U;
            tracker->force_reset_flag = TRUE;
            return 0.0f;
        }

        tracker->delta = delta;
        return delta;
    }

    /* Clear negative counter if we exited the negative zone */
    tracker->neg_cycle_count = 0U;

    /* ---------------------------------------------------------------------------------------------
     * Zone 3: Active Touch & Continuous Hold Timeout Protection
     * Triggered when delta >= threshold_touch (or delta >= threshold_release if already touched).
     * --------------------------------------------------------------------------------------------- */
    boolean touch_condition = FALSE;
    if (tracker->is_touched)
    {
        touch_condition = (delta >= tracker->threshold_release) ? TRUE : FALSE;
    }
    else
    {
        touch_condition = (delta >= tracker->threshold_touch) ? TRUE : FALSE;
    }

    if (touch_condition)
    {
        tracker->is_touched = TRUE;
        tracker->hold_cycle_count++;

        /* Check for 15-second continuous touch hold timeout (e.g. coin / object placed on pad) */
        if (tracker->hold_cycle_count >= TOUCH_HOLD_TIMEOUT_CYCLES)
        {
            /* Force baseline reset to current x_est to self-heal and release touch output */
            tracker->baseline         = x_est;
            tracker->delta            = 0.0f;
            tracker->is_touched       = FALSE;
            tracker->hold_cycle_count = 0U;
            tracker->force_reset_flag = TRUE;
            tracker->active_zone      = TOUCH_ZONE_HOLD_TIMEOUT;
            return 0.0f;
        }

        /* Baseline MUST BE FROZEN while touch is active */
        tracker->active_zone = TOUCH_ZONE_TOUCH_ACTIVE;
        tracker->delta       = delta;
        return delta;
    }

    /* Touch has been released */
    tracker->is_touched       = FALSE;
    tracker->hold_cycle_count = 0U;

    /* ---------------------------------------------------------------------------------------------
     * Zone 2: Finger Approaching Pre-Touch Zone (0.4 * TH <= delta < TH)
     * Hand is slowly approaching the pad (e.g. 5-second slow landing test).
     * Baseline MUST BE FROZEN to prevent slow finger signal from being swallowed into baseline.
     * Guardrail: If signal stays in Zone 2 for > 8.0s (e.g. water droplet or static step),
     * auto-timeout and reset baseline to eliminate permanent freeze deadlock.
     * --------------------------------------------------------------------------------------------- */
    if (delta >= tracker->threshold_prepare)
    {
        tracker->approach_cycle_count++;
        if (tracker->approach_cycle_count >= TOUCH_APPROACH_TIMEOUT_CYCLES)
        {
            /* Environmental step / water droplet stuck in pre-touch zone > 8s: self-heal baseline */
            tracker->baseline             = x_est;
            tracker->delta                = 0.0f;
            tracker->approach_cycle_count = 0U;
            tracker->force_reset_flag     = TRUE;
            tracker->active_zone          = TOUCH_ZONE_HOLD_TIMEOUT;
            return 0.0f;
        }

        tracker->active_zone = TOUCH_ZONE_APPROACH_FREEZE;
        tracker->delta       = delta;
        /* Baseline remains frozen */
        return delta;
    }

    /* Reset approach counter when outside Zone 2 */
    tracker->approach_cycle_count = 0U;

    /* ---------------------------------------------------------------------------------------------
     * Zone 1: Ambient Slow Environmental Drift (-15 LSB <= delta < 0.4 * TH)
     * Slowly adapt baseline to thermal and humidity variations with strict Slew-Rate Limiting.
     * --------------------------------------------------------------------------------------------- */
    tracker->active_zone = TOUCH_ZONE_IDLE_DRIFT;

    if (delta > 0.0f)
    {
        float max_step = TOUCH_SLEW_RATE_POS_LSB_S * dt_sec;
        float step = (delta < max_step) ? delta : max_step;
        tracker->baseline += step;
    }
    else if (delta < 0.0f)
    {
        float neg_delta = -delta;
        float max_step = TOUCH_SLEW_RATE_NEG_LSB_S * dt_sec;
        float step = (neg_delta < max_step) ? neg_delta : max_step;
        tracker->baseline -= step;
    }
    else
    {
        /* Exactly aligned; no adjustment */
    }

    tracker->delta = x_est - tracker->baseline;
    return tracker->delta;
}

void Touch_Baseline_ForceAlign(Touch_BaselineTracker_t *tracker, float new_baseline)
{
    if (tracker != NULL_PTR)
    {
        tracker->baseline         = new_baseline;
        tracker->delta            = 0.0f;
        tracker->is_touched       = FALSE;
        tracker->hold_cycle_count     = 0U;
        tracker->neg_cycle_count      = 0U;
        tracker->approach_cycle_count = 0U;
        tracker->force_reset_flag     = TRUE;
        tracker->active_zone          = TOUCH_ZONE_IDLE_DRIFT;
    }
}

boolean Touch_Baseline_IsTouched(const Touch_BaselineTracker_t *tracker)
{
    if (tracker == NULL_PTR)
    {
        return FALSE;
    }
    return tracker->is_touched;
}
