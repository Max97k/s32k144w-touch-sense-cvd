/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Filter Engine (LFSR, MFP, Alpha-Trim, 1D Adaptive Kalman)
*   Dependencies         : touch_filter.h, touch_cvd.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
*   SPDX-License-Identifier: BSD-3-Clause
==================================================================================================*/

#include "touch_filter.h"

/*==================================================================================================
*                                      LOCAL CONSTANTS
==================================================================================================*/

#define NOP() __asm__ volatile ("nop")

static const uint8_t s_coprime_delays[3] = {
    TOUCH_COPRIME_DELAY_0,
    TOUCH_COPRIME_DELAY_1,
    TOUCH_COPRIME_DELAY_2
};

/*==================================================================================================
*                                   FUNCTION IMPLEMENTATIONS
==================================================================================================*/

uint16_t Touch_LFSR_Next(uint16_t *lfsr)
{
    if ((lfsr == NULL_PTR) || (*lfsr == 0U))
    {
        /* LFSR all-zero state is forbidden; reset to default seed */
        if (lfsr != NULL_PTR)
        {
            *lfsr = TOUCH_LFSR_DEFAULT_SEED;
        }
        return TOUCH_LFSR_DEFAULT_SEED;
    }

    uint16_t lsb = *lfsr & 1U;
    *lfsr >>= 1U;
    if (lsb != 0U)
    {
        *lfsr ^= TOUCH_LFSR_POLYNOMIAL;
    }

    return *lfsr;
}

volatile Touch_RawSample_t g_last_burst_raw_sample = {0U, 0U, 0, 0U, 0};

void Touch_AcquireBurstMFP(int16_t *burst_samples, uint8_t count, uint8_t nop_count, uint16_t *lfsr_state)
{
    if ((burst_samples == NULL_PTR) || (count == 0U))
    {
        return;
    }

    for (uint8_t i = 0U; i < count; i++)
    {
        /* 1. Calculate pseudo-random dither based on LFSR + Coprime base delay */
        uint16_t prng = Touch_LFSR_Next(lfsr_state);
        uint8_t base_delay = s_coprime_delays[i % 3U];
        uint8_t jitter = (uint8_t)(prng & 0x07U); /* 0 ~ 7 extra cycles */
        uint16_t total_delay = (uint16_t)base_delay + (uint16_t)jitter;

        /* 2. Disperse inter-sample timing to break EMI harmonic lock */
        for (volatile uint16_t d = 0U; d < total_delay; d++)
        {
            NOP();
        }

        /* 3. Acquire 4-phase bipolar differential CVD sample */
        Touch_RawSample_t raw_s = Touch_AcquireSingleDiffCVD(nop_count);
        burst_samples[i] = raw_s.delta_D;

        if (i == (count / 2U))
        {
            g_last_burst_raw_sample = raw_s;
        }
    }
}

static volatile boolean s_last_burst_spike_detected = FALSE;

