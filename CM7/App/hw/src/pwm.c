/*
 * tim.c
 *
 *  Created on: 2024. 9. 15.
 *      Author: User
 */

#include "pwm.h"
#include "cli.h"

#ifdef _USE_HW_PWM

typedef struct {
	TIM_HandleTypeDef *h_tim;
	uint32_t channel;
	uint32_t period;
	uint32_t pulse;
	bool is_timeInit;
	bool is_pwmInit;
} pwm_t;

TIM_HandleTypeDef htim1;

#define ABP_CLK 96000000
#define fM	 2000000
// 1count 500ns ICG : 10, SH : 4
// 2Mhz : 500ns*1480 = 7400us
// 7400 ms n=

// when onePulseMode, period is pulse width and pulse is delay between start point and pulse starting point
static pwm_t pwm_tbl[PWM_MAX_CH] = {
		{ &htim1, TIM_CHANNEL_1, 100, 1},
		};

bool pwmInit(void) {
	bool ret = true;

	for (int i = 0; i < PWM_MAX_CH; i++) {
		pwm_tbl[i].is_timeInit = false;
		pwm_tbl[i].is_pwmInit = false;
		pwmBegin(i, pwm_tbl[i].period);
	}

	return ret;
}

bool pwmBegin(uint8_t ch, uint32_t period) {
	bool ret = false;

	if (ch >= PWM_MAX_CH)
		return false;

	pwm_t *p_pwm = &pwm_tbl[ch];
	p_pwm->period = period - 1;

	  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	  TIM_MasterConfigTypeDef sMasterConfig = {0};
	  TIM_OC_InitTypeDef sConfigOC = {0};
	  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

	switch (ch) {
	case _DEF_PWM1:
		p_pwm->h_tim = &htim1;



		  /* USER CODE BEGIN TIM1_Init 1 */

		  /* USER CODE END TIM1_Init 1 */
		  htim1.Instance = TIM1;
		  htim1.Init.Prescaler = 1-1;
		  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
		  htim1.Init.Period = p_pwm->period;
		  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
		  htim1.Init.RepetitionCounter = 0;
		  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
		  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
		  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  if (HAL_TIM_OnePulse_Init(&htim1, TIM_OPMODE_SINGLE) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
		  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
		  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  sConfigOC.OCMode = TIM_OCMODE_PWM2;
		  sConfigOC.Pulse = p_pwm->pulse;
		  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
		  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
		  sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
		  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
		  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
		  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
		  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
		  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
		  sBreakDeadTimeConfig.DeadTime = 0;
		  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
		  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
		  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
		  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  /* USER CODE BEGIN TIM1_Init 2 */

		  /* USER CODE END TIM1_Init 2 */
		  HAL_TIM_MspPostInit(&htim1);


		break;

	}
	ret = true;
	return ret;
// https://www.youtube.com/watch?v=3yvY7pLMHAg
}

void pwmAttachInterrupt(uint8_t channel, void (*func)(void)) {

}

void pwmDetachInterrupt(uint8_t channel) {

}


void pwmStart(uint8_t ch) {
	pwm_t *p_pwm = &pwm_tbl[ch];
	HAL_TIM_PWM_Start(p_pwm->h_tim, p_pwm->channel);
}


void pwmStart_IT(uint8_t ch) {
	pwm_t *p_pwm = &pwm_tbl[ch];
	HAL_TIM_PWM_Start_IT(p_pwm->h_tim, p_pwm->channel);
}


void pwmStart_onePulse(uint8_t ch) {
	pwm_t *p_pwm = &pwm_tbl[ch];
	HAL_TIM_OnePulse_Start(p_pwm->h_tim, p_pwm->channel);
}


void pwmStop_onePulse(uint8_t ch) {
	pwm_t *p_pwm = &pwm_tbl[ch];
	HAL_TIM_OnePulse_Stop(p_pwm->h_tim, p_pwm->channel);
}


bool pwmDeinit(uint8_t ch) {
	bool ret = false;
	if (ch >= PWM_MAX_CH)
		return ret;

	pwm_t *p_pwm = &pwm_tbl[ch];

	if (HAL_TIM_PWM_DeInit(p_pwm->h_tim) == HAL_OK) {
		p_pwm->is_pwmInit = false;
	}
	if (HAL_TIM_Base_DeInit(p_pwm->h_tim) == HAL_OK) {
		p_pwm->is_timeInit = false;
	}

	return true;
}

bool pwmStop_IT(uint8_t ch) {
	if (ch >= PWM_MAX_CH)
		return false;
	pwm_t *p_pwm = &pwm_tbl[ch];

	HAL_TIM_PWM_Stop_IT(p_pwm->h_tim, p_pwm->channel);

	return true;
}

void onePulseTrigger() {
	TIM1->CR1 |=TIM_CR1_CEN;
}

void pwmSycDelay(uint8_t ch, uint32_t delay) {
	pwm_t *p_pwm = &pwm_tbl[ch];
	// 1 clock is delayed when using encapulated function. To synchronize, add 1 clock
	p_pwm->h_tim->Instance->CNT = delay + 1;
}



#endif
