/*
 * USARTCommunication.c
 *
 *  Created on: Jul 14, 2025
 *      Author: wawer
 */

#include "USARTCommunication.h"

#include <stdio.h>

volatile bool isTransmissionComplete = true;
uint8_t txBuffer[TX_BUFFER_SIZE];

void InitUSART(void)
{
    HAL_UART_DeInit(Channel_USART);
    HAL_Delay(100);
    MX_USART1_UART_Init();
}

static HAL_StatusTypeDef Send_IT_internal(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (data == NULL) {
        return HAL_ERROR;
    }

    if (len == 0U) {
        return HAL_OK;
    }

    if (len > sizeof(txBuffer)) {
        len = sizeof(txBuffer);
    }

    /*
     * HAL_UART_Transmit_IT() korzysta z bufora po powrocie z funkcji,
     * dlatego dane muszą znajdować się w stabilnym buforze globalnym.
     */
    if (data != txBuffer) {
        memcpy(txBuffer, data, len);
    }

    isTransmissionComplete = false;
    HAL_StatusTypeDef status = HAL_UART_Transmit_IT(Channel_USART, txBuffer, (uint16_t)len);
    if (status != HAL_OK) {
        isTransmissionComplete = true;
        return status;
    }

    const uint32_t startTick = HAL_GetTick();
    while (!isTransmissionComplete) {
        if ((HAL_GetTick() - startTick) >= timeout_ms) {
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef Sendf(const char *fmt, ...)
{
    if (fmt == NULL) {
        return HAL_ERROR;
    }

    va_list args;
    va_start(args, fmt);
    const int written = vsnprintf((char *)txBuffer, sizeof(txBuffer), fmt, args);
    va_end(args);

    if (written < 0) {
        return HAL_ERROR;
    }

    const size_t len = (written < (int)sizeof(txBuffer)) ? (size_t)written : sizeof(txBuffer);
    return Send_IT_internal(txBuffer, len, 1000U);
}

HAL_StatusTypeDef Send(const char *text)
{
    if (text == NULL) {
        return HAL_ERROR;
    }

    return Sendf("%s", text);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        isTransmissionComplete = true;
    }
}
