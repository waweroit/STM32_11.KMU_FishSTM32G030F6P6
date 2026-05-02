/*
 * waweroBasicFunction.h
 *
 *  Created on: october 27, 2024
 *      Author: wawer
 */
#pragma once
#ifndef INC_WAWEROBASICFUNCTION_H_
#define INC_WAWEROBASICFUNCTION_H_


extern TIM_HandleTypeDef htim14;


//ralated to  delayUS ------------------------------------------------- START
void DelayInit(void);
void DelayUS(uint32_t us);

#endif /* INC_WAWEROBASICFUNCTION_H_ */
