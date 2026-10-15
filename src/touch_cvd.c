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
==================================================================================================*/

#include "touch_cvd.h"

#define TIMEOUT_GUARD_CYCLES  (200000U)
#define NOP()                 __asm__ volatile ("nop")

static inline void DisableAllInterrupts(void)
{
    __asm__ volatile ("cpsid i" : : : "memory");
}

static inline void EnableAllInterrupts(void)
{
    __asm__ volatile ("cpsie i" : : : "memory");
}

/* Exact cycle NOP delay without loop branch jitter */
static inline void NopDelayExact(uint8_t count)
{
    switch (count)
    {
        case 16: NOP(); /* fallthrough */
        case 15: NOP(); /* fallthrough */
        case 14: NOP(); /* fallthrough */
        case 13: NOP(); /* fallthrough */
        case 12: NOP(); /* fallthrough */
        case 11: NOP(); /* fallthrough */
        case 10: NOP(); /* fallthrough */
        case 9:  NOP(); /* fallthrough */
        case 8:  NOP(); /* fallthrough */
        case 7:  NOP(); /* fallthrough */
        case 6:  NOP(); /* fallthrough */
        case 5:  NOP(); /* fallthrough */
        case 4:  NOP(); /* fallthrough */
        case 3:  NOP(); /* fallthrough */
        case 2:  NOP(); /* fallthrough */
        case 1:  NOP(); /* fallthrough */
        default: break;
    }
}

void Touch_SetStatusLeds(boolean red_on, boolean green_on, boolean blue_on)
{
    if (red_on)
    {
        LED_RED_GPIO->PSOR = (1U << LED_RED_PIN);
    }
    else
    {
        LED_RED_GPIO->PCOR = (1U << LED_RED_PIN);
    }

    if (green_on)
    {
        LED_GREEN_GPIO->PSOR = (1U << LED_GREEN_PIN);
    }
    else
    {
        LED_GREEN_GPIO->PCOR = (1U << LED_GREEN_PIN);
    }

    if (blue_on)
    {
        LED_BLUE_GPIO->PSOR = (1U << LED_BLUE_PIN);
    }
    else
    {
        LED_BLUE_GPIO->PCOR = (1U << LED_BLUE_PIN);
    }
}

void Touch_Init(void)
{
    /* 1. Enable PORTC and PORTE Clock Gates */
    IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;
    IP_PCC->PCCn[PCC_PORTE_INDEX] |= PCC_PCCn_CGC_MASK;

    /* 2. Configure RGB Status LEDs (PTE7, PTE0, PTE3) */
    LED_RED_PORT->PCR[LED_RED_PIN]     = PORT_PCR_MUX(1U); /* GPIO */
    LED_GREEN_PORT->PCR[LED_GREEN_PIN] = PORT_PCR_MUX(1U); /* GPIO */
    LED_BLUE_PORT->PCR[LED_BLUE_PIN]   = PORT_PCR_MUX(1U); /* GPIO */

    LED_RED_GPIO->PCOR   = (1U << LED_RED_PIN);
    LED_GREEN_GPIO->PCOR = (1U << LED_GREEN_PIN);
    LED_BLUE_GPIO->PCOR  = (1U << LED_BLUE_PIN);

    LED_RED_GPIO->PDDR   |= (1U << LED_RED_PIN);
    LED_GREEN_GPIO->PDDR |= (1U << LED_GREEN_PIN);
    LED_BLUE_GPIO->PDDR  |= (1U << LED_BLUE_PIN);

    /* 3. Configure Touch Pins (PTC16 = TOUCH_B/SE14, PTC17 = TOUCH_A/SE15)
     * Internal pull-up/pull-down MUST be disabled for high-impedance capacitive divider.
     * Note on MUX(1) Digital Input Buffer:
     *   Keeping MUX=1 (GPIO) allows single-cycle atomic PDDR direction switching.
     *   When balanced at ~0.5*VDD during High-Z, the CMOS Schmitt trigger operates near threshold.
     *   To prevent shoot-through current in low power / VLPS (M6) and thermal drift (M3),
     *   pins MUST ALWAYS be returned to Idle Grounding (0V) when acquisition is not actively running.
     */
    TOUCH_PORT_BASE->PCR[TOUCH_PIN_SENSE_B] = PORT_PCR_MUX(1U) | PORT_PCR_PE(0U);
    TOUCH_PORT_BASE->PCR[TOUCH_PIN_DRV_A]   = PORT_PCR_MUX(1U) | PORT_PCR_PE(0U);

    /* Set default state: Idle Grounding (Output Low, 0V) */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_SENSE_B) | (1U << TOUCH_PIN_DRV_A);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_SENSE_B) | (1U << TOUCH_PIN_DRV_A);
}

