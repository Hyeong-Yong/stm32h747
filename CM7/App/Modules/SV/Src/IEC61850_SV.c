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

/* --- 추가/수정된 정의 --- */
extern ADC_HandleTypeDef hadc3;
extern TIM_HandleTypeDef htim15;
extern  __attribute__((section(".non_cache"))) __IO uint16_t adcBuf_3[ASDU_NUM]; 

/* Definitions for svDataQueue */
osMessageQueueId_t svDataQueueHandle;
const osMessageQueueAttr_t svDataQueue_attributes = {
  .name = "svDataQueue"
};

// SV publisher object
SVPublisher svPublisher;

// SVIds
char svId[8][12] = {"OCT_DATA_1\0", "OCT_DATA_2\0", "OCT_DATA_3\0", "NONE\0",
					"OVT_DATA_1\0", "OVT_DATA_2\0", "OVT_DATA_3\0", "NONE\0"};

// SV frame assembling and publishing task
void IEC61850_SV_Task(void *argument) {
  	/* Create the queue(s) */
  	/* creation of svDataQueue */
  	svDataQueueHandle = osMessageQueueNew (5, ASDU_NUM *sizeof(uint16_t), &svDataQueue_attributes);

	// Ethernet interface name in LWIP for STM32
	char* interface = "st0";

	// Destination MAC address
	uint8_t macAddr[6] = {0x01, 0x0C, 0xCD, 0x04, 0x00, 0x01};

	// SV frame parameters configuration
	CommParameters frameParam;
	frameParam.vlanId = CONFIG_SV_CUSTOM_VLAN_ID;
	frameParam.vlanPriority = CONFIG_SV_CUSTOM_PRIORITY;
	frameParam.appId = CONFIG_SV_CUSTOM_APPID;
	memcpy(frameParam.dstAddress, macAddr, 6);

	// Create the SV Publisher object
	svPublisher = SVPublisher_create(&frameParam, interface);

	//SV frame ASDUs configuration
	if (svPublisher) {

		// ASDU info structure
		ASDU_Inf ASDUs[ASDU_NUM];

		// Samples (points) per second
		uint16_t smpCntMax = SIGNAL_FREQ * POINTS_PER_PERIOD;

		// Adding ASDUs to svpublisher
	  	for (uint8_t i = 0; i < ASDU_NUM; ++i) {
	  		ASDUs[i].asdu = SVPublisher_addASDU(svPublisher, svId[0], NULL, 1);
	  	}

	  	// Adding 4 currents with quality (Q) and 4 voltages with Q
	  	// To add more data, use corresponding functions (addFLOAT, addINT etc. from sv_publisher.h)
        for (uint8_t i = 0; i < ASDU_NUM; ++i) {

      	  ASDUs[i].CurrentA = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);		// Current
      	  ASDUs[i].CurrA_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);	// Quality
      	  ASDUs[i].CurrentB = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].CurrB_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].CurrentC = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].CurrC_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].CurrentN = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].CurrN_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].VoltageA = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);		// Voltage
      	  ASDUs[i].VoltA_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);	// Quality
      	  ASDUs[i].VoltageB = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].VoltB_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].VoltageC = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].VoltC_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
      	  ASDUs[i].VoltageN = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);
      	  ASDUs[i].VoltN_Q = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
        }

        SVPublisher_setupComplete(svPublisher);

        // Sample (point) counter
        uint16_t smpCnt = 0;

        // Start timer to simulate signals
        HAL_TIM_Base_Start_IT(&htim15);

        // Set synchronization status (in a real application it should be changed every time synch status changes)
		for (uint8_t i = 0; i < ASDU_NUM; ++i) {
			SVPublisher_ASDU_setSmpSynch(ASDUs[i].asdu, SV_SMPSYNCH_GLOBAL);
		}

		uint16_t localAdcBuf[ASDU_NUM];

		while (1) {
		if (osMessageQueueGet(svDataQueueHandle, localAdcBuf, NULL, osWaitForever) == osOK) {
				for (uint8_t ASDU_num = 0; ASDU_num < ASDU_NUM; ++ASDU_num) {

					float currentVal = ((float)localAdcBuf[ASDU_num] * 3.3f / 65535.0f);
					
					// Setting currents values in the frame
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentA, currentVal);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrA_Q, QUALITY_VALIDITY_GOOD);
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentB, 0);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrB_Q, QUALITY_VALIDITY_GOOD);
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentC, 0);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrC_Q, QUALITY_VALIDITY_GOOD);
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentN, 0);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrN_Q, QUALITY_VALIDITY_GOOD);

					// Setting voltages values in the frame
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageA, 0);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltA_Q, QUALITY_VALIDITY_GOOD);
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageB, 0);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltB_Q, QUALITY_VALIDITY_GOOD);
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageC, 0);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltC_Q, QUALITY_VALIDITY_GOOD);
					SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageN, 0);
					SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltN_Q, QUALITY_VALIDITY_GOOD);

					// Update sample counter
					SVPublisher_ASDU_setSmpCnt(ASDUs[ASDU_num].asdu, smpCnt);
					smpCnt++;
					if (smpCnt >= smpCntMax) {
						smpCnt = 0;
					}

				}
				// Publish SV frame
				SVPublisher_publish(svPublisher);

        	}
			
        }

	}

}