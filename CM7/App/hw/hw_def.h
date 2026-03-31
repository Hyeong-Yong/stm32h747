/*
 * hw_def.h
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */

#ifndef E3B59A53_01D9_447A_A33C_91A0BE92EAD3
#define E3B59A53_01D9_447A_A33C_91A0BE92EAD3


#include "def.h"
#include "main.h"


#define _USE_HW_ADC
#define 	 HW_ADC_MAX_CH			1
#define         ADC_BUF_SIZE        2000    /* Size of array containing ADC converted values */

#define _USE_HW_OVTOCT

#define _USE_HW_LED
#define      HW_LED_MAX_CH          3


#define _USE_HW_USB
#define      HW_USB_MAX_CH          1
#define _USE_HW_CDC
#define      HW_USB_CDC             1           

#define _USE_HW_UART
#define      HW_UART_MAX_CH         2

#define _USE_HW_CLI
#define      HW_CLI_CMD_LIST_MAX    24
#define      HW_CLI_CMD_NAME_MAX    16
#define      HW_CLI_LINE_HIS_MAX    4
#define      HW_CLI_LINE_BUF_MAX    64

#define _USE_HW_GPIO
#define      HW_GPIO_MAX_CH         1

#define _USE_HW_FLASH
#define _USE_HW_QSPI

#define _USE_HW_SPI
#define      HW_SPI_MAX_CH			1

#define _USE_HW_DAC8562
#define _USE_HW_GALVANO
#define 	 HW_GALVANO_PKT_BUF_MAX 33


typedef enum
{
    DAC8562_SPI_SYNC,
} GpioPinName_t;


#define _USE_HW_SDRAM
#define      HW_SDRAM_MEM_ADDR      0xD0000000
#define      HW_SDRAM_MEM_SIZE      (32*1024*1024) // 32MB


#define logPrintf printf
void     delay(uint32_t ms);
uint32_t millis(void);


#endif /* E3B59A53_01D9_447A_A33C_91A0BE92EAD3 */
