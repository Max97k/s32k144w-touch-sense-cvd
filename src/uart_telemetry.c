/*==================================================================================================
*   Project              : S32K144W Touch Sensing CVD Algorithm
*   Platform             : CORTEXM
*   Peripheral           : LPUART1
*   Dependencies         : S32K144W.h
*
*   Autosar Version      : 4.7.0
*   SW Version           : 1.0.0
*
*   Copyright 2026 NXP / Max97k
*   All Rights Reserved.
==================================================================================================*/

#include "uart_telemetry.h"
#include "S32K144W.h"

#define TIMEOUT_GUARD_CYCLES  (100000U)

void Telemetry_Init(uint32_t baudrate)
{
    (void)baudrate; /* Configured for nominal 115200 bps from 48MHz FIRC */

    /* Enable PORTC clock gate */
    IP_PCC->PCCn[PCC_PORTC_INDEX] |= PCC_PCCn_CGC_MASK;

    /* Configure OpenSDA Virtual COM on XS32K14WEVB-Q064:
     * PTC8 = ALT2 (LPUART1_RX), PTC9 = ALT2 (LPUART1_TX) */
    IP_PORTC->PCR[TELEMETRY_UART_TX_PIN] = PORT_PCR_MUX(2U);
    IP_PORTC->PCR[TELEMETRY_UART_RX_PIN] = PORT_PCR_MUX(2U);

    /* Also configure PTC6 (RX) / PTC7 (TX) for J5 header expansion */
    IP_PORTC->PCR[TELEMETRY_UART_EXP_TX] = PORT_PCR_MUX(2U);
    IP_PORTC->PCR[TELEMETRY_UART_EXP_RX] = PORT_PCR_MUX(2U);

    /* Clock source selection: PCS=3 -> FIRCDIV2 (48.0 MHz) */
    IP_PCC->PCCn[PCC_LPUART1_INDEX] &= ~PCC_PCCn_CGC_MASK;
    IP_PCC->PCCn[PCC_LPUART1_INDEX] = PCC_PCCn_PCS(3U) | PCC_PCCn_CGC_MASK;

    /* Reset control and baud registers */
    IP_LPUART1->CTRL = 0U;
    IP_LPUART1->STAT = 0xC01FC000U; /* Clear all status flags */

    /*
     * Baud Rate calculation:
     * Clock = 48,000,000 Hz
     * OSR = 25 (26x oversampling)
     * SBR = 48,000,000 / (26 * 115,200) = 16.0256 -> SBR = 16
     * Actual Baud = 48,000,000 / (26 * 16) = 115,384 bps (Error: +0.16%)
     */
    IP_LPUART1->BAUD = LPUART_BAUD_OSR(25U) | LPUART_BAUD_SBR(16U);

    /* Enable Transmitter and Receiver */
    IP_LPUART1->CTRL = LPUART_CTRL_TE_MASK | LPUART_CTRL_RE_MASK;
}

void Telemetry_SendChar(char c)
{
    uint32_t guard = 0U;

    /* Auto-carriage-return before newline */
    if (c == '\n')
    {
        while ((!(IP_LPUART1->STAT & LPUART_STAT_TDRE_MASK)) && (guard < TIMEOUT_GUARD_CYCLES))
        {
            guard++;
        }
        if (guard < TIMEOUT_GUARD_CYCLES)
        {
            IP_LPUART1->DATA = (uint32_t)'\r';
        }
        guard = 0U;
    }

    /* Bounded wait for Transmit Data Register Empty (TDRE) */
    while ((!(IP_LPUART1->STAT & LPUART_STAT_TDRE_MASK)) && (guard < TIMEOUT_GUARD_CYCLES))
    {
        guard++;
    }

    if (guard < TIMEOUT_GUARD_CYCLES)
    {
        IP_LPUART1->DATA = (uint32_t)((uint8_t)c);
    }
}

void Telemetry_SendString(const char *str)
{
    if (str != NULL_PTR)
    {
        while (*str != '\0')
        {
            Telemetry_SendChar(*str);
            str++;
        }
    }
}

void Telemetry_SendUInt(uint32_t value)
{
    char buf[12];
    int idx = 0;

    if (value == 0U)
    {
        Telemetry_SendChar('0');
        return;
    }

    while (value > 0U)
    {
        buf[idx++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (idx > 0)
    {
        Telemetry_SendChar(buf[--idx]);
    }
}

void Telemetry_SendInt(int32_t value)
{
    if (value < 0)
    {
        Telemetry_SendChar('-');
        Telemetry_SendUInt((uint32_t)(-value));
    }
    else
    {
        Telemetry_SendUInt((uint32_t)value);
    }
}

void Telemetry_SendFloat(float value, uint8_t decimals)
{
    if (value < 0.0f)
    {
        Telemetry_SendChar('-');
        value = -value;
    }

    uint32_t int_part = (uint32_t)value;
    Telemetry_SendUInt(int_part);

    if (decimals > 0U)
    {
        Telemetry_SendChar('.');
        float frac = value - (float)int_part;
        for (uint8_t i = 0U; i < decimals; i++)
        {
            frac *= 10.0f;
            uint32_t digit = (uint32_t)frac;
            Telemetry_SendChar((char)('0' + digit));
            frac -= (float)digit;
        }
    }
}