boolean Touch_Adc0_Calibrate(void)
{
    uint32_t guard = 0U;

    /* 1. Enable ADC0 clock gate with 48.0 MHz FIRC source (PCS=3) */
    IP_PCC->PCCn[PCC_ADC0_INDEX] &= ~PCC_PCCn_CGC_MASK;
    IP_PCC->PCCn[PCC_ADC0_INDEX] = PCC_PCCn_PCS(3U) | PCC_PCCn_CGC_MASK;

    /* 2. Configure ADC0: 12-bit mode, Clock / 2 (24 MHz for calibration, well within 20~50 MHz RM spec) */
    IP_ADC0->CFG1 = ADC_CFG1_MODE(1U) | ADC_CFG1_ADIV(1U) | ADC_CFG1_ADICLK(0U);
    IP_ADC0->CFG2 = ADC_CFG2_SMPLTS(12U);

    /* 3. Enable 32x hardware averaging for self-calibration per NXP AN12217 */
    IP_ADC0->SC3 = ADC_SC3_AVGE_MASK | ADC_SC3_AVGS(3U);

    /* 4. Initiate Hardware Self-Calibration */
    IP_ADC0->SC3 |= ADC_SC3_CAL_MASK;

    /* 5. Bounded wait for calibration to complete (hardware clears CAL bit automatically) */
    while (((IP_ADC0->SC3 & ADC_SC3_CAL_MASK) != 0U) && (guard < TIMEOUT_GUARD_CYCLES))
    {
        guard++;
    }

    /* Check both timeout guard and hardware CALF (Calibration Failed) flag */
    if ((guard >= TIMEOUT_GUARD_CYCLES) || ((IP_ADC0->SC3 & ADC_SC3_CALF_MASK) != 0U))
    {
        return (boolean)FALSE;
    }

    /* 6. Disable hardware averaging for single-shot CVD acquisition */
    IP_ADC0->SC3 = 0U;

    /* 7. Clear conversion complete flag by reading result register */
    (void)IP_ADC0->R[0];

    return (boolean)TRUE;
}

uint32_t Touch_ReadBandgap_mV(uint16_t *out_raw_bandgap)
{
    uint32_t guard = 0U;
    uint32_t raw_bg;

    /* Enable PMC Bandgap bias buffer (1.00V nominal reference) */
    IP_PMC->REGSC |= PMC_REGSC_BIASEN_MASK;

    /* Trigger ADC0 conversion on Channel 27 (Bandgap) */
    IP_ADC0->SC1[0] = TOUCH_ADC0_CH_BANDGAP;

    while ((!(IP_ADC0->SC1[0] & ADC_SC1_COCO_MASK)) && (guard < TIMEOUT_GUARD_CYCLES))
    {
        guard++;
    }

    raw_bg = (uint32_t)(IP_ADC0->R[0] & 0x0FFFU);
    if (out_raw_bandgap != NULL_PTR)
    {
        *out_raw_bandgap = (uint16_t)raw_bg;
    }

    if (raw_bg == 0U)
    {
        return 5000U;
    }

    /* Vrefh (mV) = (1000 mV * 4095) / raw_bandgap */
    return (uint32_t)((1000UL * 4095UL) / raw_bg);
}

