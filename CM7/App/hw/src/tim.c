#include "tim.h"

extern TIM_HandleTypeDef htim2;

/* TIM2 init function */
void timInit(void)
{

}


//TIMER2 : 100MHz/100 = 1Mhz => 1 us
void delay_us(uint32_t us){
	//__HAL_TIM_SET_COUNTER(&htim2, 0);
	htim2.Instance->CNT = 0;
	while(htim2.Instance->CNT<us);
}


void timStart(){
	HAL_TIM_Base_Start(&htim2);
}
