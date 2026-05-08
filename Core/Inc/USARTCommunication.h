/*
 * USARTCommunication.h
 *
 *  Created on: Jul 14, 2025
 *      Author: wawer
 */

#ifndef INC_USARTCOMMUNICATION_H_
#define INC_USARTCOMMUNICATION_H_

#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include "usart.h"

#define Channel_USART &huart1

#define RX_BUFFER_SIZE 200U
#define TX_BUFFER_SIZE 200U

extern volatile bool isTransmissionComplete;
extern uint8_t txBuffer[TX_BUFFER_SIZE];

void InitUSART(void);
HAL_StatusTypeDef Sendf(const char *fmt, ...);
HAL_StatusTypeDef Send(const char *text);

#endif /* INC_USARTCOMMUNICATION_H_ */
