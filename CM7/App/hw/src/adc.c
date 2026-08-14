#include "adc.h"
#include "oct_calib.h"
#include "IEC61850_SV.h"


#ifdef _USE_HW_ADC     
#include "cli.h"



#include "FreeRTOS.h"
#include "semphr.h"
#include "cmsis_os.h"


typedef struct
{
  ADC_HandleTypeDef  *p_hadc;
  uint32_t            channel;  
  uint32_t            rank;  
  const char         *p_name;
} adc_tbl_t;

 #ifdef _USE_HW_CLI
 static void cliADC(cli_args_t *args);
 #endif
 


 /* ADC parameters */ 
 static bool is_init = false;

 __attribute__((section(".non_cache")))    __IO uint16_t adc1Buf[3*ASDU_NUM];
 __attribute__((section(".non_cache")))    __IO uint16_t adc2Buf[3*ASDU_NUM];
 __attribute__((section(".non_cache")))    __IO uint16_t adc3Buf[4*ASDU_NUM];


typedef struct {
  uint16_t adc1[3 * ASDU_NUM];
  uint16_t adc2[3 * ASDU_NUM];
  uint16_t adc3[4 * ASDU_NUM];
} AdcDataPacket;

 // USB MODE
 __attribute__((section(".non_cache")))    __IO uint32_t adcBuf_OVT[ADC_BUF_SIZE];
 __attribute__((section(".non_cache")))    __IO uint16_t adcBuf_OCT[ADC_BUF_SIZE];
 __attribute__((section(".non_cache")))    __IO uint16_t adcBuf_3[ASDU_NUM];
bool adc_OVT_Finished = true;
bool adc_OCT_Finished = true;

 extern ADC_HandleTypeDef hadc1;
 extern ADC_HandleTypeDef hadc2;
 extern ADC_HandleTypeDef hadc3;
 extern DMA_HandleTypeDef hdma_adc1;
 extern DMA_HandleTypeDef hdma_adc2;
 extern DMA_HandleTypeDef hdma_adc3;


 bool adcInit(void) {
     bool ret = true;
 
     if ( HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK) {
         ret = false;
     }
     if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc1Buf, 3*ASDU_NUM) != HAL_OK) {
         ret = false;
     }

     delay(1);

     if ( HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK) {
         ret = false;
     }
     if (HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2Buf, 3*ASDU_NUM) != HAL_OK) {
         ret = false;
     }
     delay(1);

     if ( HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK) {
         ret = false;
     }
     if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adc3Buf, 4*ASDU_NUM) != HAL_OK) {
         ret = false;
     }
     
     is_init = ret;

     logPrintf("[%s] adcInit()\n", is_init ? "OK":"NO");

 #ifdef _USE_HW_CLI
     cliAdd("adc", cliADC);
 #endif
     return ret;
 }
 
 
bool adcIsInit(void)
{
  return is_init;
}

 
bool adcOvtMeasure(void){
     bool ret = true;
 
     return ret;
 }

 
bool adcOctMeasure(void){
     bool ret = true;
 
     return ret;
 }
 

bool adc3Measure(void){
     bool ret = true;
 
     return ret;
 }
 
