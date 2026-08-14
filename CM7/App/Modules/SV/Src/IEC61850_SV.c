/*
 * IEC61850_SV.c
 */

#include "IEC61850_SV.h"
#include "stm32h7xx_hal.h"
#include "hw_def.h"
#include "FreeRTOS.h"
#include "sv_publisher.h"
#include "task.h"
#include "semphr.h"
#include "cmsis_os.h"
#include "stack_config.h"

// adc.c와 상호작용하기 위한 통합 데이터 패킷 구조체 재선언
typedef struct {
  uint16_t adc1[3 * ASDU_NUM];
  uint16_t adc2[3 * ASDU_NUM];
  uint16_t adc3[4 * ASDU_NUM];
} AdcDataPacket;


extern TIM_HandleTypeDef htim15;


/* Definitions for svDataQueue */
osMessageQueueId_t svDataQueueHandle;
const osMessageQueueAttr_t svDataQueue_attributes = {
  .name = "svDataQueue"
};

// SV publisher object
SVPublisher svPublisher;

// SVIds (ASDU_NUM = 8 기준)
char svId[8][12] = {"OCT_Ia\0", "OCT_Ib\0", "OCT_Ic\0", "NONE\0",
                    "OVT_Va\0", "OVT_Vb\0", "OVT_Vc\0", "NONE\0"};

