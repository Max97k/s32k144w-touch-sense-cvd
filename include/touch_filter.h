/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : Filter Engine (LFSR, MFP, Alpha-Trim, 1D Adaptive Kalman)
*   Dependencies         : StandardTypes.h, touch_cvd.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright (c) 2026, Max97k
*   All Rights Reserved.
==================================================================================================*/

#ifndef TOUCH_FILTER_H
#define TOUCH_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "StandardTypes.h"
#include <stdint.h>
#include "touch_cvd.h"

/*==================================================================================================
*                                      DEFINES AND CONSTANTS
==================================================================================================*/

/* Burst sample size for multi-frequency acquisition */
#define TOUCH_BURST_SAMPLE_COUNT   (9U)

/* Alpha-Trimmed Mean: Discard top 2 and bottom 2 extremes, average middle 5 */
#define TOUCH_TRIM_DISCARD_EACH    (2U)
#define TOUCH_TRIM_AVERAGE_COUNT   (5U)

/* 16-bit Galois LFSR Polynomial: x^16 + x^14 + x^13 + x^11 + 1 */
#define TOUCH_LFSR_POLYNOMIAL      (0xB400U)
#define TOUCH_LFSR_DEFAULT_SEED    (0xACE1U)

/* Three coprime base delay loops to disperse harmonic interference (IEC 61000-4-6) */
#define TOUCH_COPRIME_DELAY_0      (15U)
#define TOUCH_COPRIME_DELAY_1      (23U)
#define TOUCH_COPRIME_DELAY_2      (31U)

/* 1D Adaptive Kalman Filter Parameters tuned to M1 baseline noise (Q=0.35, Rbase=1.0, Alpha=0.75) */
#define TOUCH_KALMAN_DEFAULT_Q     (0.35f)   /* Process noise covariance (0.25 ~ 0.5 for fast tap tracking) */
#define TOUCH_KALMAN_DEFAULT_RBASE (1.0f)    /* Base measurement noise covariance */
#define TOUCH_KALMAN_DEFAULT_ALPHA (0.75f)   /* Adaptive factor mapping spread^2 -> R_k for EFT rejection */

/*==================================================================================================
*                                STRUCTURES AND DATA TYPES
==================================================================================================*/

/*!
 * @brief 1D Adaptive Scalar Kalman Filter State Structure
 */
typedef struct {
    float x_est;     /*!< Filtered touch state estimate (LSB) */
    float p_est;     /*!< Estimation error covariance */
    float q_proc;    /*!< Process noise covariance */
    float r_base;    /*!< Quiescent measurement noise covariance */
    float alpha;     /*!< Adaptive weighting coefficient for spread^2 */
    float k_gain;    /*!< Last computed Kalman gain K_k [0.0 ~ 1.0] */
    float spread;    /*!< Last measured sample spread (sample[6] - sample[2]) */
} Touch_Kalman_t;

/*!
 * @brief Milestone 2 Comprehensive Filter Pipeline Telemetry Record
 */
typedef struct {
    int16_t  raw_diff;       /*!< Single raw differential sample (LSB) */
    float    z_meas;         /*!< Alpha-trimmed mean observation (LSB) */
    float    x_est;          /*!< Adaptive Kalman filter output (LSB) */
    float    spread;         /*!< High-frequency noise metric spread (LSB) */
    float    kalman_gain;    /*!< Instantaneous Kalman gain K_k */
    boolean  spike_rejected; /*!< Set TRUE if EFT/ESD pulse was clipped by alpha-trim */
} Touch_M2_FilteredSample_t;

/*==================================================================================================
*                                FUNCTION PROTOTYPES
==================================================================================================*/

/*!
 * @brief Advance 16-bit Galois Linear Feedback Shift Register (LFSR)
 * @param[in,out] lfsr Pointer to current LFSR state (must be non-zero)
 * @return 16-bit pseudo-random value
 */
uint16_t Touch_LFSR_Next(uint16_t *lfsr);

/*!
 * @brief Acquire 9x Bipolar Differential CVD samples with Coprime Multi-Frequency Jitter
 * @param[out] burst_samples Destination array of length count
 * @param[in] count Number of samples to collect (typically TOUCH_BURST_SAMPLE_COUNT)
 * @param[in] nop_count Charge balancing delay (from M1 calibration)
 * @param[in,out] lfsr_state Pointer to LFSR PRNG state
 */
void Touch_AcquireBurstMFP(int16_t *burst_samples, uint8_t count, uint8_t nop_count, uint16_t *lfsr_state);

/*!
 * @brief Apply Alpha-Trimmed Mean Filter to reject spikes and compute noise spread
 * @param[in] burst_samples Array of raw burst samples
 * @param[in] count Number of burst samples (must be at least 5, typically 9)
 * @param[out] out_z_meas Pointer to output trimmed mean observation
 * @param[out] out_spread Pointer to output noise spread metric
 * @param[out] out_spike_rejected Pointer to output EFT/ESD spike rejection flag (can be NULL)
 */
void Touch_AlphaTrimmedMean(const int16_t *burst_samples, uint8_t count, float *out_z_meas, float *out_spread, boolean *out_spike_rejected);

/*!
 * @brief Initialize 1D Adaptive Scalar Kalman Filter
 * @param[out] kf Pointer to Kalman filter instance
 * @param[in] initial_val Initial state estimate (e.g. initial z_meas or baseline)
 * @param[in] q_proc Process noise covariance
 * @param[in] r_base Base measurement noise covariance
 * @param[in] alpha Adaptive sensitivity factor
 */
void Touch_Kalman_Init(Touch_Kalman_t *kf, float initial_val, float q_proc, float r_base, float alpha);

/*!
 * @brief Update 1D Adaptive Scalar Kalman Filter with new observation and spread
 * @param[in,out] kf Pointer to Kalman filter instance
 * @param[in] z_meas Current alpha-trimmed observation
 * @param[in] spread Instantaneous sample spread
 * @return Updated state estimate x_est
 */
float Touch_Kalman_Update(Touch_Kalman_t *kf, float z_meas, float spread);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_FILTER_H */