extern osMessageQueueId_t svDataQueueHandle;
#define _USE_ADC_TEST
#ifdef _USE_ADC_TEST

 // 디버깅용: 각 ADC의 ConvCplt/Error 콜백 호출 횟수를 눈으로 확인하기 위한 카운터.
 // 브레이크포인트로 멈추지 않고 watch/live-expression으로 계속 늘어나는지 확인할 수 있음.
 volatile uint32_t adc1CpltCnt = 0;
 volatile uint32_t adc2CpltCnt = 0;
 volatile uint32_t adc3CpltCnt = 0;

 volatile uint32_t adc1ErrCnt = 0;
 volatile uint32_t adc2ErrCnt = 0;
 volatile uint32_t adc3ErrCnt = 0;
 volatile uint32_t adc1LastErrorCode = 0;
 volatile uint32_t adc2LastErrorCode = 0;
 volatile uint32_t adc3LastErrorCode = 0;

 // SV 큐(svDataQueueHandle)로의 전달 성공/실패 카운터.
 // osMessageQueuePut(...,0,0)은 큐가 꽉 차면 대기하지 않고 그냥 실패를
 // 리턴하므로, 실패 시 그 샘플묶음(8개)은 이더넷으로 절대 나가지 못하고
 // 조용히 버려진다. 이 카운터로 실제 손실량을 정량적으로 확인한다.
 volatile uint32_t svPublishOkCnt   = 0;
 volatile uint32_t svPublishDropCnt = 0;

  void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *AdcHandle) {
      if ( AdcHandle->Instance == ADC1){
        adc1CpltCnt++;
        adcCalibNotifyConvCplt(AdcHandle);  // 캘리브레이션 캡처 중이면 완료 통지
        return;
      }
      else if ( AdcHandle->Instance == ADC2){
        adc2CpltCnt++;
        adcCalibNotifyConvCplt(AdcHandle);  // 캘리브레이션 캡처 중이면 완료 통지
        return ;
      }
      else if (AdcHandle->Instance == ADC3) {
        adc3CpltCnt++;
        if (adcCalibNotifyConvCplt(AdcHandle)) {
          // ADC3 캘리브레이션 캡처가 방금 끝난 경우: adc3Buf는 캘리브레이션
          // 동안 갱신되지 않았으므로(다른 버퍼로 캡처 중) 정상 SV 패킷
          // 조립은 건너뛴다.
          return;
        }
        if (adcCalibBusy) {
          // ADC1/ADC2/ADC3 중 아무거나 캘리브레이션(캡처+파라미터 계산)이
          // 진행 중인 동안은 이더넷으로 SV 패킷을 절대 내보내지 않는다.
          return;
        }
        static AdcDataPacket packet;
        
        // 메모리 비캐시 영역 또는 하드웨어 동기화를 고려하여 데이터를 로컬 패킷에 복사
        memcpy(packet.adc1, (const void*)adc1Buf, sizeof(adc1Buf));
        memcpy(packet.adc2, (const void*)adc2Buf, sizeof(adc2Buf));
        memcpy(packet.adc3, (const void*)adc3Buf, sizeof(adc3Buf));

        // 데이터 태스크(IEC61850_SV_Task)로 전달
        if (osMessageQueuePut(svDataQueueHandle, &packet, 0, 0) == osOK)
        {
          svPublishOkCnt++;
        }
        else
        {
          // 큐가 꽉 찬 상태 - 이 8샘플묶음은 이더넷으로 나가지 못하고 버려짐
          svPublishDropCnt++;
        }
      }
  }

  // ADC1/ADC2가 한 번만 돌고 멈추는 현상은 OVR(overrun) 등 에러가 발생한 뒤
  // 아무도 복구하지 않아서 멈춰있는 것일 가능성이 큼. 지금까지는 이 콜백이
  // 아예 구현되어 있지 않아서(HAL weak 함수로 비어있음) 에러가 나도 조용히
  // 무시되고 있었음. 우선 에러 카운트/코드를 남기고, DMA를 재시작해서
  // 자동 복구를 시도한다.
  void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *AdcHandle) {
      if (AdcHandle->Instance == ADC1) {
          adc1ErrCnt++;
          adc1LastErrorCode = AdcHandle->ErrorCode;
          __HAL_ADC_CLEAR_FLAG(AdcHandle, ADC_FLAG_OVR);
          HAL_ADC_Stop_DMA(AdcHandle);
          HAL_ADC_Start_DMA(AdcHandle, (uint32_t*)adc1Buf, 3*ASDU_NUM);
      }
      else if (AdcHandle->Instance == ADC2) {
          adc2ErrCnt++;
          adc2LastErrorCode = AdcHandle->ErrorCode;
          __HAL_ADC_CLEAR_FLAG(AdcHandle, ADC_FLAG_OVR);
          HAL_ADC_Stop_DMA(AdcHandle);
          HAL_ADC_Start_DMA(AdcHandle, (uint32_t*)adc2Buf, 3*ASDU_NUM);
      }
      else if (AdcHandle->Instance == ADC3) {
          adc3ErrCnt++;
          adc3LastErrorCode = AdcHandle->ErrorCode;
          __HAL_ADC_CLEAR_FLAG(AdcHandle, ADC_FLAG_OVR);
          HAL_ADC_Stop_DMA(AdcHandle);
          HAL_ADC_Start_DMA(AdcHandle, (uint32_t*)adc3Buf, 4*ASDU_NUM);
      }
  }

#endif

 #ifdef _USE_HW_CLI
 void cliADC(cli_args_t *args) {
     bool ret = false;

     if (args->argc == 1 && args->isStr(0, "stat") == true) {
 #ifdef _USE_ADC_TEST
         uint32_t okCnt   = svPublishOkCnt;
         uint32_t dropCnt = svPublishDropCnt;
         uint32_t total   = okCnt + dropCnt;
         float    dropPct = (total > 0) ? (100.0f * (float)dropCnt / (float)total) : 0.0f;

         cliPrintf("adc1CpltCnt=%u adc2CpltCnt=%u adc3CpltCnt=%u\n",
                   (unsigned int)adc1CpltCnt, (unsigned int)adc2CpltCnt, (unsigned int)adc3CpltCnt);
         cliPrintf("adc1ErrCnt=%u(0x%X) adc2ErrCnt=%u(0x%X) adc3ErrCnt=%u(0x%X)\n",
                   (unsigned int)adc1ErrCnt, (unsigned int)adc1LastErrorCode,
                   (unsigned int)adc2ErrCnt, (unsigned int)adc2LastErrorCode,
                   (unsigned int)adc3ErrCnt, (unsigned int)adc3LastErrorCode);
         cliPrintf("sv publish OK=%u DROP=%u (drop=%f %%)\n",
                   (unsigned int)okCnt, (unsigned int)dropCnt, dropPct);
 #else
         cliPrintf("_USE_ADC_TEST 비활성화 상태 - 카운터 없음\n");
 #endif
         ret = true;
     }

     if (ret != true) {
         cliPrintf("adc stat : 캘리브레이션/SV 발행 관련 카운터 출력\n");
     }
 }

 #endif
 
#endif
  
 
