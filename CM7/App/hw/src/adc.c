#include "adc.h"
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
  void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *AdcHandle) {
      if ( AdcHandle->Instance == ADC1){

      }
      else if ( AdcHandle->Instance == ADC2){

      }
      else if (AdcHandle->Instance == ADC3) {
        static AdcDataPacket packet;
        
        // 메모리 비캐시 영역 또는 하드웨어 동기화를 고려하여 데이터를 로컬 패킷에 복사
        memcpy(packet.adc1, (const void*)adc1Buf, sizeof(adc1Buf));
        memcpy(packet.adc2, (const void*)adc2Buf, sizeof(adc2Buf));
        memcpy(packet.adc3, (const void*)adc3Buf, sizeof(adc3Buf));
        
        // 데이터 태스크(IEC61850_SV_Task)로 전달
        osMessageQueuePut(svDataQueueHandle, &packet, 0, 0);    }
  }

#endif

 #ifdef _USE_HW_CLI
 void cliADC(cli_args_t *args) {
     bool ret = false;
     volatile uint32_t i;
     (void)i;
     if (ret != true) {
         cliPrintf("ADC ovt_run\n");

     }
 
 }
 
 #endif
 
#endif
  
 
