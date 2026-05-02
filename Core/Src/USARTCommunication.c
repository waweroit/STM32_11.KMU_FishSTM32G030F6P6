/*
 * USARTCommunication.c
 *
 *  Created on: Jul 14, 2025
 *      Author: wawer
 */

#include "USARTCommunication.h"
#

volatile bool isTransmissionComplete = false;
uint8_t txBuffer[TX_BUFFER_SIZE];

void InitUSART()
{
	HAL_UART_DeInit(Channel_USART);
//	DelayUS(500);
	HAL_Delay(100);
	MX_USART1_UART_Init();

	//HAL_UART_Receive_IT(Channel_USART, (uint8_t *)&rxData, 1);  // Włącz przerwania RX
}


static HAL_StatusTypeDef Send_IT_internal(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
    if (len == 0) return HAL_OK;
    if (len > sizeof(txBuffer)) len = sizeof(txBuffer);

    // Jeżeli źródło nie jest naszym buforem – skopiuj, aby ISR miał stabilne dane
    if (data != txBuffer) {
        memcpy(txBuffer, data, len);
    }

    isTransmissionComplete = false;
    HAL_StatusTypeDef st = HAL_UART_Transmit_IT(Channel_USART, txBuffer, len);
    if (st != HAL_OK) return st;

    uint32_t t0 = HAL_GetTick();
    while (!isTransmissionComplete) {
        if ((HAL_GetTick() - t0) >= timeout_ms) {
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
}

HAL_StatusTypeDef Sendf(const char *fmt, ...)
{
    if (!fmt) return HAL_ERROR;

    // Formatujemy bezpośrednio do txBuffer, żeby uniknąć dodatkowej kopii
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf((char*)txBuffer, sizeof(txBuffer), fmt, ap);
    va_end(ap);

    if (n < 0) return HAL_ERROR;  // błąd formatowania
    size_t len = (n < (int)sizeof(txBuffer)) ? (size_t)n : sizeof(txBuffer); // ewentualne ucięcie

    return Send_IT_internal(txBuffer, len, 1000);
}

HAL_StatusTypeDef Send(const char *text)
{
    if (!text) return HAL_ERROR;
    return Sendf("%s", text);  // alias
}


//
//HAL_StatusTypeDef Send(char *text)
//{
////    HAL_StatusTypeDef retStatus = HAL_UART_Transmit(ESP_USART, (uint8_t*)command, strlen(command), HAL_MAX_DELAY);
//    HAL_StatusTypeDef retStatus = HAL_UART_Transmit_IT(Channel_USART, (uint8_t*)text, strlen(text));
//
//    if (retStatus == HAL_OK)
//    {
//    	int breakTimeStart = HAL_GetTick();
//    	int breakAfterMS = 10000;
//    	int breakTimeNOW = 0;
//
//        while (!isTransmissionComplete)
//        {
//        	breakTimeNOW = HAL_GetTick();
//
//    		if((uint32_t)(breakTimeNOW - breakTimeStart) >= breakAfterMS)
//    		{
//    			// send timeout
//    		}
//        }
//
//
//        isTransmissionComplete = false; // Reset flagi
//
//    }
//    else
//    {
//
//        // Obsłuż błąd transmisji
//    }
//    memset(text, 0, strlen(text));
//    return retStatus;
//}