void Touch_AlphaTrimmedMean(const int16_t *burst_samples, uint8_t count, float *out_z_meas, float *out_spread)
{
    if ((burst_samples == NULL_PTR) || (out_z_meas == NULL_PTR) || (out_spread == NULL_PTR))
    {
        return;
    }

    if (count < 5U)
    {
        *out_z_meas = (float)burst_samples[0];
        *out_spread = 0.0f;
        return;
    }

    /* 1. Copy burst samples to deterministic stack buffer */
    int16_t sorted[TOUCH_BURST_SAMPLE_COUNT];
    uint8_t n = (count <= TOUCH_BURST_SAMPLE_COUNT) ? count : TOUCH_BURST_SAMPLE_COUNT;

    for (uint8_t i = 0U; i < n; i++)
    {
        sorted[i] = burst_samples[i];
    }

    /* 2. Deterministic Bounded Insertion Sort (O(N^2), max 36 iterations for N=9, 0 bytes heap) */
    for (uint8_t i = 1U; i < n; i++)
    {
        int16_t key = sorted[i];
        int8_t j = (int8_t)(i - 1U);

        while ((j >= 0) && (sorted[j] > key))
        {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    /* 3. Discard Top 2 and Bottom 2 extremes (spikes/EFT), average the middle 5 */
    /* For n = 9: indices are 0, 1 | 2, 3, 4, 5, 6 | 7, 8 */
    int32_t sum = 0;
    const uint8_t start_idx = TOUCH_TRIM_DISCARD_EACH;
    const uint8_t end_idx = (uint8_t)(n - 1U - TOUCH_TRIM_DISCARD_EACH);

    for (uint8_t i = start_idx; i <= end_idx; i++)
    {
        sum += (int32_t)sorted[i];
    }

    uint8_t trimmed_count = (uint8_t)(end_idx - start_idx + 1U);
    *out_z_meas = (float)sum / (float)trimmed_count;

    /* 4. High-frequency noise spread metric: 7th item minus 3rd item (0-indexed: [6] - [2]) */
    *out_spread = (float)(sorted[end_idx] - sorted[start_idx]);

    /* 5. Integrated Spike Detection: Compute EFT spike gaps directly on sorted array (zero re-sorting overhead!) */
    int16_t high_outlier_gap = sorted[n - 1U] - sorted[end_idx];
    int16_t low_outlier_gap  = sorted[start_idx] - sorted[0U];

    s_last_burst_spike_detected = ((high_outlier_gap > TOUCH_SPIKE_REJECT_TH_LSB) || (low_outlier_gap > TOUCH_SPIKE_REJECT_TH_LSB)) ? TRUE : FALSE;
}

boolean Touch_DetectBurstSpike(const int16_t *burst_samples, uint8_t count)
{
    (void)burst_samples;
    (void)count;
    /* Return cached spike decision computed during Touch_AlphaTrimmedMean (saves ~15-20 us CPU time!) */
    return s_last_burst_spike_detected;
}

void Touch_Kalman_Init(Touch_Kalman_t *kf, float initial_val, float q_proc, float r_base, float alpha)
{
    if (kf != NULL_PTR)
    {
        kf->x_est  = initial_val;
        kf->p_est  = 1.0f;
        kf->q_proc = q_proc;
        kf->r_base = r_base;
        kf->alpha  = alpha;
        kf->k_gain = 0.5f;
        kf->spread = 0.0f;
    }
}

float Touch_Kalman_Update(Touch_Kalman_t *kf, float z_meas, float spread)
{
    if (kf == NULL_PTR)
    {
        return z_meas;
    }

    /* 1. Time Update (Predict) */
    /* Constant-position kinematic model: x_pred = x_est */
    float x_pred = kf->x_est;
    float p_pred = kf->p_est + kf->q_proc;

    /* Large consistent step (measured real finger: ~+1300 LSB): burst samples agree
     * (spread small relative to the step), so it is genuine, not EFT. Follow immediately. */
    float innov = z_meas - x_pred;
    float abs_innov = (innov < 0.0f) ? -innov : innov;
    if ((abs_innov > 100.0f) && (spread < (0.25f * abs_innov)))
    {
        kf->x_est  = z_meas;
        kf->p_est  = kf->r_base;
        kf->k_gain = 0.99f;
        return kf->x_est;
    }

    /* 2. Adaptive Measurement Noise Variance mapping */
    /* R_k = R_base + alpha * (spread)^2 */
    float r_k = kf->r_base + (kf->alpha * (spread * spread));

    /* 3. Measurement Update (Correct) */
    /* Kalman Gain: K_k = P_pred / (P_pred + R_k) */
    float denom = p_pred + r_k;
    float k_k;

    if (denom > 0.00001f)
    {
        k_k = p_pred / denom;
    }
    else
    {
        k_k = 0.5f;
    }

    /* Clamp Kalman gain within [0.01, 0.99] to guarantee stability */
    if (k_k < 0.01f)
    {
        k_k = 0.01f;
    }
    else if (k_k > 0.99f)
    {
        k_k = 0.99f;
    }
    else
    {
        /* within safe bounds */
    }

    /* Update state estimate: x_est = x_pred + K_k * (z_meas - x_pred) */
    kf->x_est = x_pred + (k_k * (z_meas - x_pred));

    /* Update error covariance: P_est = (1 - K_k) * P_pred */
    kf->p_est = (1.0f - k_k) * p_pred;

    /* Prevent error covariance underflow */
    if (kf->p_est < 0.001f)
    {
        kf->p_est = 0.001f;
    }

    /* Cache diagnostics */
    kf->k_gain = k_k;
    kf->spread = spread;

    return kf->x_est;
}
