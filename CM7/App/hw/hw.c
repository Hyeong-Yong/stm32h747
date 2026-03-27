/*
 * hw.c
 *
 *  Created on: Dec 6, 2020
 *      Author: baram
 */


#include "hw.h"


void hwInit(void)
{

  ledInit();
  gpioInit();
  cliInit();
  uartInit();
  //uartOpen(_DEF_UART1, 115200);
  //flashInit();
  //qspiInit();
  //sdramInit();
  spiInit();
  spiBegin(_DEF_SPI1);
  adcInit();
  cdcInit();
  usbInit();
  timInit();
  usbBegin(USB_CDC_MODE);
  uartOpen(_DEF_UART2, 115200);


  logPrintf("\r\n[ Firmware Begin... ]\r\n");
  logPrintf("Booting..Clock\t: %d Mhz\r\n", (int)HAL_RCC_GetSysClockFreq()/1000000);
  logPrintf("\n");

}

void delay(uint32_t ms)
{
  HAL_Delay(ms);
}

uint32_t millis(void)
{
  return HAL_GetTick();
}
