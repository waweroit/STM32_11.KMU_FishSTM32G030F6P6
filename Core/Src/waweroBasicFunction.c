/*
 * waweroBasicFunction.c
 *
 *  Created on: Feb 4, 2024
 *      Author: wawer
 */
#include "main.h"

#include "waweroBasicFunction.h"

//ralated to  delayUS ------------------------------------------------- START
void DelayInit(void)
{
    HAL_TIM_Base_Start(&htim14);
}

void DelayUS(uint32_t us) {
    __HAL_TIM_SET_COUNTER(&htim14, 0);  // reset licznika

    while (__HAL_TIM_GET_COUNTER(&htim14) < us);  // czekaj
}

//ralated to  delayUS ------------------------------------------------- END