Touch_RawSample_t Touch_AcquireSingleDiffCVD(uint8_t nop_balance_count)
{
    Touch_RawSample_t sample;
    uint32_t guard;

    /* Enter atomic critical section: Microsecond CVD timing must not be interrupted */
    DisableAllInterrupts();

    /* =========================================================================
     * Phase 1: Scan A (Positive Phase Precharge & Share)
     *   - PTC17 (TOUCH_A, Pad SW7) = 0V
     *   - PTC16 (TOUCH_B, Ref C533) = VDD
     * ========================================================================= */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_DRV_A);
    TOUCH_GPIO_BASE->PSOR = (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);

    /* ~60 ns precharge pulse */
    NOP(); NOP(); NOP();

    /* Synchronously disconnect drive: single instruction switches both pins to High-Z */
    TOUCH_GPIO_BASE->PDDR &= ~((1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B));

    /* Wait for charge to transfer across R525 (4.7k) */
    NopDelayExact(nop_balance_count);

    /* Trigger ADC0_SE14 on PTC16 */
    IP_ADC0->SC1[0] = TOUCH_ADC0_CH_SENSE_B;

    guard = 0U;
    while ((!(IP_ADC0->SC1[0] & ADC_SC1_COCO_MASK)) && (guard < TIMEOUT_GUARD_CYCLES))
    {
        guard++;
    }
    sample.sample_A = (uint16_t)(IP_ADC0->R[0] & 0x0FFFU);

    /* =========================================================================
     * Phase 2: Scan B (Negative Phase Precharge & Share)
     *   - PTC17 (TOUCH_A, Pad SW7) = VDD
     *   - PTC16 (TOUCH_B, Ref C533) = 0V
     * ========================================================================= */
    TOUCH_GPIO_BASE->PSOR = (1U << TOUCH_PIN_DRV_A);
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);

    /* ~60 ns precharge pulse */
    NOP(); NOP(); NOP();

    /* Synchronously disconnect drive: single instruction switches both pins to High-Z */
    TOUCH_GPIO_BASE->PDDR &= ~((1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B));

    /* Wait for charge to transfer across R525 (4.7k) */
    NopDelayExact(nop_balance_count);

    /* Trigger ADC0_SE14 on PTC16 */
    IP_ADC0->SC1[0] = TOUCH_ADC0_CH_SENSE_B;

    guard = 0U;
    while ((!(IP_ADC0->SC1[0] & ADC_SC1_COCO_MASK)) && (guard < TIMEOUT_GUARD_CYCLES))
    {
        guard++;
    }
    sample.sample_B = (uint16_t)(IP_ADC0->R[0] & 0x0FFFU);

    /* =========================================================================
     * Phase 3: Idle Grounding
     * Immediately ground both electrodes to prevent floating EMI antenna pickup
     * ========================================================================= */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);

    /* Exit atomic critical section */
    EnableAllInterrupts();

    /* Phase 4: Differential Synthesis delta_D = sample_B - sample_A */
    sample.delta_D = (int16_t)sample.sample_B - (int16_t)sample.sample_A;
    sample.sample_A_se15 = 0U;
    sample.balance_diff = 0;

    return sample;
}

int16_t Touch_MeasureBalanceDiff(uint8_t nop_count)
{
    uint16_t v14 = 0U;
    uint16_t v15 = 0U;
    volatile uint32_t guard;
    int16_t diff;

    DisableAllInterrupts();

    /* Pass 1: Precharge -> High-Z -> nop_count -> Sample SE14 (TOUCH_B) */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_DRV_A);
    TOUCH_GPIO_BASE->PSOR = (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);
    NOP(); NOP(); NOP();

    TOUCH_GPIO_BASE->PDDR &= ~((1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B));
    NopDelayExact(nop_count);

    IP_ADC0->SC1[0] = TOUCH_ADC0_CH_SENSE_B;
    guard = 0U;
    while ((!(IP_ADC0->SC1[0] & ADC_SC1_COCO_MASK)) && (guard < TIMEOUT_GUARD_CYCLES)) guard++;
    v14 = (uint16_t)(IP_ADC0->R[0] & 0x0FFFU);

    /* Discharge & settle briefly before Pass 2 */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);
    NOP(); NOP(); NOP();

    /* Pass 2: Identical Precharge -> High-Z -> nop_count -> Sample SE15 (TOUCH_A) */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_DRV_A);
    TOUCH_GPIO_BASE->PSOR = (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);
    NOP(); NOP(); NOP();

    TOUCH_GPIO_BASE->PDDR &= ~((1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B));
    NopDelayExact(nop_count);

    IP_ADC0->SC1[0] = TOUCH_ADC0_CH_DRV_A;
    guard = 0U;
    while ((!(IP_ADC0->SC1[0] & ADC_SC1_COCO_MASK)) && (guard < TIMEOUT_GUARD_CYCLES)) guard++;
    v15 = (uint16_t)(IP_ADC0->R[0] & 0x0FFFU);

    /* Return to Idle Grounding */
    TOUCH_GPIO_BASE->PCOR = (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);
    TOUCH_GPIO_BASE->PDDR |= (1U << TOUCH_PIN_DRV_A) | (1U << TOUCH_PIN_SENSE_B);

    EnableAllInterrupts();

    diff = (int16_t)v14 - (int16_t)v15;
    return (diff >= 0) ? diff : -diff;
}

void Touch_M1_NOP_Sweep(Touch_M1_Report_t *report)
{
    const uint8_t nop_candidates[4] = {4U, 8U, 12U, 16U};
    uint8_t best_nop = 8U;
    int16_t min_diff = 9999;

    for (uint8_t i = 0U; i < 4U; i++)
    {
        /* Average 5 balance measurements for stability */
        int32_t acc = 0;
        for (uint8_t rep = 0U; rep < 5U; rep++)
        {
            acc += (int32_t)Touch_MeasureBalanceDiff(nop_candidates[i]);
        }
        int16_t avg_diff = (int16_t)(acc / 5);
        report->nop_balance_diff[i] = avg_diff;

        if (avg_diff < min_diff)
        {
            min_diff = avg_diff;
            best_nop = nop_candidates[i];
        }
    }

    report->optimal_nop = best_nop;
    report->min_balance_diff = min_diff;
}
