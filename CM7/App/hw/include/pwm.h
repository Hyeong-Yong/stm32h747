/*
 * tim.h
 *
 *  Created on: 2024. 9. 15.
 *      Author: User
 */

#ifndef A66143AA_E312_41E6_9E2C_1A87D1DB9FFC
#define A66143AA_E312_41E6_9E2C_1A87D1DB9FFC

#ifndef SRC_COMMON_HW_INCLUDE_PWM_H_
#define SRC_COMMON_HW_INCLUDE_PWM_H_


#include "hw_def.h"

#define PWM_MAX_CH	HW_PWM_MAX_CH


#ifdef _USE_HW_PWM


bool pwmInit(void);
bool pwmBegin(uint8_t ch, uint32_t period);
bool pwmDeinit(uint8_t ch);

void pwmStart(uint8_t ch);
void pwmStart_onePulse(uint8_t ch);
void pwmStart_IT(uint8_t ch);
void pwmStop_onePulse(uint8_t ch);
bool pwmStop_IT(uint8_t ch);
void pwmSycDelay(uint8_t ch, uint32_t delay);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
void onePulseTrigger();



#endif

#endif /* SRC_COMMON_HW_INCLUDE_PWM_H_ */


#endif /* A66143AA_E312_41E6_9E2C_1A87D1DB9FFC */
