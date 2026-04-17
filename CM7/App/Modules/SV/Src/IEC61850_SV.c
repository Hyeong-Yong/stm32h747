/*
 * IEC61850_SV.c
 *
 *  Created on: Oct 1, 2024
 *      Author: Professional
 */

#include "IEC61850_SV.h"
#include "stm32h7xx_hal.h"

#include "FreeRTOS.h"
#include "sv_publisher.h"
#include "task.h"

#include "semphr.h"

#include "cmsis_os.h"
#include "stack_config.h"

#include "adc.h"


/* --- 추가/수정된 정의 --- */
extern ADC_HandleTypeDef hadc3;
extern TIM_HandleTypeDef htim15;

// ADC_BUF_SIZE는 (ASDU_NUM * 2) 이상이어야 핑퐁이 가능합니다.
extern __IO uint16_t adcBuf_3[ASDU_NUM *2]; 

// Semaphore to emulate 60 Hz signal captured with 5400 Hz sample rate (90 samples per period)
osSemaphoreId_t signalSemHandle;
__IO uint16_t* activeAdcPtr = NULL; // 현재 전송 대기 중인 버퍼 포인터

// SV publisher object 및 설정
SVPublisher svPublisher;
char svId[8][12] = {"svpub1_test\0"};

const osSemaphoreAttr_t signalSem_attr = { .name = "signalSemaphore" };


// SV frame assembling and publishing task
void IEC61850_SV_Task(void *argument) {

	/*------------- Socket Configuration BEGIN -------------*/

	// Create a signal semaphore
	signalSemHandle = osSemaphoreNew(1, 0, &signalSem_attr);
	// Ethernet interface name in LWIP for STM32
	char* interface = "st0";
	// Destination MAC address
	uint8_t macAddr[6] = {0x01, 0x0C, 0xCD, 0x04, 0x00, 0x01};
	// SV frame parameters configuration
	CommParameters frameParam;
	frameParam.vlanId = CONFIG_SV_CUSTOM_VLAN_ID;
	frameParam.vlanPriority = CONFIG_SV_CUSTOM_PRIORITY;
	frameParam.appId = CONFIG_SV_CUSTOM_APPID;
	for (int i = 0; i < 6; i++) frameParam.dstAddress[i] = macAddr[i];
	// Create the SV Publisher object
	svPublisher = SVPublisher_create(&frameParam, interface);

	/*------------- Configuration END  -------------*/

	//SV frame ASDUs configuration
	if (svPublisher) {

		// ASDU info structure
		ASDU_Inf ASDUs[ASDU_NUM];
		// Samples (points) per second
		uint16_t smpCntMax = SIGNAL_FREQ * POINTS_PER_PERIOD;
		// Sample (point) counter
        uint16_t smpCnt = 0;

        for (uint8_t i = 0; i < ASDU_NUM; ++i) {
		  // Adding ASDUs to svpublisher
		  ASDUs[i].asdu = SVPublisher_addASDU(svPublisher, svId[0], NULL, 1);

		  // Adding 4 currents with quality (Q) and 4 voltages with Q
	  	  // To add more data, use corresponding functions (addFLOAT, addINT etc. from sv_publisher.h)
		  
      	  ASDUs[i].CurrentA = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);	// Current
      	  ASDUs[i].CurrA_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);	// Quality
      	  ASDUs[i].CurrentB = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].CurrB_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].CurrentC = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].CurrC_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].CurrentN = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].CurrN_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);

      	  ASDUs[i].VoltageA = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);	// Voltage
      	  ASDUs[i].VoltA_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);	// Quality
      	  ASDUs[i].VoltageB = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].VoltB_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].VoltageC = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].VoltC_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].VoltageN = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].VoltN_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);

		  // Set synchronization status (in a real application it should be changed every time synch status changes)
		  SVPublisher_ASDU_setSmpSynch(ASDUs[i].asdu, SV_SMPSYNCH_GLOBAL);
        }

        SVPublisher_setupComplete(svPublisher);

        if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adcBuf_3, ADC_BUF_SIZE) != HAL_OK)
           logPrintf("ADC_Start_DMA failed\n");

		HAL_TIM_Base_Start(&htim15);
        // You can add any condition of SV publisher operation
        while (1) {
        	if(osSemaphoreAcquire(signalSemHandle , portMAX_DELAY) == osOK) {
				for (uint8_t ASDU_num = 0; ASDU_num < ASDU_NUM; ++ASDU_num) {

					// 1. ADC Raw 값을 물리량으로 변환 (예: 16bit ADC, 3.3V 기준)
                    // 여기서는 activeAdcPtr에서 순차적으로 데이터를 가져옵니다.
                    // 단일 채널이므로 i번째 샘플을 사용합니다.
					uint16_t rawValue = activeAdcPtr[ASDU_num];
                    float currentVal = ((float)rawValue * 3.3f / 65535.0f) * 100.0f; // 스케일링 예시

					// 2. Current A에만 실제 데이터 입력
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentA, currentVal);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrA_Q, QUALITY_VALIDITY_GOOD);

                    // 3. 나머지 채널은 0.0f로 초기화
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentB, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrB_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentC, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrC_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentN, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrN_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageA, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltA_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageB, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltB_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageC, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltC_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageN, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltN_Q, QUALITY_VALIDITY_GOOD);

					// Update sample counter
					SVPublisher_ASDU_setSmpCnt(ASDUs[ASDU_num].asdu, smpCnt);
					
					smpCnt++;
					if (smpCnt >= smpCntMax) 
						smpCnt = 0;
				}
				// Publish SV frame
				SVPublisher_publish(svPublisher);
        	}
        }
	}
}