// SV frame assembling and publishing task
void IEC61850_SV_Task(void *argument) {
    /* 기존 큐 생성을 새로운 패킷 크기(AdcDataPacket)에 맞추어 할당.
     * 깊이 16(≈8.3ms 버퍼) → 64(≈33.3ms 버퍼)로 확장: 이더넷 TX가 순간적으로
     * 막혀도(디스크립터 부족 등) 드롭 대신 지연으로 흡수할 여유를 늘린다.
     * sizeof(AdcDataPacket)=160B 기준 64개 = 10.24KB, RAM 여유상 문제없음. */
    svDataQueueHandle = osMessageQueueNew (64, sizeof(AdcDataPacket), &svDataQueue_attributes);

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

    if (svPublisher == NULL) {
        logPrintf("[ERROR] SVPublisher_create failed\n");
        vTaskDelete(NULL);
        return;
    }

    // SV frame ASDUs configuration
    {
        ASDU_Inf ASDUs[ASDU_NUM];
        uint16_t smpCntMax = SIGNAL_FREQ * POINTS_PER_PERIOD;

        // Adding ASDUs to svpublisher
        for (uint8_t i = 0; i < ASDU_NUM; ++i) {
            ASDUs[i].asdu = SVPublisher_addASDU(svPublisher, svId[i], NULL, 1);
        }

        // Adding currents & voltages data fields with quality (Q)
        for (uint8_t i = 0; i < ASDU_NUM; ++i) {
          ASDUs[i].CurrentA = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Current A
          ASDUs[i].CurrA_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);    // Quality
          ASDUs[i].CurrentB = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Current B
          ASDUs[i].CurrB_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
          ASDUs[i].CurrentC = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Current C
          ASDUs[i].CurrC_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
          ASDUs[i].CurrentN = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Neutral Current
          ASDUs[i].CurrN_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
          
          ASDUs[i].VoltageA = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Voltage A
          ASDUs[i].VoltA_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);    // Quality
          ASDUs[i].VoltageB = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Voltage B
          ASDUs[i].VoltB_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
          ASDUs[i].VoltageC = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Voltage C
          ASDUs[i].VoltC_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
          ASDUs[i].VoltageN = SVPublisher_ASDU_addFLOAT(ASDUs[i].asdu);     // Neutral Voltage
          ASDUs[i].VoltN_Q  = SVPublisher_ASDU_addQuality(ASDUs[i].asdu);
        }

        SVPublisher_setupComplete(svPublisher);

        // Sample counter
        uint16_t smpCnt = 0;

        // Start timer to simulate signals
        HAL_TIM_Base_Start_IT(&htim15);

        // Set synchronization status
        for (uint8_t i = 0; i < ASDU_NUM; ++i) {
            SVPublisher_ASDU_setSmpSynch(ASDUs[i].asdu, SV_SMPSYNCH_GLOBAL);
        }

        // 수신 버퍼용 구조체 변수 선언
        AdcDataPacket localPacket;

        // 평균화 연산 결과를 저장할 새로운 변수 배열 (ASDU_NUM = 8 기준 크기 8)
        float octA[ASDU_NUM];
        float octB[ASDU_NUM];
        float octC[ASDU_NUM];
        float ovtA[ASDU_NUM];

        while (1) {
            if (osMessageQueueGet(svDataQueueHandle, &localPacket, NULL, osWaitForever) == osOK) {
                
                // 1. 신호 처리 프로세스 (평균화 연산 및 전압/전류 물리값 변환 연산 통합)
for (uint8_t i = 0; i < ASDU_NUM; ++i) {
    // ADC1: 같은 순간(i)의 3채널(ch0, ch1, ch16) 평균
    //uint32_t sumA = localPacket.adc1[i*3 + 0]+ localPacket.adc1[i*3 + 1]+ localPacket.adc1[i*3 + 2];
    octA[i] = ((float)localPacket.adc1[i*3 + 0] / 3.0f) * 3.3f / 65535.0f;

    // ADC2: 같은 순간(i)의 3채널(ch3, ch12, ch13) 평균
    //uint32_t sumB = localPacket.adc2[i*3 + 0]+ localPacket.adc2[i*3 + 1]+ localPacket.adc2[i*3 + 2];
    octB[i] = ((float)localPacket.adc1[i*3 + 1] / 3.0f) * 3.3f / 65535.0f;

    // ADC3: 같은 순간(i)의 1~3채널(ch2, ch6, ch7) 평균
    //uint32_t sumC = localPacket.adc3[i*4 + 0]+ localPacket.adc3[i*4 + 1]+ localPacket.adc3[i*4 + 2];
    octC[i] = ((float)localPacket.adc1[i*3 + 2] / 3.0f) * 3.3f / 65535.0f;

    // ADC3: 같은 순간(i)의 4채널(ch8)은 평균 없이 그대로 전송
    ovtA[i] = ((float)localPacket.adc3[i*4 + 3]) * 3.3f / 65535.0f;
}

                // 2. 연산된 데이터를 IEC61850 ASDU 프레임 데이터 필드에 각각 매핑 및 할당
                for (uint8_t ASDU_num = 0; ASDU_num < ASDU_NUM; ++ASDU_num) {

                    // 매핑: octA -> CurrentA, octB -> CurrentB, octC -> CurrentC, ovtA -> VoltageA
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentA, octA[ASDU_num]);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrA_Q, QUALITY_VALIDITY_GOOD);

                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentB, octB[ASDU_num]);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrB_Q, QUALITY_VALIDITY_GOOD);

                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentC, octC[ASDU_num]);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrC_Q, QUALITY_VALIDITY_GOOD);
                    
                    // 나머지 미사용 전류 채널(CurrentN)은 0으로 유지
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrentN, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].CurrN_Q, QUALITY_VALIDITY_GOOD);

                    // 전압 채널 매핑: ovtA -> VoltageA
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageA, ovtA[ASDU_num]);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltA_Q, QUALITY_VALIDITY_GOOD);
                    
                    // 나머지 미사용 전압 채널(B, C, N)들은 0으로 유지
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageB, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltB_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageC, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltC_Q, QUALITY_VALIDITY_GOOD);
                    SVPublisher_ASDU_setFLOAT(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltageN, 0.0f);
                    SVPublisher_ASDU_setQuality(ASDUs[ASDU_num].asdu, ASDUs[ASDU_num].VoltN_Q, QUALITY_VALIDITY_GOOD);

                    // Update sample counter
                    SVPublisher_ASDU_setSmpCnt(ASDUs[ASDU_num].asdu, smpCnt);
                    smpCnt++;
                    if (smpCnt >= smpCntMax) {
                        smpCnt = 0;
                    }
                }
                
                // Publish 완성이 완료된 통신 패킷 송신
                SVPublisher_publish(svPublisher);
            }
        }
    }
}