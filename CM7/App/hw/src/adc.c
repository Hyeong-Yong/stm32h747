#include "adc.h"


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

 // single ADC : OVT, 16bit, sampling rate, oversampling.ratio = 128, 23bit
 // pin number : PA0_C
 // ADC clock : 40Mhz, ADC prescaler : 2
 // Sampling (conversion) cycles : 32.5 (8.5)
 // Sampling rate : (40*10^6/2)/(32.5+8.5)/128 = 3810.975
 // 나누기 2가 더 되어야하는데 왜 그럴까??

 // single ADC : OcT, 16bit, sampling rate, oversampling.ratio = 128 , "23bit (shift 7) => 16bit"
 // pin number : PA1_C
 // ADC clock : 40Mhz, ADC prescaler : 2
 // Sampling (conversion) cycles : 32.5 (8.5)
 // Sampling rate : (40*10^6/2)/(32.5+8.5)/128 = 3810.975
 // 나누기 2가 더 되어야하는데 왜 그럴까??


 // single ADC : adc3, 16bit, sampling rate, oversampling.ratio = 128 , "23bit (shift 7) => 16bit"
 // pin number : PC1
 // ADC clock : 40Mhz, ADC prescaler : 2
 // Sampling (conversion) cycles : 32.5 (8.5)
 // Sampling rate : (40*10^6/2)/(32.5+8.5)/128 = 3810.975
 // 나누기 2가 더 되어야하는데 왜 그럴까??



 #ifdef _USE_HW_CLI
 static void cliADC(cli_args_t *args);
 #endif
 


 /* ADC parameters */
 static bool is_init = false;

 //static __attribute__((section(".non_cache")))    ALIGN_32BYTES(__IO uint32_t adcBuf_OVT[ADC_BUF_SIZE]);



 __attribute__((section(".non_cache")))    __IO uint32_t adcBuf_OVT[ADC_BUF_SIZE];
 __attribute__((section(".non_cache")))    __IO uint16_t adcBuf_OCT[ADC_BUF_SIZE];
 __attribute__((section(".non_cache")))    __IO uint16_t adcBuf_3[160];



 extern ADC_HandleTypeDef hadc1;
 extern ADC_HandleTypeDef hadc2;
 extern ADC_HandleTypeDef hadc3;
 extern DMA_HandleTypeDef hdma_adc1;
 extern DMA_HandleTypeDef hdma_adc2;
 extern DMA_HandleTypeDef hdma_adc3;

 volatile bool adc_OVT_Finished = false;
 volatile bool adc_OCT_Finished = false;
 volatile bool adc_3_Finished = false;

 bool adcInit(void) {
     bool ret = true;
 
     if ( HAL_ADCEx_Calibration_Start(&hadc1, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK) {
         ret = false;
     }
     if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuf_OVT, ADC_BUF_SIZE) != HAL_OK) {
         ret = false;
     }
     delay(1);

     if ( HAL_ADCEx_Calibration_Start(&hadc2, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK) {
         ret = false;
     }
     if (HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adcBuf_OCT, ADC_BUF_SIZE) != HAL_OK) {
         ret = false;
     }
     delay(1);

     if ( HAL_ADCEx_Calibration_Start(&hadc3, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED) != HAL_OK) {
         ret = false;
     }
     if (HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adcBuf_3, ADC_BUF_SIZE) != HAL_OK) {
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
     if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuf_OVT, ADC_BUF_SIZE) != HAL_OK){
         ret = false;
         logPrintf("ADC_Start_DMA failed\n");
     }
 
     return ret;
 }

 
bool adcOctMeasure(void){
     bool ret = true;
     if (HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adcBuf_OCT, ADC_BUF_SIZE) != HAL_OK){
         ret = false;
         logPrintf("ADC_Start_DMA failed\n");
     }
 
     return ret;
 }
 


extern __IO uint16_t* activeAdcPtr;
extern osSemaphoreId_t signalSemHandle;
#define _USE_ADC_TEST
#ifdef _USE_ADC_TEST 

// 버퍼의 절반이 찼을 때 호출 (0 ~ 1023)
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* AdcHandle) {
    if (AdcHandle->Instance == ADC3) {
        activeAdcPtr = &adcBuf_3[0]; // 전반부 버퍼 준비 완료
        static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(signalSemHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

  void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *AdcHandle) {
      if ( AdcHandle->Instance == ADC1){
          adc_OVT_Finished = true;
      }
      else if ( AdcHandle->Instance == ADC2){
          adc_OCT_Finished = true;
      }    
      else if (AdcHandle->Instance == ADC3) {
        activeAdcPtr = &adcBuf_3[ADC_BUF_SIZE / 2]; // 후반부 버퍼 준비 완료
        static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(signalSemHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }



#endif

 #ifdef _USE_HW_CLI
 void cliADC(cli_args_t *args) {
     bool ret = false;
     volatile uint32_t i;
 
     if (args->argc == 1 && args->isStr(0, "ovt_run") == true) {
        HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuf_OVT, ADC_BUF_SIZE);
         while (cliKeepLoop()) {
             if (adc_OVT_Finished == true) {
                adc_OVT_Finished = false;
                 for (i = 0; i < ADC_BUF_SIZE; i++) {
                     cliPrintf("%4.2f [mV]\n", ((float)adcBuf_OVT[i]/8388607)*3.3); // LSB-> Master, 23bit => 8388607
                 }
                 cliPrintf("-------------------------\n");
                 HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adcBuf_OVT, ADC_BUF_SIZE);
             }
             delay(1500);
         }
         ret = true;
     }
 
     if (args->argc == 1 && args->isStr(0, "oct_run") == true) {
        HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adcBuf_OCT, ADC_BUF_SIZE);
         while (cliKeepLoop()) {
             if (adc_OCT_Finished == true) {
                adc_OCT_Finished = false;
                 for (i = 0; i < ADC_BUF_SIZE; i++) {
                     cliPrintf("%4.2f [mV]\n", ((float)adcBuf_OCT[i]/65535)*3.3); // LSB-> Master, 16bit => 65535
                 }
                 cliPrintf("-------------------------\n");
                 HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adcBuf_OCT, ADC_BUF_SIZE);
             }
             delay(1500);
         }
         ret = true;
     }

     if (args->argc == 1 && args->isStr(0, "adc_3") == true) {
        HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adcBuf_3, ADC_BUF_SIZE);
         while (cliKeepLoop()) {
             if (adc_3_Finished == true) {
                adc_3_Finished = false;
                 for (i = 0; i < ADC_BUF_SIZE; i++) {
                     cliPrintf("%4.2f [mV]\n", ((float)adcBuf_3[i]/65535)*3.3); // LSB-> Master, 16bit => 65535
                 }
                 cliPrintf("-------------------------\n");
                 HAL_ADC_Start_DMA(&hadc3, (uint32_t*)adcBuf_3, ADC_BUF_SIZE);
             }
             delay(1500);
         }
         ret = true;
     }

     if (ret != true) {
         cliPrintf("ADC ovt_run\n");
         cliPrintf("ADC oct_run\n");
         cliPrintf("ADC adc_3\n");

     }
 
 }
 
 #endif
 
#endif
  
 
