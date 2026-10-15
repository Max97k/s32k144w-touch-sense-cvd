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

#ifndef UART_TELEMETRY_H
#define UART_TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "StandardTypes.h"
#include <stdint.h>

/* OpenSDA Virtual COM Port LPUART1 Pins on XS32K14WEVB-Q064 */
#define TELEMETRY_UART_PORT      IP_PORTC
#define TELEMETRY_UART_TX_PIN    (9U)   /* PTC9 -> OpenSDA LPUART1_TX (ALT2) */
#define TELEMETRY_UART_RX_PIN    (8U)   /* PTC8 -> OpenSDA LPUART1_RX (ALT2) */
#define TELEMETRY_UART_EXP_TX    (7U)   /* PTC7 -> J5 Header LPUART1_TX (ALT2) */
#define TELEMETRY_UART_EXP_RX    (6U)   /* PTC6 -> J5 Header LPUART1_RX (ALT2) */

/*!
 * @brief Initialize LPUART1 for OpenSDA ASCII telemetry.
 * @param[in] baudrate Nominal baudrate (e.g. 115200).
 */
void Telemetry_Init(uint32_t baudrate);

/*!
 * @brief Transmit a single character over LPUART1.
 */
void Telemetry_SendChar(char c);

/*!
 * @brief Transmit a null-terminated string over LPUART1.
 */
void Telemetry_SendString(const char *str);

/*!
 * @brief Transmit a signed 32-bit integer formatted as ASCII.
 */
void Telemetry_SendInt(int32_t value);

/*!
 * @brief Transmit an unsigned 32-bit integer formatted as ASCII.
 */
void Telemetry_SendUInt(uint32_t value);

/*!
 * @brief Transmit a floating-point value formatted with decimal precision.
 */
void Telemetry_SendFloat(float value, uint8_t decimals);

#ifdef __cplusplus
}
#endif

#endif /* UART_TELEMETRY_H */
